// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.content.ComponentName;
import android.content.Intent;
import android.net.Uri;
import android.provider.Browser;
import android.view.LayoutInflater;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.widget.LinearLayout;

import androidx.annotation.IdRes;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.CallbackUtils;
import org.chromium.base.IntentUtils;
import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.base.supplier.NullableObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.app.tabwindow.TabWindowManagerSingleton;
import org.chromium.chrome.browser.browserservices.intents.WebappConstants;
import org.chromium.chrome.browser.document.ChromeLauncherActivity;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.omnibox.BackKeyBehaviorDelegate;
import org.chromium.chrome.browser.omnibox.LocationBarEmbedder;
import org.chromium.chrome.browser.omnibox.suggestions.OmniboxLoadUrlParams;
import org.chromium.chrome.browser.omnibox.suggestions.action.OmniboxActionDelegateImpl;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.searchwidget.SearchActivityUtils;
import org.chromium.chrome.browser.searchwidget.SearchBoxDataProvider;
import org.chromium.chrome.browser.searchwidget.SearchUiCoordinator;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabwindow.TabWindowInfo;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.searchactivityutils.SearchActivityExtras.IntentOrigin;
import org.chromium.chrome.browser.ui.searchactivityutils.SearchActivityExtras.SearchType;
import org.chromium.components.metrics.OmniboxEventProtos.OmniboxEventProto.PageClassification;
import org.chromium.ui.AsyncViewStub;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.edge_to_edge.EdgeToEdgeSystemBarColorHelper;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.url.GURL;

/**
 * Coordinator that hosts SearchUiCoordinator in a floating Tab Search panel positioned overlaying
 * the Vertical Tabs rail.
 */
@NullMarked
public class TabSearchOverlayCoordinator {
    private final Activity mActivity;
    private final ViewGroup mParentContainer;
    private final WindowAndroid mWindowAndroid;
    private final MonotonicObservableSupplier<Profile> mProfileSupplier;
    private final SnackbarManager mSnackbarManager;
    private final NullableObservableSupplier<ModalDialogManager> mModalDialogManagerSupplier;
    private final ActivityLifecycleDispatcher mLifecycleDispatcher;
    private final MonotonicObservableSupplier<TabModelSelector> mTabModelSelectorSupplier;
    private final @Nullable EdgeToEdgeSystemBarColorHelper mEdgeToEdgeSystemBarColorHelper;
    private final PropertyModel mModel;

    private @Nullable
            PropertyModelChangeProcessor<
                    PropertyModel, TabSearchOverlayViewBinder.ViewHolder, PropertyKey>
            mChangeProcessor;
    private @Nullable LinearLayout mPanelContainer;
    private @Nullable SearchUiCoordinator mSearchUiCoordinator;
    private final SearchBoxDataProvider mSearchBoxDataProvider;

    /**
     * Constructs a new TabSearchOverlayCoordinator.
     *
     * @param activity The current Android Activity.
     * @param parentContainer The parent ViewGroup to attach the search overlay view to.
     * @param windowAndroid The window helper for managing window-level state.
     * @param profileSupplier Supplier for the current Profile.
     * @param snackbarManager Manager for showing snackbar notifications.
     * @param modalDialogManagerSupplier Supplier for the modal dialog manager.
     * @param lifecycleDispatcher Dispatcher for activity lifecycle events.
     * @param tabModelSelectorSupplier Supplier for the tab model selector.
     * @param edgeToEdgeSystemBarColorHelper Helper for managing system bar colors in edge-to-edge.
     */
    public TabSearchOverlayCoordinator(
            Activity activity,
            ViewGroup parentContainer,
            WindowAndroid windowAndroid,
            MonotonicObservableSupplier<Profile> profileSupplier,
            SnackbarManager snackbarManager,
            NullableObservableSupplier<ModalDialogManager> modalDialogManagerSupplier,
            ActivityLifecycleDispatcher lifecycleDispatcher,
            MonotonicObservableSupplier<TabModelSelector> tabModelSelectorSupplier,
            @Nullable EdgeToEdgeSystemBarColorHelper edgeToEdgeSystemBarColorHelper) {
        mActivity = activity;
        mParentContainer = parentContainer;
        mWindowAndroid = windowAndroid;
        mProfileSupplier = profileSupplier;
        mSnackbarManager = snackbarManager;
        mModalDialogManagerSupplier = modalDialogManagerSupplier;
        mLifecycleDispatcher = lifecycleDispatcher;
        mTabModelSelectorSupplier = tabModelSelectorSupplier;
        mEdgeToEdgeSystemBarColorHelper = edgeToEdgeSystemBarColorHelper;

        mModel = TabSearchOverlayProperties.createDefaultModel();
        mModel.set(TabSearchOverlayProperties.VISIBLE, false);
        mModel.set(TabSearchOverlayProperties.ON_SCRIM_CLICK, (v) -> hide());

        mSearchBoxDataProvider = new SearchBoxDataProvider();
        mSearchBoxDataProvider.setPageClassification(PageClassification.ANDROID_HUB_VALUE);
    }

    /** Destroys the coordinator, cleaning up resources and child coordinators. */
    public void destroy() {
        if (mChangeProcessor != null) {
            mChangeProcessor.destroy();
            mChangeProcessor = null;
        }
        if (mSearchUiCoordinator != null) {
            mSearchUiCoordinator.destroy();
            mSearchUiCoordinator = null;
        }
        mSearchBoxDataProvider.destroy();
        if (mPanelContainer != null) {
            mParentContainer.removeView(mPanelContainer);
            mPanelContainer = null;
        }
    }

    @SuppressLint("ClickableViewAccessibility")
    @VisibleForTesting
    void ensureInitialized() {
        if (mPanelContainer != null) return;

        mPanelContainer =
                (LinearLayout)
                        LayoutInflater.from(mActivity)
                                .inflate(
                                        R.layout.tab_search_overlay_layout,
                                        mParentContainer,
                                        false);
        final LinearLayout panelContainer = mPanelContainer;
        View scrim = panelContainer.findViewById(R.id.tab_search_overlay_scrim);
        View panelView = panelContainer.findViewById(R.id.tab_search_overlay_panel);
        View searchActivityView = panelContainer.findViewById(R.id.search_activity_container);
        mParentContainer.addView(panelContainer);

        // Consume all unhandled touch, hover, generic motion, and context click events to prevent
        // them from bleeding through to sibling views underneath the overlay (i.e. Vertical Tabs).
        // This makes the search box have focus the entire time the overlay panel is visible. If the
        // desire is to remove focus when clicking on empty space on the panel, the bleed through
        // bug will need to be addressed and input preservation logic added in LocationBarMediator.
        panelView.setOnTouchListener(this::consumeMotionEvent);
        panelView.setOnHoverListener(this::consumeMotionEvent);
        panelView.setOnGenericMotionListener(this::consumeMotionEvent);
        panelView.setOnContextClickListener(this::consumeContextClick);

        if (mSearchUiCoordinator == null) {
            boolean isIncognito =
                    mProfileSupplier.get() != null && mProfileSupplier.get().isOffTheRecord();
            mSearchBoxDataProvider.initialize(mActivity, isIncognito);
            mSearchUiCoordinator = new SearchUiCoordinator(mActivity, mSearchBoxDataProvider);
        }

        LocationBarEmbedder embedder =
                new LocationBarEmbedder() {
                    @Override
                    public @Nullable AsyncViewStub getSuggestionsContainerStub() {
                        return panelContainer.findViewById(
                                R.id.search_activity_suggestions_container_stub);
                    }

                    @Override
                    public @IdRes int getSuggestionsContainerInflatedViewId() {
                        return R.id.search_activity_suggestions_container;
                    }
                };

        BackKeyBehaviorDelegate backKeyBehaviorDelegate =
                new BackKeyBehaviorDelegate() {
                    @Override
                    public boolean handleBackKeyPressed() {
                        hide();
                        return true;
                    }
                };

        // Omnibox action suggestions (such as action chips or action buttons) are not supported
        // or used in Tab Search, so these action delegate callbacks are stubbed out.
        OmniboxActionDelegateImpl omniboxActionDelegate =
                new OmniboxActionDelegateImpl(
                        mActivity,
                        () -> {
                            TabModelSelector selector = mTabModelSelectorSupplier.get();
                            return selector != null ? selector.getCurrentTab() : null;
                        },
                        /* openUrlInExistingTabElseNewTabCb= */ CallbackUtils.emptyCallback()
                                ::onResult,
                        /* openIncognitoTabCb= */ CallbackUtils.emptyRunnable(),
                        /* openPasswordSettingsCb= */ CallbackUtils.emptyRunnable(),
                        /* openQuickDeleteCb= */ null,
                        TabWindowManagerSingleton::getInstance,
                        this::bringTabToFront);

        mSearchUiCoordinator.initialize(
                searchActivityView,
                panelView,
                mWindowAndroid,
                mProfileSupplier,
                mSnackbarManager,
                mModalDialogManagerSupplier,
                mLifecycleDispatcher,
                mTabModelSelectorSupplier,
                this::loadUrl,
                backKeyBehaviorDelegate,
                this::bringTabGroupToFront,
                omniboxActionDelegate,
                /* backPressManager= */ null,
                embedder,
                mEdgeToEdgeSystemBarColorHelper);
        mSearchUiCoordinator.setDefaultStatusIconOverrideResId(R.drawable.ic_suggestion_magnifier);

        TabSearchOverlayViewBinder.ViewHolder viewHolder =
                new TabSearchOverlayViewBinder.ViewHolder(panelContainer, scrim);
        mChangeProcessor =
                PropertyModelChangeProcessor.create(
                        mModel, viewHolder, TabSearchOverlayViewBinder::bind);
    }

    private boolean loadUrl(OmniboxLoadUrlParams params, boolean isIncognito) {
        if (params.url == null) return false;

        Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse(params.url));
        intent.setComponent(
                new ComponentName(mActivity.getApplicationContext(), ChromeLauncherActivity.class));

        // If an existing open tab already matches this query, reuse it.
        intent.putExtra(WebappConstants.REUSE_URL_MATCHING_TAB_ELSE_NEW_TAB, true);

        // Include support for incognito mode.
        intent.putExtra(IntentHandler.EXTRA_OPEN_NEW_INCOGNITO_TAB, isIncognito);
        if (isIncognito) {
            intent.putExtra(Browser.EXTRA_APPLICATION_ID, mActivity.getPackageName());
        }

        // Add trusted intent extras so Chrome trusts the incognito launch request
        IntentUtils.addTrustedIntentExtras(intent);
        IntentUtils.safeStartActivity(mActivity, intent);

        hide();
        return true;
    }

    private boolean consumeMotionEvent(View v, MotionEvent event) {
        return true;
    }

    private boolean consumeContextClick(View v) {
        return true;
    }

    private void bringTabToFront(TabWindowInfo tabWindowInfo, GURL url) {
        SearchActivityUtils.bringTabToFront(
                mActivity, mTabModelSelectorSupplier.get(), tabWindowInfo, url, this::hide);
    }

    private void bringTabGroupToFront(String tabGroupId) {
        IntentHandler.bringTabGroupToFront(tabGroupId);
        hide();
    }

    /**
     * Shows the tab search overlay. If the overlay has not been inflated and attached to the parent
     * container yet, this method will initialize it.
     */
    public void show() {
        ensureInitialized();
        if (mModel.get(TabSearchOverlayProperties.VISIBLE)) return;

        mModel.set(TabSearchOverlayProperties.VISIBLE, true);
        assumeNonNull(mSearchUiCoordinator)
                .beginQuery(IntentOrigin.HUB, SearchType.TEXT, /* query= */ null, mWindowAndroid);
    }

    /** Hides the tab search overlay and clears the focus from the search box. */
    public void hide() {
        mModel.set(TabSearchOverlayProperties.VISIBLE, false);
        if (mSearchUiCoordinator != null) {
            var locationBar = mSearchUiCoordinator.getLocationBarCoordinator();
            locationBar.clearOmniboxFocus();
        }
    }

    /** Returns whether the tab search overlay is currently visible. */
    public boolean isVisible() {
        return mModel.get(TabSearchOverlayProperties.VISIBLE);
    }

    @Nullable SearchUiCoordinator getSearchUiCoordinatorForTesting() {
        return mSearchUiCoordinator;
    }

    @Nullable LinearLayout getPanelContainerForTesting() {
        return mPanelContainer;
    }

    void setSearchUiCoordinatorForTesting(SearchUiCoordinator searchUiCoordinator) {
        mSearchUiCoordinator = searchUiCoordinator;
    }
}

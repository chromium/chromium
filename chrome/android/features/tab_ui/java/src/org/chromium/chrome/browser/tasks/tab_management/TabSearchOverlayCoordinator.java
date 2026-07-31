// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.content.ComponentName;
import android.content.Intent;
import android.graphics.Rect;
import android.net.Uri;
import android.os.Build;
import android.provider.Browser;
import android.view.LayoutInflater;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewConfiguration;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.annotation.DrawableRes;
import androidx.annotation.IdRes;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.CallbackUtils;
import org.chromium.base.IntentUtils;
import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.base.supplier.NonNullObservableSupplier;
import org.chromium.base.supplier.NullableObservableSupplier;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.base.supplier.SettableNonNullObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.app.tabwindow.TabWindowManagerSingleton;
import org.chromium.chrome.browser.back_press.BackPressManager;
import org.chromium.chrome.browser.browserservices.intents.WebappConstants;
import org.chromium.chrome.browser.compositor.CompositorViewHolder;
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
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncServiceFactory;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.browser.tabwindow.TabWindowInfo;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.searchactivityutils.SearchActivityExtras.IntentOrigin;
import org.chromium.chrome.browser.ui.searchactivityutils.SearchActivityExtras.SearchType;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler;
import org.chromium.components.metrics.OmniboxEventProtos.OmniboxEventProto.PageClassification;
import org.chromium.components.tab_group_sync.SavedTabGroup;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.components.tab_group_sync.TabGroupUiActionHandler;
import org.chromium.ui.AsyncViewStub;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.edge_to_edge.EdgeToEdgeSystemBarColorHelper;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.List;

/**
 * Coordinator that hosts SearchUiCoordinator in a floating Tab Search panel positioned overlaying
 * the Vertical Tabs rail.
 */
@NullMarked
public class TabSearchOverlayCoordinator implements BackPressHandler {
    private final Activity mActivity;
    private final ViewGroup mParentContainer;
    private final WindowAndroid mWindowAndroid;
    private final MonotonicObservableSupplier<Profile> mProfileSupplier;
    private final SnackbarManager mSnackbarManager;
    private final NullableObservableSupplier<ModalDialogManager> mModalDialogManagerSupplier;
    private final ActivityLifecycleDispatcher mLifecycleDispatcher;
    private final MonotonicObservableSupplier<TabModelSelector> mTabModelSelectorSupplier;
    private final @Nullable EdgeToEdgeSystemBarColorHelper mEdgeToEdgeSystemBarColorHelper;
    private final BackPressManager mBackPressManager;
    private final MonotonicObservableSupplier<CompositorViewHolder> mCompositorViewHolderSupplier;
    private final OneshotSupplier<TabGroupUiActionHandler> mTabGroupUiActionHandlerSupplier;
    // Recursion guard to prevent event dispatch loops when forwarding scrim scroll/drag events
    // to the underlying compositor view hierarchy.
    private boolean mIsForwardingScroll;
    private final SettableNonNullObservableSupplier<Boolean> mBackPressStateSupplier =
            ObservableSuppliers.createNonNull(false);
    private final PropertyModel mModel;
    private final SearchBoxDataProvider mSearchBoxDataProvider;
    private final Callback<Profile> mProfileObserver;

    private @Nullable
            PropertyModelChangeProcessor<
                    PropertyModel, TabSearchOverlayViewBinder.ViewHolder, PropertyKey>
            mChangeProcessor;
    private @Nullable LinearLayout mPanelContainer;
    private @Nullable SearchUiCoordinator mSearchUiCoordinator;
    private final Callback<Boolean> mSuggestionsObserver = this::onSuggestionsChanged;

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
     * @param backPressManager Manager for intercepting and handling system-level back presses.
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
            @Nullable EdgeToEdgeSystemBarColorHelper edgeToEdgeSystemBarColorHelper,
            BackPressManager backPressManager,
            MonotonicObservableSupplier<CompositorViewHolder> compositorViewHolderSupplier,
            OneshotSupplier<TabGroupUiActionHandler> tabGroupUiActionHandlerSupplier) {
        mActivity = activity;
        mParentContainer = parentContainer;
        mWindowAndroid = windowAndroid;
        mProfileSupplier = profileSupplier;
        mSnackbarManager = snackbarManager;
        mModalDialogManagerSupplier = modalDialogManagerSupplier;
        mLifecycleDispatcher = lifecycleDispatcher;
        mTabModelSelectorSupplier = tabModelSelectorSupplier;
        mEdgeToEdgeSystemBarColorHelper = edgeToEdgeSystemBarColorHelper;
        mBackPressManager = backPressManager;
        mCompositorViewHolderSupplier = compositorViewHolderSupplier;
        mTabGroupUiActionHandlerSupplier = tabGroupUiActionHandlerSupplier;
        mBackPressManager.addHandler(this, BackPressHandler.Type.TAB_SEARCH_OVERLAY);

        mModel = TabSearchOverlayProperties.createDefaultModel();
        mModel.set(TabSearchOverlayProperties.VISIBLE, false);
        mModel.set(TabSearchOverlayProperties.ON_SCRIM_CLICK, (v) -> hide());
        mModel.set(TabSearchOverlayProperties.ON_CLOSE_CLICK, (v) -> hide());

        mSearchBoxDataProvider = new SearchBoxDataProvider();
        mSearchBoxDataProvider.setPageClassification(
                PageClassification.ANDROID_TAB_SEARCH_OVERLAY_VALUE);

        mProfileObserver = this::onProfileChanged;
        mProfileSupplier.addSyncObserverAndCallIfNonNull(mProfileObserver);
    }

    /** Destroys the coordinator, cleaning up resources and child coordinators. */
    public void destroy() {
        mProfileSupplier.removeObserver(mProfileObserver);
        mBackPressManager.removeHandler(this);
        if (mChangeProcessor != null) {
            mChangeProcessor.destroy();
            mChangeProcessor = null;
        }
        if (mSearchUiCoordinator != null) {
            mSearchUiCoordinator
                    .getLocationBarCoordinator()
                    .getSuggestionsListNonEmptySupplier()
                    .removeObserver(mSuggestionsObserver);
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
        View panelView = panelContainer.findViewById(R.id.tab_search_overlay_panel);
        panelView.addOnLayoutChangeListener(
                (v, left, top, right, bottom, oldLeft, oldTop, oldRight, oldBottom) -> {
                    updateExclusionRects();
                });
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

        // Set up listeners on the scrim view to forward scroll and drag events to the
        // underlying web contents page, which is managed by the compositor view.
        View scrimView = panelContainer.findViewById(R.id.tab_search_overlay_scrim);
        float touchSlop = ViewConfiguration.get(mActivity).getScaledTouchSlop();
        // Forward standard touch screen gestures (e.g. scroll/drag) to the compositor.
        scrimView.setOnTouchListener(new ScrimTouchForwarder(touchSlop));
        // Forward mouse scroll wheel and trackpad scroll gestures to the compositor.
        scrimView.setOnGenericMotionListener(
                (v, event) -> {
                    if (event.getAction() == MotionEvent.ACTION_SCROLL) {
                        if (mIsForwardingScroll) {
                            return false;
                        }
                        return forwardEvent(v, event, false);
                    }
                    return false;
                });

        if (mSearchUiCoordinator == null) {
            mSearchUiCoordinator = new SearchUiCoordinator(mActivity, mSearchBoxDataProvider);
            mSearchUiCoordinator.getLocationBarUiOverrides().setVoiceEntrypointAllowed(false);
            mSearchUiCoordinator.getLocationBarUiOverrides().setLensEntrypointAllowed(false);
            mSearchUiCoordinator.getLocationBarUiOverrides().setEmbedderControlledHint(true);
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
                () -> {
                    hide();
                    return true;
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
        setSearchUiElements();

        mSearchUiCoordinator
                .getLocationBarCoordinator()
                .getSuggestionsListNonEmptySupplier()
                .addSyncObserver(mSuggestionsObserver);

        View emptyStateView = panelView.findViewById(R.id.empty_state_container);
        setupEmptyStateView(emptyStateView);

        TabSearchOverlayViewBinder.ViewHolder viewHolder =
                new TabSearchOverlayViewBinder.ViewHolder(
                        panelContainer, panelView, emptyStateView);
        mChangeProcessor =
                PropertyModelChangeProcessor.create(
                        mModel, viewHolder, TabSearchOverlayViewBinder::bind);
    }

    private void setSearchUiElements() {
        var searchUiCoordinator = assumeNonNull(mSearchUiCoordinator);
        searchUiCoordinator.setDefaultStatusIconOverrideResId(R.drawable.ic_suggestion_magnifier);

        // If the profile supplier is null (rare), default to the non-incognito state as it is the
        // safest choice in terms of incognito agnostic wording.
        boolean isIncognito =
                mProfileSupplier.get() != null && mProfileSupplier.get().isOffTheRecord();
        int hintTextRes =
                isIncognito
                        ? R.string.hub_search_empty_hint_incognito
                        : R.string.hub_search_empty_hint;
        searchUiCoordinator
                .getLocationBarCoordinator()
                .getUrlBarCoordinator()
                .setUrlBarHintText(mActivity.getResources().getString(hintTextRes));
    }

    private void setupEmptyStateView(View emptyStateView) {
        @DrawableRes int emptyImageResId = R.drawable.tablet_tab_switcher_empty_state_illustration;
        ImageView icon = emptyStateView.findViewById(R.id.empty_state_icon);
        icon.setImageResource(emptyImageResId);
        TextView title = emptyStateView.findViewById(R.id.empty_state_text_title);
        title.setText(R.string.search_in_settings_no_match);
        TextView description = emptyStateView.findViewById(R.id.empty_state_text_description);
        description.setVisibility(View.GONE);
    }

    private void onSuggestionsChanged(boolean hasSuggestions) {
        String query =
                assumeNonNull(mSearchUiCoordinator)
                        .getLocationBarCoordinator()
                        .getUrlBarCoordinator()
                        .getTextWithoutAutocomplete();
        boolean showEmptyState = query != null && !query.isEmpty() && !hasSuggestions;
        mModel.set(TabSearchOverlayProperties.EMPTY_STATE_VISIBLE, showEmptyState);
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
        Profile profile = mProfileSupplier.get();
        if (profile == null) return;

        TabGroupSyncService syncService = TabGroupSyncServiceFactory.getForProfile(profile);
        if (syncService == null) return;

        SavedTabGroup syncGroup = syncService.getGroup(tabGroupId);
        if (syncGroup == null || syncGroup.syncId == null) return;

        // If the group is not yet open locally, materialize/open it first.
        if (syncGroup.localId == null) {
            TabGroupUiActionHandler handler = mTabGroupUiActionHandlerSupplier.get();
            if (handler == null) return;

            handler.openTabGroup(syncGroup.syncId);
            // Re-fetch the synced group to obtain the newly mapped local group ID.
            syncGroup = syncService.getGroup(tabGroupId);
            if (syncGroup == null || syncGroup.localId == null) return;
        }

        TabModelSelector selector = mTabModelSelectorSupplier.get();
        if (selector == null) return;

        // Find the last active tab ID inside this local group (or the first tab if last active
        // cannot be determined).
        TabModel model = selector.getModel(mSearchBoxDataProvider.isIncognito());
        int tabId = model.getGroupLastShownTabId(syncGroup.localId.tabGroupId);
        if (tabId == Tab.INVALID_TAB_ID) return;

        // Select the active tab inside the local TabModel to focus on the group.
        int index = TabModelUtils.getTabIndexById(model, tabId);
        if (index != TabModel.INVALID_TAB_INDEX) {
            model.setIndex(index, TabSelectionType.FROM_USER);
        }

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
        mBackPressStateSupplier.set(true);
        assumeNonNull(mSearchUiCoordinator)
                .beginQuery(IntentOrigin.HUB, SearchType.TEXT, /* query= */ null, mWindowAndroid);
        updateExclusionRects();
    }

    /** Hides the tab search overlay and clears the focus from the search box. */
    public void hide() {
        mModel.set(TabSearchOverlayProperties.VISIBLE, false);
        mBackPressStateSupplier.set(false);
        if (mSearchUiCoordinator != null) {
            var locationBar = mSearchUiCoordinator.getLocationBarCoordinator();
            locationBar.clearOmniboxFocus();
        }
        updateExclusionRects();
    }

    /** Returns whether the tab search overlay is currently visible. */
    public boolean isVisible() {
        return mModel.get(TabSearchOverlayProperties.VISIBLE);
    }

    /**
     * Updates the system gesture exclusion rects for the overlay panel.
     *
     * <p>#setSystemGestureExclusionRects allows Chrome to receive touch events on the header area
     * of the panel when it is drawn under the system gesture area so that it remains accessible.
     * The exclusion rect gets removed when the panel is no longer visible.
     */
    private void updateExclusionRects() {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.Q || mPanelContainer == null) {
            return;
        }

        View panelView = mPanelContainer.findViewById(R.id.tab_search_overlay_panel);
        if (panelView == null) return;

        List<Rect> rects = new ArrayList<>();
        if (mModel.get(TabSearchOverlayProperties.VISIBLE)) {
            // Exclude the close button's interactive area to ensure it remains clickable even
            // when under the system gesture area.
            View closeButton = panelView.findViewById(R.id.tab_search_close_button);
            if (closeButton != null && closeButton.getWidth() > 0) {
                Rect rect = new Rect();
                rect.left = closeButton.getLeft();
                rect.top = closeButton.getTop();
                rect.right = closeButton.getRight();
                rect.bottom = closeButton.getBottom();
                rects.add(rect);
            }
        }
        panelView.setSystemGestureExclusionRects(rects);
    }

    /**
     * Called when the current {@link Profile} changes. Primarily intended to handle switching
     * between incognito and non-incognito modes for the current profile. Since this coordinator is
     * not torn down between context switches, any profile-dependent changes must be added here.
     */
    private void onProfileChanged(Profile profile) {
        // If the profile supplier is null (rare), default to the non-incognito state as it is the
        // safest choice for coloring since some features still do not support incognito colors.
        boolean isIncognito = profile != null && profile.isOffTheRecord();
        mSearchBoxDataProvider.initialize(mActivity, isIncognito);
        if (mSearchUiCoordinator != null) {
            mSearchUiCoordinator.setColorScheme(isIncognito);
        }
    }

    // BackPressHandler implementation.

    @Override
    public @BackPressResult int handleBackPress() {
        hide();
        return BackPressResult.SUCCESS;
    }

    @Override
    public NonNullObservableSupplier<Boolean> getHandleBackPressChangedSupplier() {
        return mBackPressStateSupplier;
    }

    /**
     * Translates coordinates and forwards a MotionEvent to the underlying CompositorViewHolder.
     *
     * @param v The view that received the event (scrim).
     * @param event The original MotionEvent.
     * @param isTouchEvent True if this is a standard touch event, false if generic motion (scroll).
     * @return True if the event was handled by the compositor, false otherwise.
     */
    private boolean forwardEvent(View v, MotionEvent event, boolean isTouchEvent) {
        CompositorViewHolder compositorViewHolder = mCompositorViewHolderSupplier.get();
        if (compositorViewHolder == null) {
            return false;
        }

        // Get window locations of both the scrim and the compositor views to compute coordinate
        // translation offset.
        int[] scrimLocation = new int[2];
        v.getLocationInWindow(scrimLocation);
        int[] targetLocation = new int[2];
        compositorViewHolder.getLocationInWindow(targetLocation);

        // Copy and offset the event coordinates to match the CompositorViewHolder's coordinate
        // space.
        MotionEvent offsetEvent = MotionEvent.obtain(event);
        offsetEvent.offsetLocation(
                scrimLocation[0] - targetLocation[0], scrimLocation[1] - targetLocation[1]);

        // Dispatch the translated event using a recursion guard to prevent dispatch loops.
        mIsForwardingScroll = true;
        boolean handled =
                isTouchEvent
                        ? compositorViewHolder.dispatchTouchEvent(offsetEvent)
                        : compositorViewHolder.dispatchGenericMotionEvent(offsetEvent);
        mIsForwardingScroll = false;
        offsetEvent.recycle();
        return handled;
    }

    /**
     * Touch listener for the overlay scrim that forwards drag/scroll events to the underlying
     * compositor webpage view, while preserving standard tap-to-dismiss clicks on the scrim. This
     * listener primarily handles touchscreen gestures (finger dragging and tapping).
     */
    private class ScrimTouchForwarder implements View.OnTouchListener {
        private final float mTouchSlop;
        private float mTouchDownX;
        private float mTouchDownY;
        private boolean mIsDragging;
        // Stores a copy of the ACTION_DOWN event. Deferring the dispatch of ACTION_DOWN to the
        // background view avoids clearing search box focus while the tap gesture is in-flight. If
        // focus is cleared immediately (on touch down), the suggestions list hides or the keyboard
        // is dismissed, triggering a layout pass or window resize layout shift. This layout shift
        // causes Android to send an ACTION_CANCEL to the scrim, swallowing the subsequent ACTION_UP
        // and preventing the scrim click (panel dismissal) from triggering. If the gesture is
        // confirmed as a drag, we replay this saved event before forwarding MOVE events so the
        // background compositor receives a complete gesture stream.
        private @Nullable MotionEvent mPendingDownEvent;

        /**
         * Constructs a new ScrimTouchForwarder.
         *
         * @param touchSlop The minimum distance a touch gesture must travel to be considered drag.
         */
        public ScrimTouchForwarder(float touchSlop) {
            mTouchSlop = touchSlop;
        }

        /**
         * Intercepts and processes touchscreen motion events on the scrim view.
         *
         * @param v The view that received the event (scrim).
         * @param event The touch event being processed.
         * @return True if the event was handled (consumed), false otherwise.
         */
        @Override
        public boolean onTouch(View v, MotionEvent event) {
            // Recursion guard to ignore events dispatched back from the compositor hierarchy.
            if (mIsForwardingScroll) {
                return false;
            }

            int action = event.getActionMasked();

            // Start of gesture: record initial down location and save a copy of the down event.
            // Defer forwarding to prevent focus loss and layout shifts (e.g. from hiding
            // suggestions or keyboard) from cancelling the click gesture on simple taps.
            if (action == MotionEvent.ACTION_DOWN) {
                mTouchDownX = event.getX();
                mTouchDownY = event.getY();
                mIsDragging = false;
                clearPendingDownEvent();
                mPendingDownEvent = MotionEvent.obtain(event);
                return true;
            }

            // Gesture movement: check if distance traveled exceeds the touch-slop threshold.
            // If it does, classify the gesture as a drag and forward the move events.
            if (action == MotionEvent.ACTION_MOVE) {
                if (!mIsDragging) {
                    float dx = event.getX() - mTouchDownX;
                    float dy = event.getY() - mTouchDownY;
                    if (Math.hypot(dx, dy) > mTouchSlop) {
                        mIsDragging = true;
                        // Drag confirmed: replay the saved DOWN event first so the compositor view
                        // starts the touch gesture correctly before receiving movement.
                        if (mPendingDownEvent != null) {
                            forwardEvent(v, mPendingDownEvent, true);
                            clearPendingDownEvent();
                        }
                    }
                }
                if (mIsDragging) {
                    forwardEvent(v, event, true);
                }
                return true;
            }

            // End of gesture:
            if (action == MotionEvent.ACTION_UP) {
                if (mIsDragging) {
                    // If dragging/scrolling, forward the final UP event to finish the scroll.
                    forwardEvent(v, event, true);
                } else {
                    // Tap detected: Since we never forwarded the DOWN event, we do not need to send
                    // a CANCEL event to the compositor. Recycle the pending event.
                    clearPendingDownEvent();

                    // Call performClick to natively invoke the registered onClickListener (which
                    // dismisses/hides the overlay) and notify accessibility services of the tap.
                    v.performClick();
                }
                mIsDragging = false;
                return true;
            }

            // Cancel event: Forward cancel events directly to keep background view state in
            // sync if we were already dragging, otherwise discard the pending down event.
            if (action == MotionEvent.ACTION_CANCEL) {
                if (mIsDragging) {
                    forwardEvent(v, event, true);
                }
                clearPendingDownEvent();
                mIsDragging = false;
                return true;
            }

            return false;
        }

        /**
         * Recycles the cached {@link #mPendingDownEvent} and clears the reference.
         *
         * <p>MotionEvents in Android are pooled by the system. If we copy an event using {@link
         * MotionEvent#obtain(MotionEvent)}, we must release it back to the pool by calling {@link
         * MotionEvent#recycle()} when done to prevent memory leaks. This helper ensures the cached
         * event is safely recycled and nullified when the gesture ends (via UP or CANCEL), when a
         * new touch sequence begins (via DOWN), or when the deferred DOWN event is successfully
         * forwarded.
         */
        private void clearPendingDownEvent() {
            if (mPendingDownEvent != null) {
                mPendingDownEvent.recycle();
                mPendingDownEvent = null;
            }
        }
    }

    // Testing methods.

    @Nullable SearchUiCoordinator getSearchUiCoordinatorForTesting() {
        return mSearchUiCoordinator;
    }

    @Nullable LinearLayout getPanelContainerForTesting() {
        return mPanelContainer;
    }

    PropertyModel getModelForTesting() {
        return mModel;
    }

    void setSearchUiCoordinatorForTesting(SearchUiCoordinator searchUiCoordinator) {
        mSearchUiCoordinator = searchUiCoordinator;
    }
}

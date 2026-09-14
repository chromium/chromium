// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.content.ComponentName;
import android.content.Intent;
import android.content.res.Configuration;
import android.graphics.Color;
import android.graphics.Rect;
import android.graphics.drawable.ColorDrawable;
import android.net.Uri;
import android.os.Build;
import android.provider.Browser;
import android.view.Gravity;
import android.view.LayoutInflater;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewConfiguration;
import android.view.ViewGroup;
import android.view.ViewTreeObserver;
import android.view.WindowManager;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.PopupWindow;
import android.widget.TextView;

import androidx.annotation.DrawableRes;
import androidx.annotation.IdRes;
import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.CallbackUtils;
import org.chromium.base.IntentUtils;
import org.chromium.base.metrics.RecordHistogram;
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
import org.chromium.chrome.browser.lifecycle.ConfigurationChangedObserver;
import org.chromium.chrome.browser.omnibox.BackKeyBehaviorDelegate;
import org.chromium.chrome.browser.omnibox.LocationBarEmbedder;
import org.chromium.chrome.browser.omnibox.UrlBar;
import org.chromium.chrome.browser.omnibox.fusebox.FuseboxControls;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteCoordinator;
import org.chromium.chrome.browser.omnibox.suggestions.OmniboxLoadUrlParams;
import org.chromium.chrome.browser.omnibox.suggestions.action.OmniboxActionDelegateImpl;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.searchwidget.SearchActivityUtils;
import org.chromium.chrome.browser.searchwidget.SearchBoxDataProvider;
import org.chromium.chrome.browser.searchwidget.SearchUiCoordinator;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObscuringHandler;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncServiceFactory;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.browser.tabwindow.TabWindowInfo;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.searchactivityutils.SearchActivityExtras.IntentOrigin;
import org.chromium.chrome.browser.ui.searchactivityutils.SearchActivityExtras.SearchType;
import org.chromium.components.browser_ui.desktop_windowing.AppHeaderState;
import org.chromium.components.browser_ui.desktop_windowing.DesktopWindowStateManager;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler;
import org.chromium.components.metrics.OmniboxEventProtosIntDef.PageClassification;
import org.chromium.components.tab_group_sync.SavedTabGroup;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.components.tab_group_sync.TabGroupUiActionHandler;
import org.chromium.ui.AsyncViewStub;
import org.chromium.ui.base.KeyNavigationUtil;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.edge_to_edge.EdgeToEdgeSystemBarColorHelper;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.url.GURL;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.List;

/**
 * Coordinator that hosts SearchUiCoordinator in a floating Tab Search panel positioned overlaying
 * the Vertical Tabs rail.
 */
@NullMarked
public class TabSearchOverlayCoordinator
        implements BackPressHandler,
                DesktopWindowStateManager.AppHeaderObserver,
                ConfigurationChangedObserver {
    // LINT.IfChange(TabSearchEntryPoint)
    @IntDef({
        TabSearchEntryPoint.HORIZONTAL_TAB_STRIP,
        TabSearchEntryPoint.VERTICAL_TABS,
        TabSearchEntryPoint.KEYBOARD_SHORTCUT
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface TabSearchEntryPoint {
        int HORIZONTAL_TAB_STRIP = 0;
        int VERTICAL_TABS = 1;
        int KEYBOARD_SHORTCUT = 2;
        int NUM_ENTRIES = 3;
    }

    // LINT.ThenChange(//tools/metrics/histograms/metadata/android/enums.xml:TabSearchEntryPoint)

    // LINT.IfChange(TabSearchDismissalReason)
    @IntDef({
        TabSearchDismissalReason.CLOSE_BUTTON,
        TabSearchDismissalReason.SCRIM,
        TabSearchDismissalReason.BACK_PRESS,
        TabSearchDismissalReason.WINDOW_FOCUS_LOST,
        TabSearchDismissalReason.TAB_SELECTED,
        TabSearchDismissalReason.TAB_GROUP_SELECTED,
        TabSearchDismissalReason.URL_LOADED,
        TabSearchDismissalReason.WINDOW_RESIZED
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface TabSearchDismissalReason {
        int CLOSE_BUTTON = 0;
        int SCRIM = 1;
        int BACK_PRESS = 2;
        int WINDOW_FOCUS_LOST = 3;
        int TAB_SELECTED = 4;
        int TAB_GROUP_SELECTED = 5;
        int URL_LOADED = 6;
        int WINDOW_RESIZED = 7;
        int NUM_ENTRIES = 8;
    }

    // LINT.ThenChange(//tools/metrics/histograms/metadata/android/enums.xml:TabSearchDismissalReason)

    private final Activity mActivity;
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
    private final @Nullable DesktopWindowStateManager mDesktopWindowStateManager;
    private final TabObscuringHandler mTabObscuringHandler;
    private final SettableNonNullObservableSupplier<Boolean> mBackPressStateSupplier =
            ObservableSuppliers.createNonNull(false);
    private final PropertyModel mModel;
    private final SearchBoxDataProvider mSearchBoxDataProvider;
    private final Callback<Profile> mProfileObserver;
    private final Callback<Boolean> mSuggestionsObserver = this::onSuggestionsChanged;

    // Recursion guard to prevent event dispatch loops when forwarding scrim scroll/drag events
    // to the underlying compositor view hierarchy.
    private boolean mIsForwardingScroll;
    private @Nullable
            PropertyModelChangeProcessor<
                    PropertyModel, TabSearchOverlayViewBinder.ViewHolder, PropertyKey>
            mChangeProcessor;
    private @Nullable LinearLayout mPanelContainer;
    private @Nullable PopupWindow mPopupWindow;
    // On desktop / tablet devices with a hardware keyboard connected, loading the NTP causes the
    // Omnibox to enter a STANDBY input session. When Tab Search is opened, this interface
    // terminates
    // any active Omnibox/Fusebox session so background suggestions are not triggered while Tab
    // Search is active.
    private final @Nullable FuseboxControls mFuseboxControls;
    private @Nullable SearchUiCoordinator mSearchUiCoordinator;
    private ViewTreeObserver.@Nullable OnWindowFocusChangeListener mWindowFocusListener;
    private TabObscuringHandler.@Nullable Token mTabObscuringToken;
    private boolean mEncounteredEmptyStateThisSession;

    /**
     * Constructs a new TabSearchOverlayCoordinator.
     *
     * @param activity The current Android Activity.
     * @param windowAndroid The window helper for managing window-level state.
     * @param profileSupplier Supplier for the current Profile.
     * @param snackbarManager Manager for showing snackbar notifications.
     * @param modalDialogManagerSupplier Supplier for the modal dialog manager.
     * @param lifecycleDispatcher Dispatcher for activity lifecycle events.
     * @param tabModelSelectorSupplier Supplier for the tab model selector.
     * @param edgeToEdgeSystemBarColorHelper Helper for managing system bar colors in edge-to-edge.
     * @param backPressManager Manager for intercepting and handling system-level back presses.
     * @param compositorViewHolderSupplier Supplier for the compositor view holder.
     * @param tabGroupUiActionHandlerSupplier Supplier for the tab group UI action handler.
     * @param desktopWindowStateManager Manager for monitoring desktop windowing state changes.
     * @param tabObscuringHandler Delegate object handling obscuring views.
     * @param fuseboxControls Optional interface to control Omnibox fusebox input sessions before
     *     showing the overlay.
     */
    public TabSearchOverlayCoordinator(
            Activity activity,
            WindowAndroid windowAndroid,
            MonotonicObservableSupplier<Profile> profileSupplier,
            SnackbarManager snackbarManager,
            NullableObservableSupplier<ModalDialogManager> modalDialogManagerSupplier,
            ActivityLifecycleDispatcher lifecycleDispatcher,
            MonotonicObservableSupplier<TabModelSelector> tabModelSelectorSupplier,
            @Nullable EdgeToEdgeSystemBarColorHelper edgeToEdgeSystemBarColorHelper,
            BackPressManager backPressManager,
            MonotonicObservableSupplier<CompositorViewHolder> compositorViewHolderSupplier,
            OneshotSupplier<TabGroupUiActionHandler> tabGroupUiActionHandlerSupplier,
            @Nullable DesktopWindowStateManager desktopWindowStateManager,
            TabObscuringHandler tabObscuringHandler,
            @Nullable FuseboxControls fuseboxControls) {
        mActivity = activity;
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
        mDesktopWindowStateManager = desktopWindowStateManager;
        mTabObscuringHandler = tabObscuringHandler;
        mFuseboxControls = fuseboxControls;
        mBackPressManager.addHandler(this, BackPressHandler.Type.TAB_SEARCH_OVERLAY);
        mLifecycleDispatcher.register(this);

        if (mDesktopWindowStateManager != null) {
            mDesktopWindowStateManager.addObserver(this);
        }

        mModel = TabSearchOverlayProperties.createDefaultModel();
        mModel.set(TabSearchOverlayProperties.VISIBLE, false);
        mModel.set(
                TabSearchOverlayProperties.ON_SCRIM_CLICK,
                (_) -> hide(TabSearchDismissalReason.SCRIM));
        mModel.set(
                TabSearchOverlayProperties.ON_CLOSE_CLICK,
                (_) -> hide(TabSearchDismissalReason.CLOSE_BUTTON));
        mModel.set(TabSearchOverlayProperties.ON_HIDE_FINISHED, this::onHideFinished);

        mSearchBoxDataProvider = new SearchBoxDataProvider();
        mSearchBoxDataProvider.setPageClassification(PageClassification.ANDROID_TAB_SEARCH_OVERLAY);

        mProfileObserver = this::onProfileChanged;
        mProfileSupplier.addSyncObserverAndCallIfNonNull(mProfileObserver);
    }

    /** Destroys the coordinator, cleaning up resources and child coordinators. */
    public void destroy() {
        if (mTabObscuringToken != null) {
            mTabObscuringHandler.unobscure(mTabObscuringToken);
            mTabObscuringToken = null;
        }
        if (mDesktopWindowStateManager != null) {
            mDesktopWindowStateManager.removeObserver(this);
        }
        mLifecycleDispatcher.unregister(this);
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
        if (mPopupWindow != null) {
            if (mPopupWindow.isShowing()) {
                mModel.set(TabSearchOverlayProperties.VISIBLE, false);
            }
            mPopupWindow.dismiss();
            mPopupWindow = null;
        }
        mPanelContainer = null;
    }

    @SuppressLint("ClickableViewAccessibility")
    @VisibleForTesting
    void ensureInitialized() {
        if (mPanelContainer != null) return;

        mPanelContainer =
                (LinearLayout)
                        LayoutInflater.from(mActivity)
                                .inflate(R.layout.tab_search_overlay_layout, null, false);
        final LinearLayout panelContainer = mPanelContainer;
        View panelView = panelContainer.findViewById(R.id.tab_search_overlay_panel);
        panelView.addOnLayoutChangeListener((_, _, _, _, _, _, _, _, _) -> updateExclusionRects());
        View searchActivityView = panelContainer.findViewById(R.id.search_activity_container);

        // Set up the overlay container within a standalone PopupWindow anchored to the DecorView.
        setupPopupWindow(panelContainer);
        setupWindowFocusListener(panelContainer);

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
                    // Consume non-scroll generic motion events (such as pointer clicks or hover)
                    // to prevent them from falling through to the background web contents and
                    // stealing focus from the UrlBar, which would cause the suggestions dropdown to
                    // dismiss on focus loss and cause animation flicker during panel hide.
                    return true;
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
                    hide(TabSearchDismissalReason.BACK_PRESS);
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
        updatePanelTopMargin();
    }

    /**
     * Configures the {@link PopupWindow} used to host the Tab Search overlay.
     *
     * <p>The overlay is displayed within a standalone {@link PopupWindow} anchored to the
     * Activity's DecorView rather than directly within the root view hierarchy. This provides
     * several key benefits:
     *
     * <ul>
     *   <li>Enables the overlay to extend across the entire window area (including into the app
     *       header / caption bar area in desktop windowing environments) without being constrained
     *       by layout boundaries or parent clipping.
     *   <li>Isolates the overlay window hierarchy, eliminating hover and pointer event
     *       bleed-through to underlying views (such as toolbar buttons or tab strip elements).
     *   <li>Naturally captures and consumes touch and click events across the full window,
     *       preventing unwanted interaction with views positioned beneath the overlay.
     *   <li>Avoids focus and Z-index collisions with underlying views in the main Activity
     *       hierarchy.
     * </ul>
     *
     * @param contentView The root view to set as the popup's content view.
     */
    private void setupPopupWindow(View contentView) {
        mPopupWindow =
                new PopupWindow(mActivity) {
                    @Override
                    public void dismiss() {
                        // When the popup is visible, dismiss() is only invoked by the Android
                        // framework when a system Back press or Escape key is received by the
                        // PopupDecorView. All other dismissal paths call hide() directly with their
                        // specific dismissal reasons and invoke dismiss() once the hide animation
                        // completes (at which point isVisible() is false).
                        if (isVisible()) {
                            hide(TabSearchDismissalReason.BACK_PRESS);
                        } else {
                            super.dismiss();
                        }
                    }
                };
        mPopupWindow.setContentView(contentView);
        mPopupWindow.setWidth(ViewGroup.LayoutParams.MATCH_PARENT);
        mPopupWindow.setHeight(ViewGroup.LayoutParams.MATCH_PARENT);
        mPopupWindow.setFocusable(true);
        mPopupWindow.setOutsideTouchable(true);
        mPopupWindow.setClippingEnabled(false);
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP_MR1) {
            mPopupWindow.setAttachedInDecor(true);
        }
        mPopupWindow.setInputMethodMode(PopupWindow.INPUT_METHOD_NEEDED);
        mPopupWindow.setSoftInputMode(WindowManager.LayoutParams.SOFT_INPUT_ADJUST_RESIZE);
        mPopupWindow.setBackgroundDrawable(new ColorDrawable(Color.TRANSPARENT));
    }

    private void setupWindowFocusListener(LinearLayout panelContainer) {
        // Dismiss the tab search panel when the window loses focus (e.g. on Alt-Tab).
        mWindowFocusListener =
                (hasFocus) -> {
                    if (!hasFocus) {
                        hideIfVisible(TabSearchDismissalReason.WINDOW_FOCUS_LOST);
                    }
                };

        panelContainer.addOnAttachStateChangeListener(
                new View.OnAttachStateChangeListener() {
                    @Override
                    public void onViewAttachedToWindow(View v) {
                        panelContainer
                                .getViewTreeObserver()
                                .addOnWindowFocusChangeListener(mWindowFocusListener);
                    }

                    @Override
                    public void onViewDetachedFromWindow(View v) {
                        panelContainer
                                .getViewTreeObserver()
                                .removeOnWindowFocusChangeListener(mWindowFocusListener);
                    }
                });
    }

    private void setSearchUiElements() {
        var searchUiCoordinator = assumeNonNull(mSearchUiCoordinator);
        searchUiCoordinator.setDefaultStatusIconOverrideResId(R.drawable.ic_suggestion_magnifier);

        // Shrink the size of the UrlBar text from the default (16sp) to medium (14sp) so the hint
        // text fits within the given viewport given the tab search overlay has a fixed small width.
        var locationBarCoordinator = searchUiCoordinator.getLocationBarCoordinator();
        var urlBar = (UrlBar) locationBarCoordinator.getContainerView().findViewById(R.id.url_bar);
        if (urlBar != null) {
            urlBar.setTextAppearance(R.style.TextAppearance_TextMedium);
            urlBar.setAccessibilityTraversalAfter(R.id.tab_search_close_button);

            LinearLayout panelContainer = assumeNonNull(mPanelContainer);
            View closeButton =
                    assumeNonNull(panelContainer.findViewById(R.id.tab_search_close_button));

            var suggestionsVisualState = locationBarCoordinator.getOmniboxSuggestionsVisualState();
            AutocompleteCoordinator autocompleteCoordinator =
                    suggestionsVisualState instanceof AutocompleteCoordinator coordinator
                            ? coordinator
                            : null;
            View.OnKeyListener omniboxKeyDownListener =
                    locationBarCoordinator.getOmniboxStub() instanceof View.OnKeyListener listener
                            ? listener
                            : null;

            View.OnKeyListener keyListener =
                    (v, keyCode, event) -> {
                        boolean isTabBack = KeyNavigationUtil.isTabBackward(event);
                        boolean isUp = KeyNavigationUtil.isGoUp(event) && event.hasNoModifiers();
                        Integer selectedIndex =
                                autocompleteCoordinator != null
                                        ? autocompleteCoordinator.getSelectedIndex()
                                        : null;

                        if (isTabBack || isUp) {
                            if (selectedIndex == null) {
                                closeButton.setFocusableInTouchMode(true);
                                closeButton.requestFocus();
                                return true;
                            } else if (selectedIndex <= 0) {
                                if (autocompleteCoordinator != null) {
                                    autocompleteCoordinator.resetSelection();
                                }
                                return true;
                            }
                        }

                        return omniboxKeyDownListener != null
                                && omniboxKeyDownListener.onKey(v, keyCode, event);
                    };
            urlBar.setKeyDownListener(keyListener);
            urlBar.setOnKeyListener(keyListener);

            closeButton.setOnFocusChangeListener(
                    (v, hasFocus) -> {
                        if (!hasFocus) {
                            closeButton.setFocusableInTouchMode(false);
                        }
                    });
            closeButton.setOnKeyListener(
                    (v, keyCode, event) -> {
                        if (KeyNavigationUtil.isTabForward(event)
                                || KeyNavigationUtil.isGoDown(event)) {
                            urlBar.requestFocus();
                            return true;
                        }
                        return false;
                    });
        }

        // If the profile supplier is null (rare), default to the non-incognito state as it is the
        // safest choice in terms of incognito agnostic wording.
        boolean isIncognito =
                mProfileSupplier.get() != null && mProfileSupplier.get().isOffTheRecord();
        int hintTextRes =
                isIncognito
                        ? R.string.hub_search_empty_hint_incognito
                        : R.string.hub_search_empty_hint;
        locationBarCoordinator
                .getUrlBarCoordinator()
                .setUrlBarHintText(mActivity.getResources().getString(hintTextRes));
    }

    private void setupEmptyStateView(View emptyStateView) {
        @DrawableRes int emptyImageResId = R.drawable.tab_search_empty_state;
        ImageView icon = emptyStateView.findViewById(R.id.empty_state_icon);
        icon.setImageResource(emptyImageResId);
        TextView title = emptyStateView.findViewById(R.id.empty_state_text_title);
        title.setTextAppearance(R.style.TextAppearance_TextLarge_Secondary);
        title.setText(R.string.search_in_settings_no_match);
        TextView description = emptyStateView.findViewById(R.id.empty_state_text_description);
        description.setVisibility(View.GONE);
    }

    private void onSuggestionsChanged(boolean hasSuggestions) {
        // Guard against empty-state updates from input clearing during the hide animation.
        if (!isVisible()) return;

        var locationBar = assumeNonNull(mSearchUiCoordinator).getLocationBarCoordinator();
        String query = locationBar.getUrlBarCoordinator().getTextWithoutAutocomplete();
        boolean showEmptyState = query != null && !query.isEmpty() && !hasSuggestions;
        if (showEmptyState) {
            mEncounteredEmptyStateThisSession = true;
        }

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

        hide(TabSearchDismissalReason.URL_LOADED);
        return true;
    }

    private void bringTabToFront(TabWindowInfo tabWindowInfo, GURL url) {
        SearchActivityUtils.bringTabToFront(
                mActivity,
                mTabModelSelectorSupplier.get(),
                tabWindowInfo,
                url,
                () -> hide(TabSearchDismissalReason.TAB_SELECTED));
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

        hide(TabSearchDismissalReason.TAB_GROUP_SELECTED);
    }

    /**
     * Shows the tab search overlay with the specified entry point. If the overlay has not been
     * inflated and attached to the parent container yet, this method will initialize it.
     *
     * @param entryPoint The {@link TabSearchEntryPoint} where the overlay was triggered from.
     */
    public void show(@TabSearchEntryPoint int entryPoint) {
        RecordHistogram.recordEnumeratedHistogram(
                "Android.TabSearch.EntryPoint", entryPoint, TabSearchEntryPoint.NUM_ENTRIES);

        ensureInitialized();
        if (mModel.get(TabSearchOverlayProperties.VISIBLE)) return;

        if (mFuseboxControls != null) {
            mFuseboxControls.endFuseboxInput();
        }

        // Obscure underlying tabs and toolbar to suppress accessibility focus and screen reader
        // interactions.
        if (mTabObscuringToken == null) {
            mTabObscuringToken =
                    mTabObscuringHandler.obscure(TabObscuringHandler.Target.ALL_TABS_AND_TOOLBAR);
        }

        // Reset session-scoped metrics tracking for the new search session.
        mEncounteredEmptyStateThisSession = false;

        if (mPopupWindow != null && !mPopupWindow.isShowing()) {
            View decorView = mActivity.getWindow().getDecorView();
            // Anchor the popup window to the top-left of the screen to cover the full window.
            mPopupWindow.showAtLocation(decorView, Gravity.START | Gravity.TOP, 0, 0);
        }

        updatePanelTopMargin();
        mModel.set(TabSearchOverlayProperties.EMPTY_STATE_VISIBLE, false);
        mModel.set(TabSearchOverlayProperties.VISIBLE, true);
        mBackPressStateSupplier.set(true);
        assumeNonNull(mSearchUiCoordinator)
                .beginQuery(IntentOrigin.HUB, SearchType.TEXT, /* query= */ null, mWindowAndroid);
        updateExclusionRects();
    }

    /**
     * Hides the tab search overlay and logs the dismissal reason.
     *
     * @param reason The {@link TabSearchDismissalReason} for hiding the overlay.
     */
    public void hide(@TabSearchDismissalReason int reason) {
        if (!isVisible()) return;

        RecordHistogram.recordEnumeratedHistogram(
                "Android.TabSearch.DismissalReason", reason, TabSearchDismissalReason.NUM_ENTRIES);
        RecordHistogram.recordBooleanHistogram(
                "Android.TabSearch.SessionHadEmptyState", mEncounteredEmptyStateThisSession);

        if (mTabObscuringToken != null) {
            mTabObscuringHandler.unobscure(mTabObscuringToken);
            mTabObscuringToken = null;
        }

        mModel.set(TabSearchOverlayProperties.VISIBLE, false);
        mBackPressStateSupplier.set(false);
        updateExclusionRects();
    }

    /**
     * Hides the tab search overlay if it is currently visible.
     *
     * @param reason The {@link TabSearchDismissalReason} for hiding the overlay.
     */
    private void hideIfVisible(@TabSearchDismissalReason int reason) {
        if (isVisible()) {
            hide(reason);
        }
    }

    /** Returns whether the tab search overlay is currently visible. */
    public boolean isVisible() {
        return mModel.get(TabSearchOverlayProperties.VISIBLE);
    }

    /**
     * Updates the top margin of the panel view.
     *
     * <p>In desktop windowing, OS caption controls overlapping the window are unavoidable. We use
     * {@link AppHeaderState#getCaptionControlsTopOffset()} to ensure the panel header aligns with
     * the caption controls (or is offset below any status bar sitting above the caption).
     *
     * <p>When not in desktop windowing (conventional app state, e.g. fullscreen), the top margin is
     * calculated via {@link #getTopMarginForConventionalState()} so the panel aligns below the
     * system status bar.
     */
    private void updatePanelTopMargin() {
        if (mPanelContainer == null) {
            return;
        }
        View panelView = mPanelContainer.findViewById(R.id.tab_search_overlay_panel);
        if (panelView == null
                || !(panelView.getLayoutParams() instanceof LinearLayout.LayoutParams params)) {
            return;
        }

        AppHeaderState appHeaderState =
                mDesktopWindowStateManager != null
                        ? mDesktopWindowStateManager.getAppHeaderState()
                        : null;

        int topMargin =
                appHeaderState != null && appHeaderState.isInDesktopWindow()
                        ? appHeaderState.getCaptionControlsTopOffset()
                        : getTopMarginForConventionalState();

        if (params.topMargin != topMargin) {
            params.topMargin = topMargin;
            panelView.setLayoutParams(params);
        }
    }

    /**
     * Calculates the top margin for the panel when in a conventional app state (not in desktop
     * windowing).
     *
     * <p>The top margin is calculated from the control container and toolbar positions ({@code
     * toolbarTop - tabStripHeight}) so the panel aligns below the system status bar.
     */
    private int getTopMarginForConventionalState() {
        View controlContainer = mActivity.findViewById(R.id.control_container);
        View toolbarContainer =
                controlContainer != null
                        ? controlContainer.findViewById(R.id.toolbar_container)
                        : null;
        int tabStripHeight =
                mActivity.getResources().getDimensionPixelSize(R.dimen.tab_strip_height);
        int toolbarTop = 0;
        if (toolbarContainer != null) {
            int[] location = new int[2];
            toolbarContainer.getLocationInWindow(location);
            toolbarTop = location[1] > 0 ? location[1] : toolbarContainer.getTop();
        }
        return Math.max(0, toolbarTop - tabStripHeight);
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

        List<Rect> rects = new ArrayList<>();
        if (mModel.get(TabSearchOverlayProperties.VISIBLE)) {
            View panelView = mPanelContainer.findViewById(R.id.tab_search_overlay_panel);
            View closeButton =
                    panelView != null ? panelView.findViewById(R.id.tab_search_close_button) : null;
            if (closeButton != null && closeButton.getWidth() > 0) {
                // Exclude the close button's interactive area to ensure it remains clickable even
                // when under the system gesture area.
                Rect rect = new Rect();
                rect.left = closeButton.getLeft() + panelView.getLeft();
                rect.top = closeButton.getTop() + panelView.getTop();
                rect.right = closeButton.getRight() + panelView.getLeft();
                rect.bottom = closeButton.getBottom() + panelView.getTop();
                rects.add(rect);
            }

            if (mDesktopWindowStateManager != null
                    && mDesktopWindowStateManager.getAppHeaderState() != null
                    && mDesktopWindowStateManager.getAppHeaderState().isInDesktopWindow()) {
                AppHeaderState state = mDesktopWindowStateManager.getAppHeaderState();
                int headerHeight = state.getAppHeaderHeight();
                if (headerHeight > 0 && mPanelContainer.getWidth() > 0) {
                    // Exclude the top caption/header bar area from system window dragging. While
                    // this region is normally used to drag the window, clicks here should instead
                    // dismiss the visible tab search panel (via the overlay scrim). Once the panel
                    // is dismissed, dragging capability will be restored.
                    rects.add(new Rect(0, 0, mPanelContainer.getWidth(), headerHeight));
                }
            }
        }
        mPanelContainer.setSystemGestureExclusionRects(rects);
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
        mModel.set(TabSearchOverlayProperties.IS_INCOGNITO, isIncognito);
        mSearchBoxDataProvider.initialize(mActivity, isIncognito);
        if (mSearchUiCoordinator != null) {
            mSearchUiCoordinator.setColorScheme(isIncognito);
        }
    }

    private void onHideFinished() {
        // Clear focus only after the hide animation finishes to prevent animation flicker.
        if (mSearchUiCoordinator != null) {
            var locationBar = mSearchUiCoordinator.getLocationBarCoordinator();
            locationBar.clearOmniboxFocus();
        }
        if (mPopupWindow != null && mPopupWindow.isShowing()) {
            mPopupWindow.dismiss();
        }
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

    // BackPressHandler implementation.

    @Override
    public @BackPressResult int handleBackPress() {
        hide(TabSearchDismissalReason.BACK_PRESS);
        return BackPressResult.SUCCESS;
    }

    @Override
    public NonNullObservableSupplier<Boolean> getHandleBackPressChangedSupplier() {
        return mBackPressStateSupplier;
    }

    // DesktopWindowStateManager.AppHeaderObserver implementation.

    @Override
    public void onAppHeaderStateChanged(AppHeaderState newState) {
        hideIfVisible(TabSearchDismissalReason.WINDOW_RESIZED);
    }

    @Override
    public void onDesktopWindowingModeChanged(boolean isInDesktopWindow) {
        hideIfVisible(TabSearchDismissalReason.WINDOW_RESIZED);
    }

    // ConfigurationChangedObserver implementation.

    @Override
    public void onConfigurationChanged(Configuration newConfig) {
        hideIfVisible(TabSearchDismissalReason.WINDOW_RESIZED);
    }

    // Testing methods.

    @Nullable LinearLayout getPanelContainerForTesting() {
        return mPanelContainer;
    }

    PropertyModel getModelForTesting() {
        return mModel;
    }

    void setSearchUiCoordinatorForTesting(SearchUiCoordinator searchUiCoordinator) {
        mSearchUiCoordinator = searchUiCoordinator;
    }

    ViewTreeObserver.@Nullable OnWindowFocusChangeListener getWindowFocusListenerForTesting() {
        return mWindowFocusListener;
    }

    @Nullable PopupWindow getPopupWindowForTesting() {
        return mPopupWindow;
    }
}

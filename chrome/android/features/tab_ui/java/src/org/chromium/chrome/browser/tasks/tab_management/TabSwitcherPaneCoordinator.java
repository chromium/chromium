// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.build.NullUtil.assumeNonNull;
import static org.chromium.chrome.browser.tab_ui.TabSwitcherGroupSuggestionService.recordGroupSuggestionHistogram;
import static org.chromium.chrome.browser.tasks.tab_management.TabKeyEventHandler.onPageKeyEvent;
import static org.chromium.chrome.browser.tasks.tab_management.TabListContainerProperties.ALL_KEYS;
import static org.chromium.chrome.browser.tasks.tab_management.TabListContainerProperties.BROWSER_CONTROLS_STATE_PROVIDER;
import static org.chromium.chrome.browser.tasks.tab_management.TabListContainerProperties.FETCH_VIEW_BY_INDEX_CALLBACK;
import static org.chromium.chrome.browser.tasks.tab_management.TabListContainerProperties.GET_VISIBLE_RANGE_CALLBACK;
import static org.chromium.chrome.browser.tasks.tab_management.TabListContainerProperties.IS_SCROLLING_SUPPLIER_CALLBACK;
import static org.chromium.chrome.browser.tasks.tab_management.TabListContainerProperties.MODE;
import static org.chromium.chrome.browser.tasks.tab_management.TabListContainerProperties.PAGE_KEY_LISTENER;

import android.app.Activity;
import android.content.ComponentCallbacks;
import android.content.res.ColorStateList;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.graphics.Color;
import android.graphics.Rect;
import android.util.Size;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.ImageView;

import androidx.annotation.DrawableRes;
import androidx.annotation.VisibleForTesting;
import androidx.core.util.Function;
import androidx.core.util.Pair;
import androidx.recyclerview.widget.RecyclerView;
import androidx.recyclerview.widget.RecyclerView.ViewHolder;

import org.chromium.base.Callback;
import org.chromium.base.DeviceInfo;
import org.chromium.base.SysUtils;
import org.chromium.base.TimeUtils.UptimeMillisTimer;
import org.chromium.base.Token;
import org.chromium.base.TraceEvent;
import org.chromium.base.ValueChangedCallback;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.LazyOneshotSupplier;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.bookmarks.TabBookmarker;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.collaboration.CollaborationServiceFactory;
import org.chromium.chrome.browser.crash.ChromePureJavaExceptionReporter;
import org.chromium.chrome.browser.data_sharing.DataSharingTabManager;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.hub.DirectionalScrollListener;
import org.chromium.chrome.browser.hub.HubUtils;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileProvider;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabId;
import org.chromium.chrome.browser.tab_ui.RecyclerViewPosition;
import org.chromium.chrome.browser.tab_ui.TabContentManager;
import org.chromium.chrome.browser.tab_ui.TabSwitcherCustomViewManager;
import org.chromium.chrome.browser.tab_ui.TabSwitcherGroupSuggestionService;
import org.chromium.chrome.browser.tab_ui.TabSwitcherGroupSuggestionService.SuggestionUiEvent;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabList;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tasks.tab_management.PriceWelcomeMessageController.PriceMessageUpdateObserver;
import org.chromium.chrome.browser.tasks.tab_management.TabGridContextMenuCoordinator.ShowTabListEditor;
import org.chromium.chrome.browser.tasks.tab_management.TabGridDialogMediator.DialogController;
import org.chromium.chrome.browser.tasks.tab_management.TabGridItemLongPressOrchestrator.CancelLongPressTabItemEventListener;
import org.chromium.chrome.browser.tasks.tab_management.TabListCoordinator.TabListMode;
import org.chromium.chrome.browser.tasks.tab_management.TabListEditorCoordinator.TabListEditorController;
import org.chromium.chrome.browser.tasks.tab_management.TabListMediator.GridCardOnClickListenerProvider;
import org.chromium.chrome.browser.tasks.tab_management.TabSwitcherMessageManager.MessageUpdateObserver;
import org.chromium.chrome.browser.tasks.tab_management.pinned_tabs_strip.PinnedTabStripCoordinator;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeController;
import org.chromium.chrome.browser.undo_tab_close_snackbar.UndoBarThrottle;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.desktop_windowing.DesktopWindowStateManager;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler;
import org.chromium.components.browser_ui.widget.scrim.ScrimManager;
import org.chromium.components.collaboration.CollaborationService;
import org.chromium.components.collaboration.ServiceStatus;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.edge_to_edge.EdgeToEdgePadAdjuster;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.widget.ViewRectProvider;

import java.util.List;
import java.util.Objects;
import java.util.Set;
import java.util.function.Supplier;

/** Coordinator for a {@link TabSwitcherPaneBase}'s UI. */
@NullMarked
public class TabSwitcherPaneCoordinator implements BackPressHandler {
    static final String COMPONENT_NAME = "GridTabSwitcher";
    static final int XR_FADING_EDGE_LENGTH_PX = 24;
    static final boolean CONTEXT_MENU_FOCUSABLE = true;

    private final MessageUpdateObserver mMessageUpdateObserver =
            new MessageUpdateObserver() {
                @Override
                public void onAppendedMessage() {
                    updateBottomPadding();
                }

                @Override
                public void onRemovedMessage() {
                    updateBottomPadding();
                }

                @Override
                public void onRemoveAllAppendedMessage() {
                    updateBottomPadding();
                }

                @Override
                public void onRestoreAllAppendedMessage() {
                    updateBottomPadding();
                }
            };

    private final PriceMessageUpdateObserver mPriceMessageUpdateObserver =
            new PriceMessageUpdateObserver() {
                @Override
                public void onShowPriceWelcomeMessage() {
                    updateBottomPadding();
                }

                @Override
                public void onRemovePriceWelcomeMessage() {
                    updateBottomPadding();
                }

                @Override
                public void onRestorePriceWelcomeMessage() {
                    updateBottomPadding();
                }
            };

    private final ComponentCallbacks mComponentsCallbacks =
            new ComponentCallbacks() {
                @Override
                public void onConfigurationChanged(Configuration configuration) {
                    maybeMakeSpaceForSearchBar();
                }

                @Override
                public void onLowMemory() {}
            };

    private final TabModelObserver mTabModelObserver =
            new TabModelObserver() {
                @Override
                public void didChangePinState(Tab tab) {
                    if (mPinnedTabsCoordinator == null
                            || mHubSearchBoxVisibilitySupplier.get() == null) {
                        return;
                    }
                    if (isAnyTabPinned()) {
                        if (mHubSearchBoxVisibilitySupplier.get()) {
                            // If search box is visible either we are at the start of the recycler
                            // view or we had no pinned tabs.
                            updatePinnedTabsStripOnScroll(
                                    /* shouldShowSearchBox= */ true, /* forced= */ true);
                        } else {
                            mPinnedTabsCoordinator.onScrolled();
                        }
                    } else {
                        mMediator.setHubSearchBoxVisibility(true);
                    }
                }
            };
    private final View.OnLayoutChangeListener mOnLayoutChangedAfterInitialScrollListener =
            new View.OnLayoutChangeListener() {
                @Override
                public void onLayoutChange(
                        View view, int i, int i1, int i2, int i3, int i4, int i5, int i6, int i7) {
                    if (mPinnedTabsCoordinator != null) {
                        updatePinnedTabsStripOnScroll(
                                /* shouldShowSearchBox= */ true, /* forced= */ true);
                    }
                    mTabListCoordinator.getContainerView().removeOnLayoutChangeListener(this);
                }
            };

    private final TabGridItemLongPressOrchestrator.OnLongPressTabItemEventListener
            mLongPressItemEventListener = this::onLongPressOnTabCard;
    private final Activity mActivity;
    private final ProfileProvider mProfileProvider;
    private final Callback<Boolean> mOnVisibilityChanged = this::onVisibilityChanged;
    private final ObservableSupplier<Boolean> mIsVisibleSupplier;
    private final ObservableSupplier<Boolean> mIsAnimatingSupplier;
    private final TabSwitcherPaneMediator mMediator;
    private final Supplier<Boolean> mTabGridDialogVisibilitySupplier = this::isTabGridDialogVisible;
    private final MultiThumbnailCardProvider mMultiThumbnailCardProvider;
    private final TabListCoordinator mTabListCoordinator;
    private final PropertyModel mContainerViewModel;
    private final PropertyModelChangeProcessor mContainerViewChangeProcessor;
    private final LazyOneshotSupplier<DialogController> mDialogControllerSupplier;
    private final TabListEditorManager mTabListEditorManager;
    private final ViewGroup mParentView;
    private final TabSwitcherMessageManager mMessageManager;
    private final ModalDialogManager mModalDialogManager;
    private final BottomSheetController mBottomSheetController;
    private final Runnable mOnDestroyed;
    private final ObservableSupplier<EdgeToEdgeController> mEdgeToEdgeSupplier;
    private final TabListOnScrollListener mTabListOnScrollListener = new TabListOnScrollListener();
    private final OneshotSupplierImpl<ObservableSupplier<Boolean>> mIsScrollingSupplier =
            new OneshotSupplierImpl<>();
    private final Callback<EdgeToEdgeController> mOnEdgeToEdgeControllerChangedCallback =
            new ValueChangedCallback<>(this::onEdgeToEdgeControllerChanged);
    private final @Nullable TabGroupLabeller mTabGroupLabeller;
    private final ObservableSupplier<@Nullable TabGroupModelFilter> mTabGroupModelFilterSupplier;
    private final ObservableSupplier<ShareDelegate> mShareDelegateSupplier;
    private final ObservableSupplier<TabBookmarker> mTabBookmarkerSupplier;
    private final @Nullable Runnable mOnTabGroupCreation;
    private final Callback<@Nullable TabGroupModelFilter> mOnFilterChange = this::onFilterChange;
    private final ObservableSupplierImpl<Boolean> mIsContextMenuFocusableSupplier =
            new ObservableSupplierImpl<>();
    private final Callback<Boolean> mOnContextMenuFocusableChanged =
            this::onContextMenuFocusableChanged;
    private final ObservableSupplierImpl<Boolean> mHubSearchBoxVisibilitySupplier;
    private final @Nullable ImageView mPaneHairline;
    private @Nullable TabGridContextMenuCoordinator mContextMenuCoordinator;
    private @Nullable TabGroupListBottomSheetCoordinator mTabGroupListBottomSheetCoordinator;

    /** Lazily initialized when shown. */
    private @Nullable TabGridDialogCoordinator mTabGridDialogCoordinator;

    private @Nullable Function<Integer, View> mFetchViewByIndex;
    private @Nullable Supplier<Pair<Integer, Integer>> mGetVisibleIndex;
    private EdgeToEdgePadAdjuster mEdgeToEdgePadAdjuster;
    private TabListCoordinator.@Nullable DragObserver mDragObserver;
    private @Nullable TabSwitcherGroupSuggestionService mTabSwitcherGroupSuggestionService;
    private @Nullable PinnedTabStripCoordinator mPinnedTabsCoordinator;
    private @Nullable DirectionalScrollListener mSearchBoxVisibilityScrollListener;
    private int mEdgeToEdgeBottomInsets;

    /**
     * @param activity The {@link Activity} that hosts the pane.
     * @param profileProvider The provider for profiles.
     * @param tabGroupModelFilterSupplier The supplier of the tab model filter fo rthis pane.
     * @param tabContentManager For management of thumbnails.
     * @param browserControlsStateProvider For determining thumbnail size.
     * @param scrimManager The scrim component to use for the tab grid dialog.
     * @param modalDialogManager The modal dialog manager for the activity.
     * @param bottomSheetController The {@link BottomSheetController} for the current activity.
     * @param dataSharingTabManager The {@link} DataSharingTabManager managing communication between
     *     UI and DataSharing services.
     * @param messageManager The {@link TabSwitcherMessageManager} for the message service.
     * @param parentView The view to use as a parent.
     * @param resetHandler The tab list reset handler for the pane.
     * @param isVisibleSupplier The supplier of the pane's visibility.
     * @param isAnimatingSupplier Whether the pane is animating into or out of view.
     * @param onTabClickCallback Callback to invoke when a tab is clicked.
     * @param mode The {@link TabListMode} to use.
     * @param supportsEmptyState Whether empty state UI should be shown when the model is empty.
     * @param onTabGroupCreation Should be run when the UI is used to create a tab group.
     * @param onDestroyed A {@link Runnable} to execute when {@link #destroy()} is invoked.
     * @param edgeToEdgeSupplier Supplier to the {@link EdgeToEdgeController} instance.
     * @param desktopWindowStateManager Manager to get desktop window and app header state.
     * @param shareDelegateSupplier Supplies the {@link ShareDelegate} that will be used to share
     *     the tab's URL when the user selects the "Share" option.
     * @param tabBookmarkerSupplier Supplier of {@link TabBookmarker} for bookmarking a given tab.
     * @param undoBarThrottle Throttle to block undo snackbar.
     * @param setOverlayViewCallback Callback to set the current overlay view.
     * @param tabSwitcherDragHandler An instance of the {@link TabSwitcherDragHandler}.
     * @param hubSearchBoxVisibilitySupplier Used to set the visibility of the hub search box.
     */
    public TabSwitcherPaneCoordinator(
            Activity activity,
            ProfileProvider profileProvider,
            ObservableSupplier<@Nullable TabGroupModelFilter> tabGroupModelFilterSupplier,
            TabContentManager tabContentManager,
            BrowserControlsStateProvider browserControlsStateProvider,
            ScrimManager scrimManager,
            ModalDialogManager modalDialogManager,
            BottomSheetController bottomSheetController,
            DataSharingTabManager dataSharingTabManager,
            TabSwitcherMessageManager messageManager,
            ViewGroup parentView,
            TabSwitcherResetHandler resetHandler,
            ObservableSupplier<Boolean> isVisibleSupplier,
            ObservableSupplier<Boolean> isAnimatingSupplier,
            Callback<Integer> onTabClickCallback,
            @TabListMode int mode,
            boolean supportsEmptyState,
            @Nullable Runnable onTabGroupCreation,
            Runnable onDestroyed,
            ObservableSupplier<EdgeToEdgeController> edgeToEdgeSupplier,
            @Nullable DesktopWindowStateManager desktopWindowStateManager,
            ObservableSupplier<ShareDelegate> shareDelegateSupplier,
            ObservableSupplier<TabBookmarker> tabBookmarkerSupplier,
            UndoBarThrottle undoBarThrottle,
            Callback<@Nullable View> setOverlayViewCallback,
            @Nullable TabSwitcherDragHandler tabSwitcherDragHandler,
            ObservableSupplierImpl<Boolean> hubSearchBoxVisibilitySupplier) {
        try (TraceEvent e = TraceEvent.scoped("TabSwitcherPaneCoordinator.constructor")) {
            mProfileProvider = profileProvider;
            mIsVisibleSupplier = isVisibleSupplier;
            mIsAnimatingSupplier = isAnimatingSupplier;
            mActivity = activity;
            mModalDialogManager = modalDialogManager;
            mBottomSheetController = bottomSheetController;
            mParentView = parentView;
            mOnDestroyed = onDestroyed;
            mEdgeToEdgeSupplier = edgeToEdgeSupplier;
            mTabGroupModelFilterSupplier = tabGroupModelFilterSupplier;
            mOnTabGroupCreation = onTabGroupCreation;
            mShareDelegateSupplier = shareDelegateSupplier;
            mTabBookmarkerSupplier = tabBookmarkerSupplier;
            mHubSearchBoxVisibilitySupplier = hubSearchBoxVisibilitySupplier;

            assert mode != TabListMode.STRIP : "TabListMode.STRIP not supported.";

            ViewGroup coordinatorView = activity.findViewById(R.id.coordinator);

            PropertyModel containerViewModel =
                    new PropertyModel.Builder(ALL_KEYS)
                            .with(BROWSER_CONTROLS_STATE_PROVIDER, browserControlsStateProvider)
                            .with(MODE, mode)
                            .with(FETCH_VIEW_BY_INDEX_CALLBACK, (f) -> mFetchViewByIndex = f)
                            .with(GET_VISIBLE_RANGE_CALLBACK, (f) -> mGetVisibleIndex = f)
                            .with(
                                    IS_SCROLLING_SUPPLIER_CALLBACK,
                                    (f) -> mIsScrollingSupplier.set(f))
                            .with(
                                    PAGE_KEY_LISTENER,
                                    event ->
                                            onPageKeyEvent(
                                                    event,
                                                    assumeNonNull(
                                                            mTabGroupModelFilterSupplier.get()),
                                                    /* moveSingleTab= */ false))
                            .build();

            mContainerViewModel = containerViewModel;
            Profile profile = profileProvider.getOriginalProfile();
            mDialogControllerSupplier =
                    LazyOneshotSupplier.fromSupplier(
                            () -> {
                                mTabGridDialogCoordinator =
                                        new TabGridDialogCoordinator(
                                                activity,
                                                browserControlsStateProvider,
                                                bottomSheetController,
                                                dataSharingTabManager,
                                                tabGroupModelFilterSupplier,
                                                tabContentManager,
                                                resetHandler,
                                                getGridCardOnClickListenerProvider(),
                                                TabSwitcherPaneCoordinator.this
                                                        ::getTabGridDialogAnimationSourceView,
                                                scrimManager,
                                                mModalDialogManager,
                                                desktopWindowStateManager,
                                                undoBarThrottle,
                                                tabBookmarkerSupplier,
                                                shareDelegateSupplier,
                                                setOverlayViewCallback);
                                mTabGridDialogCoordinator.setPageKeyEvent(
                                        event ->
                                                onPageKeyEvent(
                                                        event,
                                                        assumeNonNull(
                                                                mTabGroupModelFilterSupplier.get()),
                                                        /* moveSingleTab= */ true));
                                return mTabGridDialogCoordinator.getDialogController();
                            });

            mMediator =
                    new TabSwitcherPaneMediator(
                            mActivity,
                            resetHandler,
                            tabGroupModelFilterSupplier,
                            mDialogControllerSupplier,
                            containerViewModel,
                            parentView,
                            this::onTabSwitcherShown,
                            isVisibleSupplier,
                            isAnimatingSupplier,
                            onTabClickCallback,
                            this::getNthTabIndexInModel,
                            bottomSheetController,
                            this::addOnLayoutChangedAfterInitialScrollListener,
                            hubSearchBoxVisibilitySupplier);

            mMultiThumbnailCardProvider =
                    new MultiThumbnailCardProvider(
                            activity,
                            browserControlsStateProvider,
                            tabContentManager,
                            tabGroupModelFilterSupplier);

            var recyclerViewTimer = new UptimeMillisTimer();

            @DrawableRes
            int emptyImageResId =
                    DeviceFormFactor.isNonMultiDisplayContextOnTablet(activity)
                            ? R.drawable.tablet_tab_switcher_empty_state_illustration
                            : R.drawable.phone_tab_switcher_empty_state_illustration_static;
            TabListCoordinator tabListCoordinator =
                    new TabListCoordinator(
                            mode,
                            activity,
                            browserControlsStateProvider,
                            mModalDialogManager,
                            tabGroupModelFilterSupplier,
                            mMultiThumbnailCardProvider,
                            /* actionOnRelatedTabs= */ true,
                            dataSharingTabManager,
                            getGridCardOnClickListenerProvider(),
                            /* dialogHandler= */ null,
                            TabProperties.TabActionState.CLOSABLE,
                            /* selectionDelegateProvider= */ null,
                            this::getPriceWelcomeMessageController,
                            parentView,
                            /* attachToParent= */ false,
                            COMPONENT_NAME,
                            /* onModelTokenChange= */ null,
                            /* emptyViewParent= */ supportsEmptyState ? parentView : null,
                            supportsEmptyState ? emptyImageResId : Resources.ID_NULL,
                            supportsEmptyState
                                    ? R.string.tabswitcher_no_tabs_empty_state
                                    : Resources.ID_NULL,
                            supportsEmptyState
                                    ? R.string.tabswitcher_no_tabs_open_to_visit_different_pages
                                    : Resources.ID_NULL,
                            onTabGroupCreation,
                            /* allowDragAndDrop= */ true,
                            tabSwitcherDragHandler,
                            /* undoBarExplicitTrigger= */ null,
                            /* snackbarManager= */ null,
                            TabListEditorCoordinator.UNLIMITED_SELECTION);
            mTabListCoordinator = tabListCoordinator;
            tabListCoordinator.setOnLongPressTabItemEventListener(mLongPressItemEventListener);

            TabListRecyclerView recyclerView = tabListCoordinator.getContainerView();
            // Create a `FrameLayout` to hold both the pinned tab strip and the regular tab
            // list.
            FrameLayout layout =
                    (FrameLayout)
                            LayoutInflater.from(mActivity)
                                    .inflate(
                                            R.layout.tab_switcher_pane_layout,
                                            parentView,
                                            /* attachToParent= */ false);
            parentView.addView(layout);

            FrameLayout tabListContainer = layout.findViewById(R.id.tab_list_container);
            tabListContainer.addView(recyclerView);
            mPaneHairline = layout.findViewById(R.id.pane_hairline);

            maybeMakeSpaceForSearchBar();
            mActivity.registerComponentCallbacks(mComponentsCallbacks);

            // TODO(crbug.com/436614730): Inline the view construction once feature is launched.
            if (ChromeFeatureList.sAndroidPinnedTabs.isEnabled()) {
                // If the feature is enabled, create and set up the pinned tab strip, and add it as
                // a sibling of the regular tab list. The pinned tab strip will be positioned above
                // the regular tab list.
                mPinnedTabsCoordinator =
                        new PinnedTabStripCoordinator(
                                mActivity,
                                parentView,
                                tabListCoordinator,
                                mTabGroupModelFilterSupplier,
                                tabBookmarkerSupplier,
                                bottomSheetController,
                                modalDialogManager,
                                onTabGroupCreation);

                mContainerViewModel.set(
                        TabListContainerProperties.IS_PINNED_TAB_STRIP_ANIMATING_SUPPLIER,
                        mPinnedTabsCoordinator.getIsVisibilityAnimationRunningSupplier());

                TabListRecyclerView pinnedTabStripRecyclerView =
                        mPinnedTabsCoordinator.getPinnedTabsRecyclerView();

                FrameLayout pinnedTabsContainer = layout.findViewById(R.id.pinned_tabs_container);
                pinnedTabsContainer.addView(pinnedTabStripRecyclerView);
            }

            if (DeviceInfo.isXr()) {
                recyclerView.setVerticalFadingEdgeEnabled(true);
                recyclerView.setFadingEdgeLength(XR_FADING_EDGE_LENGTH_PX);
            } else {
                mTabListOnScrollListener
                        .getYOffsetNonZeroSupplier()
                        .addObserver(this::setHairlineVisibility);
            }

            recyclerView.setVisibility(View.VISIBLE);
            recyclerView.setBackgroundColor(Color.TRANSPARENT);
            recyclerView.addOnScrollListener(mTabListOnScrollListener);
            if (ChromeFeatureList.sAndroidPinnedTabs.isEnabled()) {
                mSearchBoxVisibilityScrollListener =
                        new DirectionalScrollListener(
                                () -> { // Scroll up.
                                    if (isAnyTabPinned()) {
                                        updatePinnedTabsStripOnScroll(
                                                /* shouldShowSearchBox= */ true,
                                                /* forced= */ false);
                                    }
                                },
                                () -> { // Scroll down.
                                    if (isAnyTabPinned()) {
                                        updatePinnedTabsStripOnScroll(
                                                /* shouldShowSearchBox= */ false,
                                                /* forced= */ false);
                                    }
                                });
                // While the DirectionalScrollListener handles continuous scrolling, this is needed
                // to notify the PinnedTabStripCoordinator of the final scroll position once a
                // fling settles, ensuring its internal state is consistent.
                RecyclerView.OnScrollListener scrollStateChangedListener =
                        new RecyclerView.OnScrollListener() {
                            @Override
                            public void onScrollStateChanged(
                                    RecyclerView recyclerView, int newState) {
                                super.onScrollStateChanged(recyclerView, newState);

                                if (newState == RecyclerView.SCROLL_STATE_IDLE) {
                                    assert mPinnedTabsCoordinator != null;
                                    if (isAnyTabPinned()) {
                                        mPinnedTabsCoordinator.onScrolled();
                                    }
                                }
                            }
                        };
                recyclerView.addOnScrollListener(mSearchBoxVisibilityScrollListener);
                recyclerView.addOnScrollListener(scrollStateChangedListener);
            }
            mTabGroupModelFilterSupplier.get().getTabModel().addObserver(mTabModelObserver);
            mContainerViewChangeProcessor =
                    PropertyModelChangeProcessor.create(
                            containerViewModel,
                            new TabListContainerViewBinder.ViewHolder(recyclerView, mPaneHairline),
                            TabListContainerViewBinder::bind);

            mEdgeToEdgePadAdjuster =
                    new EdgeToEdgePadAdjuster() {
                        @Override
                        public void overrideBottomInset(int inset) {
                            mEdgeToEdgeBottomInsets = inset;
                            updateBottomPadding();
                        }

                        @Override
                        public void destroy() {}
                    };
            mEdgeToEdgeSupplier.addObserver(mOnEdgeToEdgeControllerChangedCallback);

            RecordHistogram.recordTimesHistogram(
                    "Android.TabSwitcher.SetupRecyclerView.Time",
                    recyclerViewTimer.getElapsedMillis());

            TabListEditorManager tabListEditorManager =
                    new TabListEditorManager(
                            activity,
                            mModalDialogManager,
                            coordinatorView,
                            /* rootView= */ parentView,
                            browserControlsStateProvider,
                            tabGroupModelFilterSupplier,
                            tabContentManager,
                            tabListCoordinator,
                            bottomSheetController,
                            mode,
                            onTabGroupCreation,
                            desktopWindowStateManager,
                            mEdgeToEdgeSupplier);
            mTabListEditorManager = tabListEditorManager;
            mMediator.setTabListEditorControllerSupplier(
                    mTabListEditorManager.getControllerSupplier());

            mMessageManager = messageManager;
            mMessageManager.registerMessageHostDelegate(
                    MessageHostDelegateFactory.build(tabListCoordinator));

            CollaborationService collaborationService =
                    CollaborationServiceFactory.getForProfile(profile);
            ServiceStatus serviceStatus = collaborationService.getServiceStatus();
            if (serviceStatus.isAllowedToJoin()) {
                mTabGroupLabeller =
                        new TabGroupLabeller(
                                profile,
                                mTabListCoordinator.getTabListNotificationHandler(),
                                tabGroupModelFilterSupplier);
            } else {
                mTabGroupLabeller = null;
            }

            mOnVisibilityChanged.onResult(
                    assumeNonNull(isVisibleSupplier.addObserver(mOnVisibilityChanged)));
            mTabGroupModelFilterSupplier.addObserver(mOnFilterChange);

            mDragObserver =
                    new TabListCoordinator.DragObserver() {
                        @Override
                        public void onDragStart() {
                            // Prevent the context menu from interfering with tab dragging.
                            mIsContextMenuFocusableSupplier.set(false);
                        }

                        @Override
                        public void onDragEnd() {
                            // Restore default behavior.
                            mIsContextMenuFocusableSupplier.set(CONTEXT_MENU_FOCUSABLE);
                        }
                    };

            mIsContextMenuFocusableSupplier.set(CONTEXT_MENU_FOCUSABLE);
            mIsContextMenuFocusableSupplier.addObserver(mOnContextMenuFocusableChanged);
            tabListCoordinator.addDragObserver(mDragObserver);

            if (ChromeFeatureList.sTabSwitcherGroupSuggestionsAndroid.isEnabled()) {
                mTabSwitcherGroupSuggestionService =
                        TabSwitcherGroupSuggestionServiceFactory.build(
                                activity,
                                mTabGroupModelFilterSupplier,
                                profile,
                                mTabListCoordinator,
                                assumeNonNull(
                                        messageManager.getTabGroupSuggestionMessageService()));
            }
        }
    }

    /** Destroys the coordinator. */
    @SuppressWarnings("NullAway")
    public void destroy() {
        mIsContextMenuFocusableSupplier.removeObserver(mOnContextMenuFocusableChanged);
        mTabListCoordinator.removeDragObserver(mDragObserver);
        mDragObserver = null;
        mMessageManager.removeObserver(mMessageUpdateObserver);
        PriceWelcomeMessageController priceWelcomeMessageController =
                getPriceWelcomeMessageController();
        if (priceWelcomeMessageController != null) {
            priceWelcomeMessageController.removeObserver(mPriceMessageUpdateObserver);
        }
        mMessageManager.unbind(mTabListCoordinator);
        mMediator.destroy();
        mTabListCoordinator.onDestroy();
        mContainerViewChangeProcessor.destroy();
        if (mTabGridDialogCoordinator != null) {
            mTabGridDialogCoordinator.destroy();
        }
        mIsVisibleSupplier.removeObserver(mOnVisibilityChanged);
        mMultiThumbnailCardProvider.destroy();
        mTabListEditorManager.destroy();
        mOnDestroyed.run();
        mEdgeToEdgeSupplier.removeObserver(mOnEdgeToEdgeControllerChangedCallback);
        if (mEdgeToEdgePadAdjuster != null && mEdgeToEdgeSupplier.get() != null) {
            mEdgeToEdgeSupplier.get().unregisterAdjuster(mEdgeToEdgePadAdjuster);
            mEdgeToEdgePadAdjuster = null;
        }
        if (mTabGroupLabeller != null) {
            mTabGroupLabeller.destroy();
        }
        mTabGroupModelFilterSupplier.removeObserver(mOnFilterChange);
        if (mTabGroupListBottomSheetCoordinator != null) {
            mTabGroupListBottomSheetCoordinator.destroy();
        }
        if (mTabSwitcherGroupSuggestionService != null) {
            mTabSwitcherGroupSuggestionService.destroy();
        }
        if (ChromeFeatureList.sAndroidPinnedTabs.isEnabled()) {
            if (mTabGroupModelFilterSupplier.get() != null
                    && mTabGroupModelFilterSupplier.get().getTabModel() != null) {
                mTabGroupModelFilterSupplier.get().getTabModel().removeObserver(mTabModelObserver);
            }
        }
        if (mPinnedTabsCoordinator != null) {
            mPinnedTabsCoordinator.destroy();
        }
        mActivity.unregisterComponentCallbacks(mComponentsCallbacks);
    }

    /** Post native initialization. */
    public void initWithNative() {
        try (TraceEvent e = TraceEvent.scoped("TabSwitcherPaneCoordinator.initWithNative")) {
            Profile originalProfile = mProfileProvider.getOriginalProfile();
            mTabListCoordinator.initWithNative(originalProfile);
            mMultiThumbnailCardProvider.initWithNative(originalProfile);
        }
    }

    /** Shows the tab list editor. */
    public void showTabListEditor() {
        mTabListEditorManager.showTabListEditor();
    }

    /**
     * Resets the UI with the specified tabs.
     *
     * @param tabList The {@link TabList} to show tabs for.
     */
    public void resetWithListOfTabs(@Nullable List<Tab> tabs) {
        mMessageManager.beforeReset();
        // Quick mode being false here ensures the selected tab's thumbnail gets updated. With Hub
        // the TabListCoordinator no longer triggers thumbnail captures so this shouldn't guard
        // against the large amount of work that is used to.
        mTabListCoordinator.resetWithListOfTabs(
                tabs, /* tabGroupSyncIds= */ null, /* quickMode= */ false);
        mMessageManager.afterReset(tabs == null ? 0 : tabs.size());
        mTabListOnScrollListener.postUpdate(mTabListCoordinator.getContainerView());
        if (mTabGroupLabeller != null) {
            mTabGroupLabeller.showAll();
        }
    }

    /** Performs soft cleanup which removes thumbnails to relieve memory usage. */
    public void softCleanup() {
        mTabListCoordinator.softCleanup();
    }

    /** Performs hard cleanup which saves price drop information. */
    public void hardCleanup() {
        // TODO(crbug.com/40946413): The pre-fork implementation resets the tab list, this seems
        // suboptimal. Consider not doing this.
        resetWithListOfTabs(null);
    }

    /**
     * Scrolls so that the selected tab in the current model is in the middle of the screen or as
     * close as possible if at the start/end of the recycler view.
     */
    public void setInitialScrollIndexOffset() {
        mMediator.setInitialScrollIndexOffset();
    }

    /** Requests accessibility focus on the current tab. */
    public void requestAccessibilityFocusOnCurrentTab() {
        mMediator.requestAccessibilityFocusOnCurrentTab();
    }

    /** Returns a {@link Supplier} that provides dialog visibility information. */
    public Supplier<Boolean> getTabGridDialogVisibilitySupplier() {
        return mTabGridDialogVisibilitySupplier;
    }

    /** Provides information on whether the tab grid dialog is showing or animating. */
    public @Nullable ObservableSupplier<Boolean> getTabGridDialogShowingOrAnimationSupplier() {
        return mTabGridDialogCoordinator != null
                ? mTabGridDialogCoordinator.getShowingOrAnimationSupplier()
                : null;
    }

    /** Provides the tab ID for the most recently swiped tab. */
    public ObservableSupplier<Integer> getRecentlySwipedTabIdSupplier() {
        return mTabListCoordinator.getRecentlySwipedTabSupplier();
    }

    /** Returns whether the TabListEditor needs a clean up. */
    public boolean doesTabListEditorNeedCleanup() {
        @Nullable TabListEditorController controller = mMediator.getTabListEditorController();
        return controller != null && controller.needsCleanUp();
    }

    /** Returns a {@link TabSwitcherCustomViewManager.Delegate} for supplying custom views. */
    public TabSwitcherCustomViewManager.@Nullable Delegate
            getTabSwitcherCustomViewManagerDelegate() {
        return mMediator;
    }

    /** Indicates whether any animator for the {@link TabListRecyclerView} is running. */
    public @Nullable ObservableSupplier<Boolean> getIsRecyclerViewAnimatorRunning() {
        TabListRecyclerView containerView = mTabListCoordinator.getContainerView();
        if (containerView == null) {
            return null;
        }
        return containerView.getIsAnimatorRunningSupplier();
    }

    /** Returns the number of elements in the tab switcher's tab list model. */
    public int getTabSwitcherTabListModelSize() {
        return mTabListCoordinator.getTabListModelSize();
    }

    /** Set the tab switcher's RecyclerViewPosition. */
    public void setTabSwitcherRecyclerViewPosition(RecyclerViewPosition position) {
        mTabListCoordinator.setRecyclerViewPosition(position);
    }

    /** Returns the {@link Rect} of the recyclerview in global coordinates. */
    public Rect getRecyclerViewRect() {
        return mTabListCoordinator.getRecyclerViewLocation();
    }

    /**
     * @param tabId The tab ID to get a rect for.
     * @return a {@link Rect} for the tab's thumbnail (may be an empty rect if the tab is not
     *     found).
     */
    public Rect getTabThumbnailRect(int tabId) {
        TabGridDialogCoordinator dialogCoordinator = mTabGridDialogCoordinator;
        if (dialogCoordinator != null && dialogCoordinator.isVisible()) {
            return dialogCoordinator.getTabThumbnailRect(tabId);
        }
        return mTabListCoordinator.getTabThumbnailRect(tabId);
    }

    /** Returns the {@link Rect} of the recyclerview in global coordinates. */
    public Size getThumbnailSize() {
        TabGridDialogCoordinator dialogCoordinator = mTabGridDialogCoordinator;
        if (dialogCoordinator != null && dialogCoordinator.isVisible()) {
            return dialogCoordinator.getThumbnailSize();
        }
        return mTabListCoordinator.getThumbnailSize();
    }

    /**
     * @param tabId The tab ID whose view to wait for.
     * @param r A runnable to be executed on the next layout pass or immediately if one is not
     *     scheduled.
     */
    public void waitForLayoutWithTab(int tabId, Runnable r) {
        TabGridDialogCoordinator dialogCoordinator = mTabGridDialogCoordinator;
        if (dialogCoordinator != null && dialogCoordinator.isVisible()) {
            dialogCoordinator.waitForLayoutWithTab(tabId, r);
            return;
        }
        mTabListCoordinator.waitForLayoutWithTab(tabId, r);
    }

    /**
     * Scrolls to the specified group and animates open a dialog. It is the caller's responsibility
     * to ensure that this pane is showing before calling this.
     *
     * @param tabId The id of any tab in the group.
     */
    public void requestOpenTabGroupDialog(int tabId) {
        mMediator.scrollToTabById(tabId);
        mMediator.openTabGroupDialog(tabId);
    }

    /** Returns the range (inclusive) of visible view indexes. */
    public @Nullable Pair<Integer, Integer> getVisibleRange() {
        return mGetVisibleIndex == null ? null : mGetVisibleIndex.get();
    }

    /** Returns the root view at a given index. */
    public @Nullable View getViewByIndex(int viewIndex) {
        return mFetchViewByIndex == null ? null : mFetchViewByIndex.apply(viewIndex);
    }

    /** Returns a nested supplier for the scrolling state of the view. */
    public OneshotSupplier<ObservableSupplier<Boolean>> getIsScrollingSupplier() {
        return mIsScrollingSupplier;
    }

    /**
     * Sets the content sensitivity on the recycler view of the tab switcher.
     *
     * @param contentIsSensitive True if the tab switcher is sensitive.
     */
    public void setTabSwitcherContentSensitivity(boolean contentIsSensitive) {
        mContainerViewModel.set(
                TabListContainerProperties.IS_CONTENT_SENSITIVE, contentIsSensitive);
    }

    @Override
    public @BackPressResult int handleBackPress() {
        return mMediator.handleBackPress();
    }

    @Override
    public ObservableSupplier<Boolean> getHandleBackPressChangedSupplier() {
        return mMediator.getHandleBackPressChangedSupplier();
    }

    @VisibleForTesting
    @Nullable CancelLongPressTabItemEventListener onLongPressOnTabCard(
            TabGridContextMenuCoordinator tabGridContextMenuCoordinator,
            TabListGroupMenuCoordinator tabListGroupMenuCoordinator,
            @TabId int tabId,
            @Nullable View cardView) {
        TabGroupModelFilter filter = mTabGroupModelFilterSupplier.get();
        assumeNonNull(filter);
        Tab tab = filter.getTabModel().getTabById(tabId);
        if (tab == null
                || cardView == null
                || !ChromeFeatureList.sTabGroupParityBottomSheetAndroid.isEnabled()) {
            return null;
        }

        ViewRectProvider viewRectProvider =
                new ViewRectProvider(cardView, TabGridViewRectUpdater::new);
        Token groupId = tab.getTabGroupId();
        boolean focusable = assumeNonNull(mIsContextMenuFocusableSupplier.get());
        if (groupId != null) {
            tabListGroupMenuCoordinator.showMenu(viewRectProvider, groupId, focusable);
            return tabListGroupMenuCoordinator::dismiss;
        } else {
            tabGridContextMenuCoordinator.showMenu(viewRectProvider, tabId, focusable);
            RecordUserAction.record("TabSwitcher.ContextMenu");
            return tabGridContextMenuCoordinator::dismiss;
        }
    }

    private void onContextMenuFocusableChanged(boolean focusable) {
        if (mContextMenuCoordinator == null) {
            boolean isTabModelIncognito =
                    mTabGroupModelFilterSupplier.get().getTabModel().isOffTheRecord();
            String logMessage =
                    "ContextMenuCoordinator is null due to null profile. isTabModelIncognito = "
                            + isTabModelIncognito;
            ChromePureJavaExceptionReporter.reportJavaException(new Throwable(logMessage));
            return;
        }
        mContextMenuCoordinator.setMenuFocusable(focusable);
    }

    private boolean isTabGridDialogVisible() {
        return mTabGridDialogCoordinator == null ? false : mTabGridDialogCoordinator.isVisible();
    }

    private void onTabSwitcherShown() {
        if (ChromeFeatureList.sTabSwitcherGroupSuggestionsAndroid.isEnabled()) {
            recordGroupSuggestionHistogram(SuggestionUiEvent.TAB_SWITCHER_OPENED);
            showGroupSuggestionsAfterAnimations();
        }

        mTabListCoordinator.attachEmptyView();
    }

    private void showGroupSuggestionsAfterAnimations() {
        mIsAnimatingSupplier.addSyncObserver(
                new Callback<>() {
                    @Override
                    public void onResult(Boolean result) {
                        if (!Objects.equals(result, false)) return;

                        assert mTabSwitcherGroupSuggestionService != null;
                        if (ChromeFeatureList.sTabSwitcherGroupSuggestionsTestModeAndroid
                                .isEnabled()) {
                            mTabSwitcherGroupSuggestionService.forceTabGroupSuggestion();
                        } else {
                            mTabSwitcherGroupSuggestionService.maybeShowSuggestions();
                        }
                        mIsAnimatingSupplier.removeObserver(this);
                    }
                });
    }

    private @Nullable View getTabGridDialogAnimationSourceView(Token tabGroupId) {
        // Returning null causes the animation to be a fade.
        // Do so if we are animating to show or hide the HubLayout or this is a low end device.
        if (mIsAnimatingSupplier.get() || SysUtils.isLowEndDevice()) return null;

        TabGroupModelFilter filter = mTabGroupModelFilterSupplier.get();
        assumeNonNull(filter);
        int tabId = filter.getGroupLastShownTabId(tabGroupId);
        if (tabId == Tab.INVALID_TAB_ID) return null;

        TabListCoordinator coordinator = mTabListCoordinator;
        int index = coordinator.getIndexForTabIdWithRelatedTabs(tabId);
        ViewHolder sourceViewHolder =
                coordinator.getContainerView().findViewHolderForAdapterPosition(index);
        // TODO(crbug.com/41479135): This is band-aid fix that will show basic fade-in/fade-out
        // animation when we cannot find the animation source view holder. This is happening due to
        // current group id in TabGridDialog can not be indexed in TabListModel, which should never
        // happen. Remove this when figure out the actual cause.
        return sourceViewHolder == null ? null : sourceViewHolder.itemView;
    }

    private void onVisibilityChanged(boolean visible) {
        PriceWelcomeMessageController priceWelcomeMessageController =
                getPriceWelcomeMessageController();
        if (visible) {
            mMessageManager.bind(
                    mTabListCoordinator,
                    mParentView,
                    /* priceWelcomeMessageReviewActionProvider= */ mMediator,
                    (tabId) -> mMediator.onTabSelecting(tabId, false));
            mMessageManager.addObserver(mMessageUpdateObserver);
            if (priceWelcomeMessageController != null) {
                priceWelcomeMessageController.addObserver(mPriceMessageUpdateObserver);
            }
            updateBottomPadding();
            mTabListCoordinator.prepareTabSwitcherPaneView();
        } else {
            mMessageManager.removeObserver(mMessageUpdateObserver);
            if (priceWelcomeMessageController != null) {
                priceWelcomeMessageController.removeObserver(mPriceMessageUpdateObserver);
            }
            mMessageManager.unbind(mTabListCoordinator);
            updateBottomPadding();
            mTabListCoordinator.postHiding();

            if (mTabSwitcherGroupSuggestionService != null) {
                mTabSwitcherGroupSuggestionService.clearSuggestions();
            }
        }
    }

    private GridCardOnClickListenerProvider getGridCardOnClickListenerProvider() {
        return mMediator;
    }

    private @Nullable PriceWelcomeMessageController getPriceWelcomeMessageController() {
        return mMessageManager.getPriceWelcomeMessageController();
    }

    private @Nullable CancelLongPressTabItemEventListener onLongPressOnTabCard(
            int tabId, @Nullable View cardView) {
        assert mContextMenuCoordinator != null;
        return onLongPressOnTabCard(
                mContextMenuCoordinator,
                assumeNonNull(mTabListCoordinator.getTabListGroupMenuCoordinator()),
                tabId,
                cardView);
    }

    private void onEdgeToEdgeControllerChanged(
            @Nullable EdgeToEdgeController newController,
            @Nullable EdgeToEdgeController oldController) {
        if (oldController != null) {
            oldController.unregisterAdjuster(assumeNonNull(mEdgeToEdgePadAdjuster));
        }
        if (newController != null && mEdgeToEdgePadAdjuster != null) {
            newController.registerAdjuster(mEdgeToEdgePadAdjuster);
        }
    }

    /** Returns the container view property model for testing. */
    PropertyModel getContainerViewModelForTesting() {
        return mContainerViewModel;
    }

    /** Returns the dialog controller for testing. */
    @Nullable DialogController getTabGridDialogControllerForTesting() {
        return mDialogControllerSupplier.get();
    }

    /** Return the Edge to edge pad adjuster. */
    EdgeToEdgePadAdjuster getEdgeToEdgePadAdjusterForTesting() {
        return mEdgeToEdgePadAdjuster;
    }

    @Nullable PinnedTabStripCoordinator getPinnedTabsCoordinatorForTesting() {
        return mPinnedTabsCoordinator;
    }

    public @Nullable DirectionalScrollListener getDirectionalScrollListenerForTesting() {
        return mSearchBoxVisibilityScrollListener;
    }

    /* package */ @Nullable TabGridDialogCoordinator getTabGridDialogCoordinatorForTesting() {
        return mTabGridDialogCoordinator;
    }

    public ComponentCallbacks getComponentsCallbacksForTesting() {
        return mComponentsCallbacks;
    }

    void showQuickDeleteAnimation(Runnable onAnimationEnd, List<Tab> tabs) {
        Runnable onAnimEnd =
                () -> {
                    onAnimationEnd.run();
                    // Update the pinned tabs bar, as there might be movement of items in the
                    // recycler view.
                    if (mPinnedTabsCoordinator != null) mPinnedTabsCoordinator.onScrolled();
                };
        mTabListCoordinator.showQuickDeleteAnimation(onAnimEnd, tabs);

        // Reveal the search bar if needed.
        if (mPinnedTabsCoordinator != null) {
            updatePinnedTabsStripOnScroll(/* shouldShowSearchBox= */ true, /* forced= */ false);
        }
    }

    /** Returns the filter index of a tab from its view index. */
    public int countOfTabCardsOrInvalid(int viewIndex) {
        return mTabListCoordinator.indexOfTabCardsOrInvalid(viewIndex);
    }

    private int getNthTabIndexInModel(int filterIndex) {
        assert mTabListCoordinator != null;
        int indexInModel = mTabListCoordinator.getIndexOfNthTabCard(filterIndex);
        // If the tab list coordinator doesn't contain tab data yet assume filterIndex is a
        // sufficient approximation as the offset would be caused by message cards.
        if (indexInModel == TabList.INVALID_TAB_INDEX) return filterIndex;

        return indexInModel;
    }

    private void updateBottomPadding() {
        mContainerViewModel.set(
                TabListContainerProperties.IS_CLIP_TO_PADDING, mEdgeToEdgeBottomInsets == 0);
        mContainerViewModel.set(TabListContainerProperties.BOTTOM_PADDING, mEdgeToEdgeBottomInsets);
    }

    private void onFilterChange(@Nullable TabGroupModelFilter filter) {
        assumeNonNull(filter);
        if (mTabGroupListBottomSheetCoordinator != null) {
            mTabGroupListBottomSheetCoordinator.destroy();
        }

        TabGroupCreationDialogManager tabGroupCreationDialogManager =
                new TabGroupCreationDialogManager(
                        mActivity, mModalDialogManager, mOnTabGroupCreation);

        Profile profile = filter.getTabModel().getProfile();
        if (profile == null) return;

        mTabGroupListBottomSheetCoordinator =
                new TabGroupListBottomSheetCoordinator(
                        mActivity,
                        profile,
                        tabGroupId -> tabGroupCreationDialogManager.showDialog(tabGroupId, filter),
                        /* tabMovedCallback= */ null,
                        filter,
                        mBottomSheetController,
                        /* supportsShowNewGroup= */ true,
                        /* destroyOnHide= */ false);

        ShowTabListEditor showTabListEditor =
                tabId -> {
                    mTabListEditorManager.showTabListEditor();
                    TabListEditorController tabListEditorController =
                            mTabListEditorManager.getControllerSupplier().get();
                    if (tabListEditorController != null) {
                        tabListEditorController.selectTabs(
                                Set.of(TabListEditorItemSelectionId.createTabId(tabId)));
                    }
                };
        mContextMenuCoordinator =
                TabGridContextMenuCoordinator.createContextMenuCoordinator(
                        mActivity,
                        mTabBookmarkerSupplier,
                        filter,
                        mTabGroupListBottomSheetCoordinator,
                        tabGroupCreationDialogManager,
                        mShareDelegateSupplier,
                        showTabListEditor);
        if (mPaneHairline != null) {
            mPaneHairline.setImageTintList(
                    ColorStateList.valueOf(
                            TabUiThemeProvider.getPaneHairlineColor(
                                    mActivity, profile.isIncognitoBranded())));
        }
    }

    private void addOnLayoutChangedAfterInitialScrollListener() {
        mTabListCoordinator
                .getContainerView()
                .addOnLayoutChangeListener(mOnLayoutChangedAfterInitialScrollListener);
    }

    private void updatePinnedTabsStripOnScroll(boolean shouldShowSearchBox, boolean forced) {
        assert mPinnedTabsCoordinator != null;
        mPinnedTabsCoordinator.onScrolled();
        if (mPinnedTabsCoordinator.isPinnedTabsBarVisible()) {
            mMediator.maybeTranslatePinnedStrip(shouldShowSearchBox, forced);
        }
    }

    private void maybeMakeSpaceForSearchBar() {
        Configuration config = mActivity.getResources().getConfiguration();
        boolean isTabletOrLandscape = HubUtils.isScreenWidthTablet(config.screenWidthDp);
        mMediator.setIsTabletOrLandscape(isTabletOrLandscape);
        if (isTabletOrLandscape) {
            if (mPinnedTabsCoordinator != null) {
                mMediator.maybeTranslatePinnedStrip(
                        /* shouldShowSearchBox= */ false, /* forced= */ true);
            }
        } else {
            if (mPinnedTabsCoordinator != null) {
                mMediator.maybeTranslatePinnedStrip(
                        /* shouldShowSearchBox= */ true, /* forced= */ true);
            }
        }
    }

    private boolean isAnyTabPinned() {
        return mTabGroupModelFilterSupplier.get().getTabModel().getPinnedTabsCount() > 0;
    }

    private void setHairlineVisibility(boolean isYOffsetNonZero) {
        if (mPaneHairline != null) {
            mContainerViewModel.set(
                    TabListContainerProperties.IS_NON_ZERO_Y_OFFSET, isYOffsetNonZero);
        }
    }
}

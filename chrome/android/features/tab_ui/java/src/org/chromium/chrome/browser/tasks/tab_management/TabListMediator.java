// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.build.NullUtil.assertNonNull;
import static org.chromium.build.NullUtil.assumeNonNull;
import static org.chromium.chrome.browser.tasks.tab_management.MessageCardViewProperties.MESSAGE_TYPE;
import static org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.CARD_ALPHA;
import static org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.CARD_TYPE;
import static org.chromium.chrome.browser.tasks.tab_management.TabProperties.TAB_GROUP_COLOR_VIEW_PROVIDER;
import static org.chromium.chrome.browser.tasks.tab_management.TabProperties.TAB_ID;
import static org.chromium.chrome.browser.tasks.tab_management.TabProperties.THUMBNAIL_FETCHER;
import static org.chromium.chrome.browser.tasks.tab_management.UiTypeHelper.isLargeMessageCard;
import static org.chromium.chrome.browser.tasks.tab_management.UiTypeHelper.isMessageCard;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.animation.ObjectAnimator;
import android.app.Activity;
import android.content.ComponentCallbacks;
import android.content.Context;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.Rect;
import android.graphics.drawable.Drawable;
import android.os.Bundle;
import android.os.Handler;
import android.text.TextUtils;
import android.util.Pair;
import android.util.Size;
import android.util.SparseIntArray;
import android.view.View;
import android.view.accessibility.AccessibilityNodeInfo;
import android.view.accessibility.AccessibilityNodeInfo.AccessibilityAction;

import androidx.annotation.IdRes;
import androidx.annotation.IntDef;
import androidx.annotation.StringRes;
import androidx.annotation.VisibleForTesting;
import androidx.recyclerview.widget.GridLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.base.Callback;
import org.chromium.base.CollectionUtil;
import org.chromium.base.DeviceInfo;
import org.chromium.base.Log;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.Token;
import org.chromium.base.ValueChangedCallback;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.base.supplier.NonNullObservableSupplier;
import org.chromium.base.supplier.NullableObservableSupplier;
import org.chromium.build.annotations.Initializer;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.actor.ui.ActorUiTabController;
import org.chromium.chrome.browser.actor.ui.ActorUiTabController.UiTabState;
import org.chromium.chrome.browser.actor.ui.TabIndicatorStatus;
import org.chromium.chrome.browser.collaboration.CollaborationServiceFactory;
import org.chromium.chrome.browser.compositor.overlays.strip.TabUnderlineManager;
import org.chromium.chrome.browser.data_sharing.DataSharingServiceFactory;
import org.chromium.chrome.browser.data_sharing.DataSharingTabManager;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.price_tracking.PriceTrackingFeatures;
import org.chromium.chrome.browser.price_tracking.PriceTrackingUtilities;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.quick_delete.QuickDeleteAnimationGradientDrawable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabId;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tab.TabUtils;
import org.chromium.chrome.browser.tab.state.ShoppingPersistedTabData;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncFeatures;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncServiceFactory;
import org.chromium.chrome.browser.tab_ui.TabListFaviconProvider;
import org.chromium.chrome.browser.tab_ui.TabListFaviconProvider.TabFaviconFetcher;
import org.chromium.chrome.browser.tab_ui.ThumbnailProvider;
import org.chromium.chrome.browser.tab_ui.ThumbnailProvider.MultiThumbnailMetadata;
import org.chromium.chrome.browser.tabmodel.TabClosingSource;
import org.chromium.chrome.browser.tabmodel.TabClosureParams;
import org.chromium.chrome.browser.tabmodel.TabClosureParamsUtils;
import org.chromium.chrome.browser.tabmodel.TabGroupColorUtils;
import org.chromium.chrome.browser.tabmodel.TabGroupTitleUtils;
import org.chromium.chrome.browser.tabmodel.TabList;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelActionListener;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.browser.tasks.tab_management.PriceMessageService.PriceTabData;
import org.chromium.chrome.browser.tasks.tab_management.TabActionButtonData.TabActionButtonType;
import org.chromium.chrome.browser.tasks.tab_management.TabGridItemLongPressOrchestrator.OnLongPressTabItemEventListener;
import org.chromium.chrome.browser.tasks.tab_management.TabGridItemTouchHelperCallback.OnDropOnArchivalMessageCardEventListener;
import org.chromium.chrome.browser.tasks.tab_management.TabGridItemTouchHelperCallback.UngroupBarStatusHandler;
import org.chromium.chrome.browser.tasks.tab_management.TabGridView.QuickDeleteAnimationStatus;
import org.chromium.chrome.browser.tasks.tab_management.TabListModel.AnimationStatus;
import org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties;
import org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.ModelType;
import org.chromium.chrome.browser.tasks.tab_management.TabProperties.TabActionState;
import org.chromium.chrome.browser.tasks.tab_management.TabProperties.UiType;
import org.chromium.chrome.browser.tasks.tab_management.TabSwitcherMessageManager.MessageType;
import org.chromium.chrome.browser.tasks.tab_management.TabUiMetricsHelper.TabListEditorActionMetricGroups;
import org.chromium.chrome.browser.tasks.tab_management.vertical_tabs.VerticalTabListProperties.RailCollapseState;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.undo_tab_close_snackbar.UndoBarExplicitTrigger;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.components.browser_ui.util.TextResolver;
import org.chromium.components.browser_ui.util.motion.MotionEventInfo;
import org.chromium.components.browser_ui.widget.list_view.ListViewTouchTracker;
import org.chromium.components.browser_ui.widget.selectable_list.SelectionDelegate;
import org.chromium.components.collaboration.CollaborationService;
import org.chromium.components.collaboration.CollaborationServiceLeaveOrDeleteEntryPoint;
import org.chromium.components.collaboration.CollaborationServiceShareOrManageEntryPoint;
import org.chromium.components.data_sharing.DataSharingService;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.components.tab_group_sync.EitherId.EitherGroupId;
import org.chromium.components.tab_group_sync.LocalTabGroupId;
import org.chromium.components.tab_group_sync.SavedTabGroup;
import org.chromium.components.tab_group_sync.SavedTabGroupTab;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.components.tab_groups.TabGroupColorId;
import org.chromium.components.tab_groups.TabGroupColorPickerUtils;
import org.chromium.components.tabs.TabAlert;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.ListObservable;
import org.chromium.ui.modelutil.ListObservable.ListObserver;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;
import org.chromium.ui.recyclerview.widget.ItemTouchHelper2;
import org.chromium.url.GURL;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.Collections;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Objects;
import java.util.Set;
import java.util.TreeMap;
import java.util.function.Supplier;

/**
 * Mediator for business logic for the tab grid. This class should be initialized with a list of
 * tabs and a TabModel to observe for changes and should not have any logic around what the list
 * signifies. TODO(yusufo): Move some of the logic here to a parent component to make the above
 * true.
 */
@NullMarked
public class TabListMediator implements TabListNotificationHandler {
    /**
     * An interface to expose functionality needed to support reordering in grid layouts in
     * accessibility mode.
     */
    public interface TabGridAccessibilityHelper {
        /**
         * This method gets the possible actions for reordering a tab in grid layout.
         *
         * @param view The host view that triggers the accessibility action.
         * @return The list of possible {@link AccessibilityAction}s for host view.
         */
        List<AccessibilityAction> getPotentialActionsForView(View view);

        /**
         * This method gives the previous and target position of current reordering based on the
         * host view and current action.
         *
         * @param view   The host view that triggers the accessibility action.
         * @param action The id of the action.
         * @return {@link Pair} that contains previous and target position of this action.
         */
        Pair<Integer, Integer> getPositionsOfReorderAction(View view, int action);

        /**
         * This method returns whether the given action is a type of the reordering actions.
         *
         * @param action The accessibility action.
         * @return Whether the given action is a reordering action.
         */
        boolean isReorderAction(int action);
    }

    /**
     * An interface to get a SelectionDelegate that contains the selected items for a selectable tab
     * list.
     */
    public interface SelectionDelegateProvider<E> {
        SelectionDelegate<E> getSelectionDelegate();
    }

    /** An interface to get the onClickListener when clicking on a tab list item. */
    public interface TabListItemOnClickListenerProvider {
        /**
         * Returns the {@link TabActionListener} to handle a tab group card click. If the given
         * {@link Tab} is not able to create a group, return null.
         */
        @Nullable TabActionListener onTabGroupClicked(Tab tab);

        /**
         * Returns the {@link TabActionListener} to handle a tab group card click. If the given
         * syncId is not able to create a group, return null.
         */
        @Nullable TabActionListener onTabGroupClicked(String syncId);

        /**
         * Returns whether the given tab group card should show as selected. If this returns null,
         * falls back to the default selection behavior.
         */
        @Nullable Boolean isTabGroupSelected(Tab tab, PropertyModel model);

        /**
         * Returns the {@link TabActionButtonData} for a tab group card. If this returns null, the
         * card will not show any action button.
         */
        @Nullable TabActionButtonData getTabGroupActionButtonData(
                Tab tab,
                PropertyModel model,
                Supplier<TabActionListener> defaultOverflowListenerSupplier);

        /**
         * Run additional actions on tab selection.
         *
         * @param tabId The ID of selected {@link Tab}.
         */
        void onTabSelecting(int tabId);
    }

    /** Defines the structural geometry and visual packaging policy for the TabList. */
    @IntDef({
        TabListLayoutType.FLAT,
        TabListLayoutType.GROUPED,
        TabListLayoutType.NESTED,
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface TabListLayoutType {
        /**
         * Standard flat grid or list. Does not visually package tabs into clusters or headers.
         * Surfaces using this layout: - {@link TabGroupUiCoordinator} (Bottom Tab Strip) - {@link
         * TabGridDialogCoordinator} (Inside the Group Popup) - {@link TabListEditorCoordinator}
         * (Selection mode, when display groups are disabled)
         */
        int FLAT = 0;

        /**
         * Clustered grid. Visually merges an entire group of tabs into a single proxy tile model.
         * Surfaces using this layout: - {@link TabSwitcherPaneCoordinator} (Grid Tab Switcher) -
         * {@link TabListEditorCoordinator} (Selection mode, when display groups are enabled) -
         * {@link ArchivedTabsDialogCoordinator} (Implicitly uses TabListEditor with groups enabled)
         */
        int GROUPED = 1;

        /**
         * Hierarchical list. Uses dedicated group header models with physically inline child
         * models. Surfaces using this layout: - {@link
         * org.chromium.chrome.browser.tasks.tab_management.vertical_tabs.VerticalTabListCoordinator}
         * (Vertical Tabs)
         */
        int NESTED = 2;
    }

    /** Interface for toggling whether item animations will run on the recycler view. */
    interface RecyclerViewItemAnimationToggle {
        void setDisableItemAnimations(boolean state);
    }

    /** Provides capability to asynchronously acquire {@link ShoppingPersistedTabData} */
    static class ShoppingPersistedTabDataFetcher {
        protected final Tab mTab;
        protected final @Nullable Supplier<@Nullable PriceWelcomeMessageController>
                mPriceWelcomeMessageControllerSupplier;

        /**
         * @param tab {@link Tab} {@link ShoppingPersistedTabData} will be acquired for.
         * @param priceWelcomeMessageControllerSupplier to show the price welcome message.
         */
        ShoppingPersistedTabDataFetcher(
                Tab tab,
                @Nullable Supplier<@Nullable PriceWelcomeMessageController>
                        priceWelcomeMessageControllerSupplier) {
            mTab = tab;
            mPriceWelcomeMessageControllerSupplier = priceWelcomeMessageControllerSupplier;
        }

        /**
         * Asynchronously acquire {@link ShoppingPersistedTabData}
         *
         * @param callback {@link Callback} to pass {@link ShoppingPersistedTabData} back in
         */
        public void fetch(Callback<@Nullable ShoppingPersistedTabData> callback) {
            ShoppingPersistedTabData.from(
                    mTab,
                    (res) -> {
                        callback.onResult(res);
                        maybeShowPriceWelcomeMessage(res);
                    });
        }

        @VisibleForTesting
        void maybeShowPriceWelcomeMessage(
                @Nullable ShoppingPersistedTabData shoppingPersistedTabData) {
            // Avoid inserting message while RecyclerView is computing a layout.
            new Handler()
                    .post(
                            () -> {
                                if (!PriceTrackingUtilities.isPriceWelcomeMessageCardEnabled(
                                                mTab.getProfile())
                                        || (mPriceWelcomeMessageControllerSupplier == null)
                                        || (mPriceWelcomeMessageControllerSupplier.get() == null)
                                        || (shoppingPersistedTabData == null)
                                        || (shoppingPersistedTabData.getPriceDrop() == null)) {
                                    return;
                                }
                                mPriceWelcomeMessageControllerSupplier
                                        .get()
                                        .showPriceWelcomeMessage(
                                                new PriceTabData(
                                                        mTab.getId(),
                                                        shoppingPersistedTabData.getPriceDrop()));
                            });
        }
    }

    @IntDef({
        TabClosedFrom.TAB_STRIP,
        TabClosedFrom.GRID_TAB_SWITCHER,
        TabClosedFrom.GRID_TAB_SWITCHER_GROUP,
        TabClosedFrom.VERTICAL_TABS,
        TabClosedFrom.VERTICAL_TABS_GROUP
    })
    @Retention(RetentionPolicy.SOURCE)
    private @interface TabClosedFrom {
        int TAB_STRIP = 0;
        // int TAB_GRID_SHEET = 1;  // Obsolete
        int GRID_TAB_SWITCHER = 2;
        int GRID_TAB_SWITCHER_GROUP = 3;
        int VERTICAL_TABS = 4;
        int VERTICAL_TABS_GROUP = 5;
    }

    private static final String TAG = "TabListMediator";
    private final SparseIntArray mTabClosedFrom = new SparseIntArray();

    private final Callback<@Nullable TabModel> mOnTabModelChanged =
            new ValueChangedCallback<>(this::onTabModelChanged);
    private final TabOverflowMenuCoordinator.OnItemClickedCallback<Token>
            mOnMenuItemClickedCallback = this::onMenuItemClicked;
    private final Activity mActivity;
    private final TabListModel mModelList;
    private final @Nullable ModalDialogManager mModalDialogManager;
    private final NullableObservableSupplier<TabModel> mCurrentTabModelSupplier;
    private final @Nullable ThumbnailProvider mThumbnailProvider;
    private final TabListFaviconProvider mTabListFaviconProvider;
    private final @Nullable SelectionDelegateProvider<TabListEditorItemSelectionId>
            mSelectionDelegateProvider;
    private final @Nullable TabListItemOnClickListenerProvider mTabListItemOnClickListenerProvider;
    private final @Nullable UngroupBarStatusHandler mUngroupBarStatusHandler;
    private final @Nullable Supplier<@Nullable PriceWelcomeMessageController>
            mPriceWelcomeMessageControllerSupplier;
    private final @Nullable DataSharingTabManager mDataSharingTabManager;
    private final TabModelObserver mTabModelObserver;
    private final TabListLayoutDelegate mTabListLayoutDelegate;
    private final TabListObserverManager mObserverManager;
    private final TabActionListener mTabClosedListener;
    private final TabGridItemTouchHelperCallback mTabGridItemTouchHelperCallback;
    private final @Nullable TabMultiSelectHelper mMultiSelectHelper;
    private final @Nullable UndoBarExplicitTrigger mUndoBarExplicitTrigger;
    private final @Nullable SnackbarManager mSnackbarManager;
    private final @Nullable NonNullObservableSupplier<@RailCollapseState Integer>
            mRailCollapseStateSupplier;
    private final @Nullable Callback<@RailCollapseState Integer> mRailCollapseStateObserver;
    private final int mAllowedSelectionCount;
    private final boolean mIsSingleContextMode;
    private final @TabListLayoutType int mLayoutType;
    private final TabListConfig mTabListConfig;
    private final @Nullable TabUnderlineManager mTabUnderlineManager;

    private int mLastSelectedTabListModelIndex = TabList.INVALID_TAB_INDEX;
    private @TabComponentId int mComponentId;
    private @TabActionState int mTabActionState;
    private @Nullable Profile mOriginalProfile;
    private @Nullable TabGroupSyncService mTabGroupSyncService;
    private @Nullable DataSharingService mDataSharingService;
    private @Nullable CollaborationService mCollaborationService;
    private @Nullable TabListGroupMenuCoordinator mTabListGroupMenuCoordinator;
    private @Nullable Size mDefaultGridCardSize;
    private @Nullable ComponentCallbacks mComponentCallbacks;
    private @Nullable GridLayoutManager mGridLayoutManager;
    // Set to true after a `resetWithListOfTabs` that used a non-null list of tabs. Remains true
    // until `postHiding` is invoked or the mediator is destroyed. While true, this mediator is
    // actively tracking updates to a TabModel.
    private boolean mShowingTabs;
    private @Nullable Tab mTabToAddDelayed;
    private RecyclerViewItemAnimationToggle mRecyclerViewItemAnimationToggle;
    private @Nullable ListObserver<Void> mListObserver;
    private View.AccessibilityDelegate mAccessibilityDelegate;
    private int mCurrentSpanCount;
    private @Nullable OnLongPressTabItemEventListener mOnLongPressTabItemEventListener;

    private final ActorUiTabController.Observer mActorObserver =
            new ActorUiTabController.Observer() {
                @Override
                public void onUiTabStateChanged(UiTabState state) {
                    int tabId = state.tabId;
                    Tab tab = getCurrentTabModelChecked().getTabById(tabId);
                    if (tab == null) return;

                    PropertyModel model = mModelList.getModelFromTabId(tabId);
                    if (model != null) {
                        updateActorUiState(model, state);
                    }

                    mTabListLayoutDelegate.onUiTabStateChanged(tab, state);
                }
            };

    private final TabUnderlineManager.Observer mTabUnderlineObserver =
            new TabUnderlineManager.Observer() {
                @Override
                public void onIndicatorStateChanged(int tabId, boolean isActive) {
                    PropertyModel model = mModelList.getModelFromTabId(tabId);
                    if (model != null) {
                        model.set(TabProperties.IS_GLIC_ACTIVE, isActive);
                    }
                }

                @Override
                public void onResetAnimationCycle(int tabId) {}
            };

    private final TabActionListener mTabSelectedListener =
            new TabActionListener() {
                @Override
                public void run(View view, int tabId, @Nullable MotionEventInfo triggeringMotion) {
                    if (mModelList.indexFromTabId(tabId) == TabModel.INVALID_TAB_INDEX) return;

                    mTabListLayoutDelegate.recordTabSelection(tabId);
                    if (mMultiSelectHelper != null) {
                        int modifiers = triggeringMotion != null ? triggeringMotion.metaState : 0;
                        if (mMultiSelectHelper.handleTabClick(tabId, modifiers)) {
                            return;
                        }
                    }
                    handleTabSelection(tabId);
                }

                @Override
                public void run(
                        View view, String syncId, @Nullable MotionEventInfo triggeringMotion) {
                    // Intentional no-op.
                }
            };

    private final TabActionListener mSelectableTabOnClickListener =
            new TabActionListener() {
                @Override
                public void run(View view, int tabId, @Nullable MotionEventInfo triggeringMotion) {
                    @Nullable PropertyModel model = mModelList.getModelFromTabId(tabId);
                    if (model == null) return;

                    boolean wasSelected = model.get(TabProperties.IS_SELECTED);
                    if (!mIsSingleContextMode
                            && !wasSelected
                            && mAllowedSelectionCount > 0
                            && getCurrentSelectionCount() >= mAllowedSelectionCount) {
                        showLimitSnackbar();
                        return;
                    }
                    dismissLimitSnackbar();
                    SelectionDelegate<TabListEditorItemSelectionId> selectionDelegate =
                            getTabSelectionDelegate();
                    assert selectionDelegate != null;
                    selectionDelegate.toggleSelectionForItem(
                            TabListEditorItemSelectionId.createTabId(tabId));

                    TabUiMetricsHelper.recordSelectionEditorActionMetrics(
                            wasSelected
                                    ? TabListEditorActionMetricGroups.UNSELECTED
                                    : TabListEditorActionMetricGroups.SELECTED);

                    model.set(TabProperties.IS_SELECTED, !wasSelected);

                    mTabListLayoutDelegate.onTabSelectionToggled(model, tabId, wasSelected);
                }

                @Override
                public void run(
                        View view, String syncId, @Nullable MotionEventInfo triggeringMotion) {
                    SelectionDelegate<TabListEditorItemSelectionId> selectionDelegate =
                            getTabSelectionDelegate();
                    assert selectionDelegate != null;
                    selectionDelegate.toggleSelectionForItem(
                            TabListEditorItemSelectionId.createTabGroupSyncId(syncId));

                    @Nullable PropertyModel model =
                            mModelList.getModelFromArchivedTabGroupSyncId(syncId);
                    if (model == null) return;

                    boolean wasSelected = model.get(TabProperties.IS_SELECTED);
                    model.set(TabProperties.IS_SELECTED, !wasSelected);

                    assumeNonNull(mTabGroupSyncService);
                    SavedTabGroup tabGroup = mTabGroupSyncService.getGroup(syncId);
                    if (tabGroup != null) {
                        updateThumbnailFetcher(model, tabGroup);
                    }
                }
            };

    private final TabActionListener mContextClickTabItemEventListener =
            new TabActionListener() {
                @Override
                public void run(View view, int tabId, @Nullable MotionEventInfo triggeringMotion) {
                    if (mOnLongPressTabItemEventListener == null) return;
                    mOnLongPressTabItemEventListener.onLongPressEvent(tabId, view);
                }

                @Override
                public void run(
                        View view, String syncId, @Nullable MotionEventInfo triggeringMotion) {
                    // No-op.
                }
            };

    /**
     * Construct the Mediator with the given Models and observing hooks from the given
     * ChromeActivity.
     *
     * @param activity The activity used to get some configuration information.
     * @param modelList The {@link TabListModel} to keep state about a list of {@link Tab}s.
     * @param modalDialogManager The {@link ModalDialogManager} for managing dialog lifecycles.
     * @param tabModelSupplier Used to fetch the filter that provides tab group information.
     * @param thumbnailProvider {@link ThumbnailProvider} to provide screenshot related details.
     * @param tabListFaviconProvider Provider for all favicon related drawables.
     * @param selectionDelegateProvider Provider for a {@link SelectionDelegate} that is used for a
     *     selectable list. It's null when selection is not possible.
     * @param tabListItemOnClickListenerProvider Provides click listeners for regular tabs and tab
     *     group cards.
     * @param tabListConfig Configuration policies and visual capabilities (e.g. nested tab groups,
     *     message cards, etc).
     * @param ungroupBarStatusHandler A handler to update the ungroup bar status.
     * @param priceWelcomeMessageControllerSupplier A supplier of a controller to show
     *     PriceWelcomeMessage.
     * @param componentId The {@link TabComponentId} identifying the parent UI container hosting
     *     this tab list.
     * @param initialTabActionState The initial {@link TabActionState} to use for the shown tabs.
     *     Must always be CLOSABLE for {@link UiType#STRIP}.
     * @param dataSharingTabManager The service used to initiate data sharing.
     * @param onTabGroupCreation Should be run when the UI is used to create a tab group.
     * @param undoBarExplicitTrigger Interface to explicitly trigger the undo closure snackbar.
     * @param snackbarManager The manager to show snackbars.
     * @param allowedSelectionCount The maximum number of tabs that can be selected at once.
     * @param isSingleContextMode Whether this mediator runs in a single context mode.
     * @param onDragStateChangedListener Listener for drag state changes.
     */
    public TabListMediator(
            Activity activity,
            TabListModel modelList,
            @Nullable ModalDialogManager modalDialogManager,
            NullableObservableSupplier<TabModel> tabModelSupplier,
            @Nullable ThumbnailProvider thumbnailProvider,
            TabListFaviconProvider tabListFaviconProvider,
            @Nullable SelectionDelegateProvider<TabListEditorItemSelectionId>
                    selectionDelegateProvider,
            @Nullable TabListItemOnClickListenerProvider tabListItemOnClickListenerProvider,
            TabListConfig tabListConfig,
            @Nullable UngroupBarStatusHandler ungroupBarStatusHandler,
            @Nullable Supplier<@Nullable PriceWelcomeMessageController>
                    priceWelcomeMessageControllerSupplier,
            @TabComponentId int componentId,
            @TabActionState int initialTabActionState,
            @Nullable DataSharingTabManager dataSharingTabManager,
            @Nullable Runnable onTabGroupCreation,
            @Nullable UndoBarExplicitTrigger undoBarExplicitTrigger,
            @Nullable SnackbarManager snackbarManager,
            int allowedSelectionCount,
            boolean isSingleContextMode,
            Runnable onDragStateChangedListener) {
        mActivity = activity;
        mModelList = modelList;
        mModalDialogManager = modalDialogManager;
        mCurrentTabModelSupplier = tabModelSupplier;
        mThumbnailProvider = thumbnailProvider;
        mTabListFaviconProvider = tabListFaviconProvider;
        mSelectionDelegateProvider = selectionDelegateProvider;
        mTabListItemOnClickListenerProvider = tabListItemOnClickListenerProvider;
        mLayoutType = tabListConfig.layoutType;
        mRailCollapseStateSupplier = tabListConfig.railCollapseStateSupplier;
        mTabListConfig = tabListConfig;
        mTabUnderlineManager = tabListConfig.tabUnderlineManager;
        if (mTabUnderlineManager != null) {
            mTabUnderlineManager.addObserver(mTabUnderlineObserver);
        }
        mUngroupBarStatusHandler = ungroupBarStatusHandler;
        mPriceWelcomeMessageControllerSupplier = priceWelcomeMessageControllerSupplier;
        mComponentId = componentId;
        mTabActionState = initialTabActionState;
        mDataSharingTabManager = dataSharingTabManager;
        mUndoBarExplicitTrigger = undoBarExplicitTrigger;
        mSnackbarManager = snackbarManager;
        mAllowedSelectionCount = allowedSelectionCount;
        mIsSingleContextMode = isSingleContextMode;
        mMultiSelectHelper =
                tabListConfig.supportsModifierMultiSelect
                        ? new TabMultiSelectHelper(
                                this::getCurrentTabModelChecked, this::handleTabSelection)
                        : null;

        switch (mLayoutType) {
            case TabListLayoutType.FLAT:
                mTabListLayoutDelegate = new FlatLayoutDelegate(this, mModelList);
                break;
            case TabListLayoutType.GROUPED:
                mTabListLayoutDelegate =
                        new GroupedLayoutDelegate(this, mModelList, mThumbnailProvider);
                break;
            case TabListLayoutType.NESTED:
                mTabListLayoutDelegate = new NestedLayoutDelegate(this, mModelList);
                break;
            default:
                throw new IllegalArgumentException("Unsupported layout type: " + mLayoutType);
        }
        mObserverManager = new TabListObserverManager(mTabListLayoutDelegate);

        mTabModelObserver =
                new TabModelObserver() {
                    @Override
                    public void didSelectTab(Tab tab, @TabSelectionType int type, int lastId) {
                        assert mShowingTabs;

                        int tabId = tab.getId();
                        if (tabId == lastId) return;

                        mTabListLayoutDelegate.didSelectTab(tab, type, lastId);
                    }

                    @Override
                    public void tabClosureCommitted(Tab tab) {
                        mTabClosedFrom.delete(tab.getId());
                    }

                    @Override
                    public void tabClosureUndone(Tab tab) {
                        assert mShowingTabs;

                        addObserversForTab(tab);
                        mTabListLayoutDelegate.tabClosureUndone(tab);
                        recordUndoCloseMetrics(tab.getId());
                    }

                    @Override
                    public void onTabsSelectionChanged() {
                        if (!mTabListConfig.supportsModifierMultiSelect) return;

                        TabModel tabModel = mCurrentTabModelSupplier.get();
                        if (tabModel == null) return;

                        for (int i = 0; i < mModelList.size(); i++) {
                            PropertyModel model = mModelList.get(i).model;
                            if (model.getAllSetProperties().contains(TabProperties.TAB_ID)) {
                                boolean isMultiSelected =
                                        tabModel.isTabMultiSelected(
                                                model.get(TabProperties.TAB_ID));
                                model.set(TabProperties.IS_MULTI_SELECTED, isMultiSelected);
                            }
                        }
                    }

                    @Override
                    public void didAddTab(
                            Tab tab,
                            @TabLaunchType int type,
                            @TabCreationState int creationState,
                            boolean markedForSelection) {
                        assert mShowingTabs;

                        TabModel tabModel = mCurrentTabModelSupplier.get();
                        if (tabModel == null || !tabModel.isTabModelRestored()) {
                            return;
                        }

                        addObserversForTab(tab);

                        // Check if we need to delay tab addition to model.
                        boolean isSupportedLaunchType =
                                type == TabLaunchType.FROM_TAB_SWITCHER_UI
                                        || type == TabLaunchType.FROM_TAB_GROUP_UI;
                        boolean delayAdd =
                                isSupportedLaunchType
                                        && markedForSelection
                                        && mTabListConfig.supportsDelayedTabAddition;
                        if (delayAdd) {
                            mTabToAddDelayed = tab;
                            return;
                        }

                        mTabListLayoutDelegate.didAddTab(tab, type);
                    }

                    @Override
                    public void didMoveTab(Tab tab, int newIndex, int curIndex) {
                        assert mShowingTabs;

                        mTabListLayoutDelegate.didMoveTab(tab, newIndex, curIndex);
                    }

                    @Override
                    public void didRemoveTabForClosure(Tab tab) {
                        assert mShowingTabs;

                        removeObserversForTab(tab);
                        mTabListLayoutDelegate.onTabClose(tab);
                    }

                    @Override
                    public void tabRemoved(Tab tab) {
                        assert mShowingTabs;

                        removeObserversForTab(tab);

                        int index = mModelList.indexFromTabId(tab.getId());
                        if (index == TabModel.INVALID_TAB_INDEX) return;

                        mModelList.removeAt(index);
                    }

                    @Override
                    public void didChangePinState(Tab tab) {
                        int index = mModelList.indexFromTabId(tab.getId());
                        if (index != TabModel.INVALID_TAB_INDEX) {
                            mModelList
                                    .get(index)
                                    .model
                                    .set(TabProperties.IS_PINNED, tab.getIsPinned());
                            int targetIndex = mTabListLayoutDelegate.getInsertionIndexOfTab(tab);
                            mModelList.moveItem(index, targetIndex);
                        }
                    }
                };

        mTabClosedListener =
                new TabActionListener() {
                    @Override
                    public void run(
                            View view, int tabId, @Nullable MotionEventInfo triggeringMotion) {
                        // TODO(crbug.com/40638921): Consider disabling all touch events during
                        // animation.

                        int closingTabIndex = mModelList.indexFromTabId(tabId);
                        if (closingTabIndex == TabModel.INVALID_TAB_INDEX) return;

                        mTabListLayoutDelegate.prepareTabCloseAnimation(view, closingTabIndex);

                        TabModel tabModel = getCurrentTabModelChecked();
                        Tab closingTab = tabModel.getTabById(tabId);
                        if (closingTab == null) return;

                        @TabClosingSource int tabClosingSource = mTabListConfig.tabClosingSource;

                        setUseShrinkCloseAnimation(tabId, /* useShrinkCloseAnimation= */ true);
                        boolean allowUndo = TabClosureParamsUtils.shouldAllowUndo(triggeringMotion);
                        if (mTabListLayoutDelegate.isChildTabRepresentedByGroupCard(closingTab)) {
                            onGroupClosedFrom(tabId);
                            TabUiUtils.closeTabGroup(
                                    tabModel,
                                    tabId,
                                    tabClosingSource,
                                    allowUndo,
                                    /* hideTabGroups= */ true,
                                    getOnMaybeTabClosedCallback(tabId));
                            return;
                        }

                        onTabClosedFrom(tabId, mComponentId);
                        TabClosureParams closureParams =
                                TabClosureParams.closeTab(closingTab)
                                        .allowUndo(allowUndo)
                                        .tabClosingSource(tabClosingSource)
                                        .build();

                        @Nullable TabModelActionListener listener =
                                TabUiUtils.buildMaybeDidCloseTabListener(
                                        getOnMaybeTabClosedCallback(tabId));
                        tabModel.getTabRemover()
                                .closeTabs(closureParams, /* allowDialog= */ true, listener);

                        if (mComponentId == TabComponentId.ARCHIVED_TABS_DIALOG
                                && mUndoBarExplicitTrigger != null) {
                            mUndoBarExplicitTrigger.triggerSnackbarForTab(closingTab);
                        }
                    }

                    @Override
                    public void run(
                            View view, String syncId, @Nullable MotionEventInfo triggeringMotion) {
                        int index = mModelList.indexFromArchivedTabGroupSyncId(syncId);
                        if (index == TabModel.INVALID_TAB_INDEX) return;

                        @Nullable PropertyModel model =
                                mModelList.getModelFromArchivedTabGroupSyncId(syncId);
                        if (model != null) {
                            assumeNonNull(mTabGroupSyncService);
                            SavedTabGroup tabGroup = mTabGroupSyncService.getGroup(syncId);
                            assumeNonNull(tabGroup);
                            Long archivalTimeMs = tabGroup.archivalTimeMs;

                            // If the tab group is archived, run archival reset logic and remove the
                            // tab group from the model list.
                            if (archivalTimeMs != null) {
                                model.set(TabProperties.USE_SHRINK_CLOSE_ANIMATION, true);
                                mModelList.removeAt(index);
                                mTabGroupSyncService.updateArchivalStatus(syncId, false);

                                if (mUndoBarExplicitTrigger != null) {
                                    mUndoBarExplicitTrigger.triggerSnackbarForSavedTabGroup(syncId);
                                }

                                RecordUserAction.record(
                                        "TabGroups.ArchivedTabGroupManualCloseOnInactiveSurface");
                                RecordHistogram.recordCount1000Histogram(
                                        "TabGroups.ArchivedTabGroupManualCloseOnInactiveSurface.TabGroupTabCount",
                                        tabGroup.savedTabs.size());
                            }
                        }
                    }
                };

        TabActionListener swipeSafeTabActionListener =
                new TabActionListener() {
                    @Override
                    public void run(
                            View view, int tabId, @Nullable MotionEventInfo triggeringMotion) {
                        // The DefaultItemAnimator is prone to crashing in combination with the
                        // swipe animation when closing the last tab. Avoid this issue by disabling
                        // the default item animation for the duration of the removal of the last
                        // tab. This is a framework issue. For more details see crbug.com/40223318.
                        TabModel tabModel = mCurrentTabModelSupplier.get();

                        boolean shouldDisableItemAnimations =
                                tabModel != null && tabModel.getCount() <= 1;
                        if (shouldDisableItemAnimations) {
                            mRecyclerViewItemAnimationToggle.setDisableItemAnimations(true);
                        }

                        mTabClosedListener.run(view, tabId, /* triggeringMotion= */ null);

                        // It is necessary to post the restoration as otherwise any animation
                        // triggered by removing the tab will still use the animator as they are
                        // also posted to the UI thread.
                        if (shouldDisableItemAnimations) {
                            new Handler()
                                    .post(
                                            () ->
                                                    mRecyclerViewItemAnimationToggle
                                                            .setDisableItemAnimations(false));
                        }
                    }

                    @Override
                    public void run(
                            View view, String syncId, @Nullable MotionEventInfo triggeringMotion) {
                        // Swipe is disabled in the {@link ArchivedTabsDialogCoordinator}
                        // implementation of the TabListMediator. Intentional no-op.
                    }
                };

        var tabGroupCreationDialogManager =
                new TabGroupCreationDialogManager(
                        activity, assumeNonNull(modalDialogManager), onTabGroupCreation);
        mTabGridItemTouchHelperCallback =
                new TabGridItemTouchHelperCallback(
                        activity,
                        tabGroupCreationDialogManager,
                        mModelList,
                        () -> assertNonNull(mCurrentTabModelSupplier.get()),
                        swipeSafeTabActionListener,
                        mUngroupBarStatusHandler,
                        TabUiMetricsHelper.getComponentNameForMetrics(componentId),
                        mLayoutType,
                        onDragStateChangedListener);

        if (mRailCollapseStateSupplier != null) {
            mRailCollapseStateObserver = this::onRailCollapseStateChanged;
            mRailCollapseStateSupplier.addSyncObserverAndCallIfNonNull(mRailCollapseStateObserver);
        } else {
            mRailCollapseStateObserver = null;
        }
    }

    /** Returns whether tabs are currently being shown. */
    boolean isShowingTabs() {
        return mShowingTabs;
    }

    /** Returns whether the tab list supports displaying tab loading state. */
    boolean supportsTabLoadingState() {
        return mTabListConfig.supportsTabLoadingState;
    }

    /**
     * Returns the currently active {@link TabModel} from {@link #mCurrentTabModelSupplier},
     * asserting that the supplier returned a non-null model.
     */
    TabModel getCurrentTabModelChecked() {
        TabModel tabModel = mCurrentTabModelSupplier.get();
        assert tabModel != null;
        return tabModel;
    }

    /**
     * @param onLongPressTabItemEventListener to handle long press events on tabs.
     */
    public void setOnLongPressTabItemEventListener(
            @Nullable OnLongPressTabItemEventListener onLongPressTabItemEventListener) {
        mOnLongPressTabItemEventListener = onLongPressTabItemEventListener;
        mTabGridItemTouchHelperCallback.setOnLongPressTabItemEventListener(
                onLongPressTabItemEventListener);
    }

    @Nullable
    public OnLongPressTabItemEventListener getOnLongPressTabItemEventListenerForTesting() {
        return mOnLongPressTabItemEventListener;
    }

    /**
     * @param listener the handler for dropping tabs on top of an archival message card.
     */
    public void setOnDropOnArchivalMessageCardEventListener(
            @Nullable OnDropOnArchivalMessageCardEventListener listener) {
        mTabGridItemTouchHelperCallback.setOnDropOnArchivalMessageCardEventListener(listener);
    }

    @Initializer
    void setRecyclerViewItemAnimationToggle(
            RecyclerViewItemAnimationToggle recyclerViewItemAnimationToggle) {
        mRecyclerViewItemAnimationToggle = recyclerViewItemAnimationToggle;
    }

    /**
     * @param size The default size to use for any new Tab cards.
     */
    void setDefaultGridCardSize(Size size) {
        mDefaultGridCardSize = size;
    }

    /**
     * @return The default size to use for any tab cards.
     */
    @Nullable Size getDefaultGridCardSize() {
        return mDefaultGridCardSize;
    }

    void setLastSelectedTabListModelIndex(int index) {
        mLastSelectedTabListModelIndex = index;
    }

    boolean isTabDelayed(Tab tab) {
        return mTabToAddDelayed != null && mTabToAddDelayed == tab;
    }

    void selectTab(int oldIndex, int newIndex) {
        if (mModelList.isValidIndex(oldIndex)) {
            PropertyModel oldModel = mModelList.get(oldIndex).model;
            int lastId = oldModel.get(TAB_ID);
            oldModel.set(TabProperties.IS_SELECTED, false);
            if (mTabListLayoutDelegate.requiresThumbnailUpdateOnDeselect() && mShowingTabs) {
                updateThumbnailFetcher(oldModel, lastId);
            }
        }

        if (mModelList.isValidIndex(newIndex)) {
            PropertyModel newModel = mModelList.get(newIndex).model;
            int newId = newModel.get(TAB_ID);
            newModel.set(TabProperties.IS_SELECTED, true);
            if (mTabListLayoutDelegate.requiresThumbnailUpdateOnSelect() && mShowingTabs) {
                updateThumbnailFetcher(newModel, newId);
            }
        }
    }

    @Initializer
    public void initWithNative(Profile originalProfile) {
        assert !originalProfile.isOffTheRecord() : "Expecting a non-incognito profile.";
        mOriginalProfile = originalProfile;
        mTabListFaviconProvider.initWithNative(originalProfile);

        mCurrentTabModelSupplier.addSyncObserverAndCallIfNonNull(mOnTabModelChanged);

        mTabGroupSyncService = TabGroupSyncServiceFactory.getForProfile(originalProfile);
        if (mTabGroupSyncService != null) {
            mDataSharingService = DataSharingServiceFactory.getForProfile(originalProfile);
        }
        mCollaborationService = CollaborationServiceFactory.getForProfile(originalProfile);

        // Right now we need to update layout only if there is a price welcome message card in tab
        // switcher.
        if (mTabListConfig.supportsMessageCards
                && mTabActionState != TabActionState.SELECTABLE
                && PriceTrackingFeatures.isPriceAnnotationsEnabled(originalProfile)) {
            mListObserver =
                    new ListObserver<>() {
                        @Override
                        public void onItemRangeInserted(
                                ListObservable source, int index, int count) {
                            updateLayout();
                        }

                        @Override
                        public void onItemRangeRemoved(
                                ListObservable source, int index, int count) {
                            updateLayout();
                        }

                        @Override
                        public void onItemRangeChanged(
                                ListObservable<Void> source,
                                int index,
                                int count,
                                @Nullable Void payload) {
                            updateLayout();
                        }

                        @Override
                        public void onItemMoved(ListObservable source, int curIndex, int newIndex) {
                            updateLayout();
                        }
                    };
            mModelList.addObserver(mListObserver);
        }
    }

    private void recordUndoCloseMetrics(int tabId) {
        int fromIndex = mTabClosedFrom.indexOfKey(tabId);
        if (fromIndex < 0) return;

        @TabClosedFrom int from = mTabClosedFrom.valueAt(fromIndex);
        switch (from) {
            case TabClosedFrom.TAB_STRIP:
                RecordUserAction.record("TabStrip.UndoCloseTab");
                break;
            case TabClosedFrom.GRID_TAB_SWITCHER:
                RecordUserAction.record("GridTabSwitch.UndoCloseTab");
                break;
            case TabClosedFrom.GRID_TAB_SWITCHER_GROUP:
                RecordUserAction.record("GridTabSwitcher.UndoCloseTabGroup");
                break;
            case TabClosedFrom.VERTICAL_TABS:
                RecordUserAction.record("Android.VerticalTabs.UndoCloseTab");
                break;
            case TabClosedFrom.VERTICAL_TABS_GROUP:
                RecordUserAction.record("Android.VerticalTabs.UndoCloseTabGroup");
                break;
            default:
                assert false : "tabClosureUndone for tab that closed from an unknown UI";
        }
        mTabClosedFrom.removeAt(fromIndex);
    }

    private void onTabClosedFrom(int tabId, @TabComponentId int componentId) {
        @TabClosedFrom int from;
        if (componentId == TabComponentId.TAB_STRIP) {
            from = TabClosedFrom.TAB_STRIP;
        } else if (componentId == TabComponentId.GRID_TAB_SWITCHER) {
            from = TabClosedFrom.GRID_TAB_SWITCHER;
        } else if (componentId == TabComponentId.VERTICAL_TABS) {
            from = TabClosedFrom.VERTICAL_TABS;
        } else {
            Log.w(TAG, "Attempting to close tab from Unknown UI: " + componentId);
            return;
        }
        mTabClosedFrom.put(tabId, from);
    }

    private void onGroupClosedFrom(int tabId) {
        @TabClosedFrom int from;
        if (mComponentId == TabComponentId.GRID_TAB_SWITCHER) {
            from = TabClosedFrom.GRID_TAB_SWITCHER_GROUP;
        } else if (mComponentId == TabComponentId.VERTICAL_TABS) {
            from = TabClosedFrom.VERTICAL_TABS_GROUP;
        } else {
            Log.w(TAG, "Attempting to close tab group from Unknown UI: " + mComponentId);
            return;
        }
        mTabClosedFrom.put(tabId, from);
    }

    /**
     * Returns all {@link Tab}s in the same tab group as the specified tab ID, or a single-element
     * list if the tab is not in a group. Returns an empty list if the current tab model is null.
     *
     * @param id The ID of the tab whose related group members are requested.
     * @return A list of related {@link Tab} instances.
     */
    List<Tab> getRelatedTabsForId(int id) {
        TabModel tabModel = mCurrentTabModelSupplier.get();
        return tabModel == null ? new ArrayList<>() : tabModel.getRelatedTabList(id);
    }

    private List<Integer> getRelatedTabIds(int id) {
        List<Tab> relatedTabs = getRelatedTabsForId(id);
        List<@TabId Integer> tabIds = new ArrayList<>(relatedTabs.size());
        for (Tab tab : relatedTabs) {
            tabIds.add(tab.getId());
        }
        return tabIds;
    }

    private boolean areTabsUnchanged(@Nullable List<Tab> tabs) {
        int tabsCount = 0;
        for (int i = 0; i < mModelList.size(); i++) {
            if (TabProperties.isTabOrTabGroup(mModelList.get(i).model)) {
                tabsCount += 1;
            }
        }
        if (tabs == null) {
            return tabsCount == 0;
        }
        if (tabs.size() != tabsCount) return false;
        int tabsIndex = 0;
        for (int i = 0; i < mModelList.size(); i++) {
            PropertyModel model = mModelList.get(i).model;
            if (TabProperties.isTabOrTabGroup(model)) {
                Tab tab = tabs.get(tabsIndex++);
                int modelTabId = TabProperties.getTabId(model);

                if (modelTabId != tab.getId()) {
                    // If the tab is in the same tab group, we can just update the model's TAB_ID
                    // rather than resetting the list.
                    if (mTabListLayoutDelegate.areTabsInSameGroup(modelTabId, tab)) {
                        continue;
                    }
                    return false;
                }
            }
        }
        return true;
    }

    /**
     * Initialize the component with a list of tabs to show in a grid.
     *
     * @param tabs The list of tabs to be shown.
     * @param tabGroupSyncIds The list of syncIds tied to {@link SavedTabGroup}s to be shown.
     * @param quickMode Whether to skip capturing the selected live tab for the thumbnail.
     * @return Whether the {@link TabListRecyclerView} can be shown quickly.
     */
    public boolean resetWithListOfTabs(
            @Nullable List<Tab> tabs, @Nullable List<String> tabGroupSyncIds, boolean quickMode) {
        mShowingTabs = tabs != null;
        // The reset supersedes any delayed tab additions, don't add the tab.
        mTabToAddDelayed = null;
        TabModel tabModel = mCurrentTabModelSupplier.get();
        if (tabs != null) {
            assert tabModel != null;
            addObservers(tabModel, tabs);
        } else {
            removeObservers(tabModel);
        }
        if (tabs != null) {
            recordPriceAnnotationsEnabledMetrics();
        }
        // Only update tabs in place if there are no saved tab groups to be shown.
        if (tabGroupSyncIds == null && areTabsUnchanged(tabs)) {
            if (tabs == null) return true;

            for (int i = 0; i < tabs.size(); i++) {
                Tab tab = tabs.get(i);
                int index = mModelList.indexOfNthTabCard(i);
                if (!mModelList.isValidIndex(index)) continue;
                // Update the id instead of reset the tab list when the tab group's selected tab id
                // changed.
                boolean updateId = mModelList.get(index).model.get(TAB_ID) != tab.getId();
                updateTab(index, tab, updateId, quickMode);
            }
            mLastSelectedTabListModelIndex = TabList.INVALID_TAB_INDEX;
            return true;
        }
        mModelList.clear();
        mLastSelectedTabListModelIndex = TabList.INVALID_TAB_INDEX;

        if (tabs == null && tabGroupSyncIds == null) {
            return true;
        }

        if (tabs != null) {
            assumeNonNull(tabModel); // Asserted above already.
            int insertionIndex = 0;
            for (Tab tab : tabs) {
                // This item represents a tab group in the list if the tab belongs to a group and
                // the switcher layout is configured to represent the tab as a group header.
                addTabCardToModel(tab, insertionIndex);

                // Flatten nested child tabs immediately below the group header if the group is
                // expanded.
                PropertyModel model = mModelList.get(insertionIndex).model;
                if (TabProperties.isTabGroupHeader(model)
                        && !TabProperties.isTabGroupCollapsed(model)) {
                    Token tabGroupId = tab.getTabGroupId();
                    assumeNonNull(tabGroupId);
                    insertionIndex += insertChildTabs(tabGroupId, insertionIndex);
                }
                insertionIndex++;
            }
        }

        // The current design has tab groups types inserted at the start of the model list.
        assumeNonNull(mTabGroupSyncService);
        if (tabGroupSyncIds != null) {
            for (int i = 0; i < tabGroupSyncIds.size(); i++) {
                SavedTabGroup savedTabGroup = mTabGroupSyncService.getGroup(tabGroupSyncIds.get(i));
                assumeNonNull(savedTabGroup);
                addSavedTabGroupInfoToModel(savedTabGroup, i);
            }
        }

        return false;
    }

    /**
     * Toggles the collapsed/expanded state of a tab group inside the TabModel.
     *
     * @param tabId The ID of the representative tab of the tab group.
     */
    public void toggleTabGroupExpansion(int tabId) {
        TabModel tabModel = getCurrentTabModelChecked();
        Tab tab = tabModel.getTabById(tabId);
        if (tab == null) return;
        Token tabGroupId = tab.getTabGroupId();
        if (tabGroupId == null) return;

        boolean isCollapsed = tabModel.getTabGroupCollapsed(tabGroupId);
        boolean newCollapsedState = !isCollapsed;

        tabModel.setTabGroupCollapsed(tabGroupId, newCollapsedState, /* animate= */ false);

        if (mComponentId == TabComponentId.VERTICAL_TABS) {
            RecordHistogram.recordBooleanHistogram(
                    "Android.VerticalTabs.TabGroupCollapsed", newCollapsedState);
        }
    }

    void postHiding() {
        removeObservers(mCurrentTabModelSupplier.get());
        mShowingTabs = false;
        // if tab was marked for add later, add to model and mark as selected.
        if (mTabToAddDelayed != null) {
            int index = mTabListLayoutDelegate.onTabAdded(mTabToAddDelayed);
            selectTab(mLastSelectedTabListModelIndex, index);
            mTabToAddDelayed = null;
        }
        mTabGridItemTouchHelperCallback.clearCardState();
    }

    private boolean isSelectedTab(Tab tab, int tabModelSelectedTabId) {
        SelectionDelegate<TabListEditorItemSelectionId> selectionDelegate =
                getTabSelectionDelegate();
        if (selectionDelegate == null) {
            return tab.getId() == tabModelSelectedTabId;
        } else {
            return selectionDelegate.isItemSelected(
                    TabListEditorItemSelectionId.createTabId(tab.getId()));
        }
    }

    /**
     * @see TabSwitcherMediator.ResetHandler#softCleanup
     */
    void softCleanup() {
        assert !mShowingTabs;
        for (int i = 0; i < mModelList.size(); i++) {
            PropertyModel model = mModelList.get(i).model;
            if (TabProperties.isTabOrTabGroup(model)) {
                updateThumbnailFetcher(model, Tab.INVALID_TAB_ID);
                model.set(TabProperties.FAVICON_FETCHER, null);
            }
        }
    }

    void updateTab(int index, Tab tab, boolean isUpdatingId, boolean quickMode) {
        if (!mModelList.isValidIndex(index)) return;

        updateTab(mModelList.get(index).model, index, tab, isUpdatingId, quickMode);
    }

    void updateTab(
            PropertyModel model, int index, Tab tab, boolean isUpdatingId, boolean quickMode) {
        if (isUpdatingId) {
            model.set(TabProperties.TAB_ID, tab.getId());
        } else {
            // Group Header's TAB_ID is not required to match the active child's ID.
            assert TabProperties.isTabGroupHeader(model)
                    || TabProperties.getTabId(model) == tab.getId();
        }

        boolean isTabSelected = isTabSelected(tab, model, mTabActionState);
        boolean isInTabGroup = isTabInTabGroup(tab);
        @TabGroupColorId int tabGroupColorId = TabGroupColorId.GREY;
        // Only update the color if the tab is a representation of a tab group, otherwise
        // hide the icon by setting the color to INVALID.
        if (isInTabGroup) {
            Token tabGroupId = tab.getTabGroupId();
            assumeNonNull(tabGroupId);
            TabModel tabModel = getCurrentTabModelChecked();
            tabGroupColorId = tabModel.getTabGroupColorWithFallback(tabGroupId);
        }

        updateTabGroupProperties(tab, model, tabGroupColorId);
        model.set(TabProperties.TAB_CLICK_LISTENER, getTabActionListener(tab, isInTabGroup));
        model.set(TabProperties.IS_SELECTED, isTabSelected);
        model.set(TabProperties.SHOULD_SHOW_PRICE_DROP_TOOLTIP, false);
        model.set(
                TabProperties.TITLE,
                getLatestTitleForTabOrGroup(tab, model, /* useDefault= */ true));
        model.set(TabProperties.IS_PINNED, tab.getIsPinned());
        @TabAlert int alertState = getTabGridAlertState(tab, model);
        model.set(TabProperties.ALERT_STATE, alertState);
        if (model.containsKey(TabProperties.MEDIA_INDICATOR)) {
            model.set(TabProperties.MEDIA_INDICATOR, TabUtils.getMediaStateForAlert(alertState));
        }

        bindTabActionStateProperties(model.get(TabProperties.TAB_ACTION_STATE), tab, model);

        model.set(TabProperties.URL_DOMAIN, getDomainForTab(tab, model));

        setupPersistedTabDataFetcherForTab(tab, model);

        updateFaviconForTab(model, tab, null, null);

        ActorUiTabController controller = ActorUiTabController.from(tab);
        updateActorUiState(model, controller == null ? null : controller.getUiTabState());

        boolean forceUpdate = isTabSelected && !quickMode;
        boolean forceUpdateLastSelected =
                mTabListLayoutDelegate.requiresThumbnailUpdateOnDeselect()
                        && index == mLastSelectedTabListModelIndex
                        && !quickMode;
        // TODO(crbug.com/40273706): Fetching thumbnail for group is expensive, we should consider
        // to improve it.
        if (mThumbnailProvider != null
                && mShowingTabs
                && (model.get(THUMBNAIL_FETCHER) == null
                        || forceUpdate
                        || isUpdatingId
                        || forceUpdateLastSelected
                        || isInTabGroup)) {
            updateThumbnailFetcher(model, tab.getId());
        }
    }

    private void updateActorUiState(PropertyModel model, @Nullable UiTabState state) {
        boolean isTabGroupCard = TabProperties.isTabGroupHeader(model);
        model.set(
                TabProperties.ACTOR_UI_STATE,
                (isTabGroupCard || state == null || state.tabIndicator == TabIndicatorStatus.NONE)
                        ? null
                        : state);
    }

    boolean isTabInTabGroup(Tab tab) {
        TabModel tabModel = getCurrentTabModelChecked();
        assert tabModel.isTabModelRestored();

        return tabModel.isTabInTabGroup(tab);
    }

    private @TabAlert int getTabGridAlertState(Tab representativeTab, PropertyModel model) {
        if (!TabProperties.isTabOrTabGroup(model)) return TabAlert.NONE;

        return mTabListLayoutDelegate.getAlertState(representativeTab, model);
    }

    /**
     * @return The callback that hosts the logic for swipe and drag related actions.
     */
    ItemTouchHelper2.SimpleCallback getItemTouchHelperCallback(
            final float swipeToDismissThreshold,
            final float mergeThreshold,
            final float ungroupThreshold) {
        mTabGridItemTouchHelperCallback.setupCallback(
                swipeToDismissThreshold, mergeThreshold, ungroupThreshold);
        return mTabGridItemTouchHelperCallback;
    }

    void registerOrientationListener(GridLayoutManager manager) {
        mComponentCallbacks =
                new ComponentCallbacks() {
                    @Override
                    public void onConfigurationChanged(Configuration newConfig) {
                        updateSpanCount(manager, newConfig.screenWidthDp);
                        if (mTabListConfig.supportsMessageCards
                                && mTabActionState != TabActionState.SELECTABLE) {
                            updateLayout();
                        }
                    }

                    @Override
                    public void onLowMemory() {}
                };
        mActivity.registerComponentCallbacks(mComponentCallbacks);
        mGridLayoutManager = manager;
    }

    /**
     * Update the grid layout span count and span size lookup base on orientation.
     * @param manager     The {@link GridLayoutManager} used to update the span count.
     * @param screenWidthDp The screnWidth based on which we update the span count.
     * @return whether the span count changed.
     */
    boolean updateSpanCount(GridLayoutManager manager, int screenWidthDp) {
        final int oldSpanCount = manager.getSpanCount();
        final int newSpanCount = getSpanCount(screenWidthDp);
        manager.setSpanCount(newSpanCount);
        manager.setSpanSizeLookup(
                new GridLayoutManager.SpanSizeLookup() {
                    @Override
                    public int getSpanSize(int position) {
                        return getSpanCountForItem(manager, position);
                    }
                });
        mCurrentSpanCount = newSpanCount;
        return oldSpanCount != newSpanCount;
    }

    int getCurrentSpanCount() {
        return mCurrentSpanCount;
    }

    /**
     * Span count is computed based on screen width for tablets and orientation for phones. When in
     * multi-window mode on phone, the span count is fixed to 2 to keep tab card size reasonable.
     */
    @VisibleForTesting
    int getSpanCount(int screenWidthDp) {
        if (DeviceInfo.isXr()) {
            // The layout span count is restricted to medium on XR immersive devices to display
            // larger tab thumbnails, despite the large screen width.
            return TabListCoordinator.GRID_LAYOUT_SPAN_COUNT_MEDIUM;
        }
        if (DeviceFormFactor.isNonMultiDisplayContextOnTablet(mActivity)) {
            return screenWidthDp < TabListCoordinator.MAX_SCREEN_WIDTH_COMPACT_DP
                    ? TabListCoordinator.GRID_LAYOUT_SPAN_COUNT_COMPACT
                    : screenWidthDp < TabListCoordinator.MAX_SCREEN_WIDTH_MEDIUM_DP
                            ? TabListCoordinator.GRID_LAYOUT_SPAN_COUNT_MEDIUM
                            : TabListCoordinator.GRID_LAYOUT_SPAN_COUNT_LARGE;
        }
        return screenWidthDp < TabListCoordinator.MAX_SCREEN_WIDTH_COMPACT_DP
                ? TabListCoordinator.GRID_LAYOUT_SPAN_COUNT_COMPACT
                : TabListCoordinator.GRID_LAYOUT_SPAN_COUNT_MEDIUM;
    }

    /**
     * Sets up the {@link View.AccessibilityDelegate} for tab list accessibility actions.
     *
     * @param helper The {@link TabGridAccessibilityHelper} used to setup accessibility support.
     */
    @Initializer
    public void setupAccessibilityDelegate(TabGridAccessibilityHelper helper) {
        mTabListLayoutDelegate.setAccessibilityHelper(helper);
        mAccessibilityDelegate =
                new View.AccessibilityDelegate() {
                    @Override
                    public void onInitializeAccessibilityNodeInfo(
                            View host, AccessibilityNodeInfo info) {
                        super.onInitializeAccessibilityNodeInfo(host, info);
                        Context context = host.getContext();
                        PropertyModel model = getModelForView(host);

                        // 1. Layout-specific accessibility info and reorder actions.
                        mTabListLayoutDelegate.populateAccessibilityNodeInfo(host, info, model);

                        // 2. Context menu actions.
                        info.addAction(AccessibilityAction.ACTION_LONG_CLICK);
                        if (context != null
                                && model != null
                                && TabProperties.isTabGroupHeader(model)) {
                            String groupTitle = model.get(TabProperties.TITLE);
                            if (TextUtils.isEmpty(groupTitle)) {
                                Token groupId = model.get(TabProperties.TAB_GROUP_HEADER_ID);
                                TabModel tabModel = mCurrentTabModelSupplier.get();
                                if (groupId != null && tabModel != null) {
                                    groupTitle =
                                            TabGroupTitleUtils.getDisplayableTitle(
                                                    context, tabModel, groupId);
                                }
                            }
                            if (groupTitle == null) {
                                groupTitle = "";
                            }
                            info.addAction(
                                    new AccessibilityAction(
                                            R.id.tab_context_menu,
                                            context.getString(
                                                    R.string.tab_group_menu_accessibility_text,
                                                    groupTitle)));
                        }
                    }

                    @Override
                    public boolean performAccessibilityAction(
                            View host, int action, @Nullable Bundle args) {
                        PropertyModel model = getModelForView(host);
                        if (mTabListLayoutDelegate.performAccessibilityAction(
                                host, action, args, model)) {
                            return true;
                        }

                        if (action == R.id.tab_context_menu
                                || action == AccessibilityAction.ACTION_LONG_CLICK.getId()
                                || action == AccessibilityAction.ACTION_CONTEXT_CLICK.getId()) {
                            if (mOnLongPressTabItemEventListener != null) {
                                int tabId =
                                        model != null
                                                ? TabProperties.getTabId(model)
                                                : Tab.INVALID_TAB_ID;
                                mOnLongPressTabItemEventListener.onLongPressEvent(tabId, host);
                                return true;
                            }
                        }

                        return super.performAccessibilityAction(host, action, args);
                    }
                };
    }

    private @Nullable PropertyModel getModelForView(View host) {
        if (host.getParent() instanceof RecyclerView rv) {
            RecyclerView.ViewHolder vh = rv.getChildViewHolder(host);
            if (vh instanceof SimpleRecyclerViewAdapter.ViewHolder simpleVh) {
                return simpleVh.model;
            } else {
                int pos = rv.getChildAdapterPosition(host);
                if (mModelList.isValidIndex(pos)) {
                    return mModelList.get(pos).model;
                }
            }
        }
        return null;
    }

    /** Destroy any members that needs clean up. */
    public void destroy() {
        if (mListObserver != null) {
            mModelList.removeObserver(mListObserver);
        }
        removeObservers(mCurrentTabModelSupplier.get());
        mCurrentTabModelSupplier.removeObserver(mOnTabModelChanged);

        if (mComponentCallbacks != null) {
            mActivity.unregisterComponentCallbacks(mComponentCallbacks);
        }

        if (mRailCollapseStateSupplier != null && mRailCollapseStateObserver != null) {
            mRailCollapseStateSupplier.removeObserver(mRailCollapseStateObserver);
        }

        if (mTabUnderlineManager != null) {
            mTabUnderlineManager.removeObserver(mTabUnderlineObserver);
        }

        mTabClosedFrom.clear();
        mObserverManager.destroy();
    }

    void setTabActionState(@TabActionState int tabActionState) {
        if (mTabActionState == tabActionState) return;
        mTabActionState = tabActionState;
        assumeNonNull(getTabSelectionDelegate()).clearSelection();

        for (int i = 0; i < mModelList.size(); i++) {
            ListItem item = mModelList.get(i);
            if (item.type != UiType.TAB && item.type != UiType.TAB_GROUP) continue;
            // Unbind the current TabActionState properties.
            PropertyModel model = item.model;
            unbindTabActionStateProperties(model);

            model.set(TabProperties.TAB_ACTION_STATE, mTabActionState);
            if (item.type == UiType.TAB) {
                Tab tab = getTabForIndex(i);
                assumeNonNull(tab);
                bindTabActionStateProperties(tabActionState, tab, model);
            } else if (item.type == UiType.TAB_GROUP) {
                if (model.get(CARD_TYPE) == ModelType.ARCHIVED_TAB_GROUP) {
                    assumeNonNull(mTabGroupSyncService);
                    SavedTabGroup savedTabGroup =
                            mTabGroupSyncService.getGroup(
                                    model.get(TabProperties.TAB_GROUP_SYNC_ID));
                    if (savedTabGroup != null) {
                        bindTabGroupActionStateProperties(savedTabGroup, model);
                    }
                }
            } else {
                assert false : "Unexpected itemId type.";
            }
        }
    }

    private void unbindTabActionStateProperties(PropertyModel model) {
        model.set(TabProperties.IS_SELECTED, false);
        for (WritableObjectPropertyKey<?> propertyKey :
                TabProperties.TAB_ACTION_STATE_OBJECT_KEYS) {
            model.set(propertyKey, null);
        }
    }

    private @Nullable TabActionButtonData getTabActionButtonData(
            Tab tab, PropertyModel model, @TabActionState int tabActionState) {
        if (tabActionState == TabActionState.SELECTABLE) {
            return new TabActionButtonData(
                    TabActionButtonType.SELECT, mSelectableTabOnClickListener);
        }
        if (TabProperties.isTabGroupHeader(model)) {
            if (mTabListItemOnClickListenerProvider != null) {
                return mTabListItemOnClickListenerProvider.getTabGroupActionButtonData(
                        tab, model, this::getTabGroupOverflowMenuClickListener);
            }
            return new TabActionButtonData(
                    TabActionButtonType.OVERFLOW, getTabGroupOverflowMenuClickListener());
        }

        if (tab.getIsPinned()) {
            return new TabActionButtonData(TabActionButtonType.PIN, /* tabActionListener= */ null);
        }

        return new TabActionButtonData(TabActionButtonType.CLOSE, mTabClosedListener);
    }

    private TabActionListener getTabGroupOverflowMenuClickListener() {
        if (mTabListGroupMenuCoordinator == null) {
            TabModel tabModel = getCurrentTabModelChecked();
            boolean isIncognito = tabModel.isIncognitoBranded();
            TabGroupSyncService tabGroupSyncService = isIncognito ? null : mTabGroupSyncService;
            assert mCollaborationService != null;
            CollaborationService collaborationService =
                    isIncognito
                            ? CollaborationServiceFactory.getForProfile(
                                    assumeNonNull(tabModel.getProfile()))
                            : mCollaborationService;
            mTabListGroupMenuCoordinator =
                    new TabListGroupMenuCoordinator(
                            mOnMenuItemClickedCallback,
                            this::getCurrentTabModelChecked,
                            tabGroupSyncService,
                            collaborationService,
                            mActivity);
        }
        return mTabListGroupMenuCoordinator.getTabActionListener();
    }

    private @Nullable TabActionListener getTabClickListener(
            Tab tab, PropertyModel model, @TabActionState int tabActionState) {
        if (tabActionState == TabActionState.SELECTABLE) {
            return mSelectableTabOnClickListener;
        } else {
            if (TabProperties.isTabGroupHeader(model)
                    && mTabListItemOnClickListenerProvider != null) {
                return mTabListItemOnClickListenerProvider.onTabGroupClicked(tab);
            } else {
                return mTabSelectedListener;
            }
        }
    }

    /** Returns the coordinator that manages the overflow menu for tab group cards in the GTS. */
    public @Nullable TabListGroupMenuCoordinator getTabListGroupMenuCoordinator() {
        return mTabListGroupMenuCoordinator;
    }

    private @Nullable TabActionListener getTabLongClickListener(
            @TabActionState int tabActionState) {
        return tabActionState == TabActionState.SELECTABLE ? mSelectableTabOnClickListener : null;
    }

    private @Nullable TabActionListener getTabContextClickListener(
            @TabActionState int tabActionState) {
        if (!mTabListConfig.supportsTabContextClick) {
            return null;
        }
        return tabActionState != TabActionState.SELECTABLE
                ? mContextClickTabItemEventListener
                : null;
    }

    @TabActionState
    int getTabActionState() {
        return mTabActionState;
    }

    @TabComponentId
    int getComponentId() {
        return mComponentId;
    }

    void bindTabActionStateProperties(
            @TabActionState int tabActionState, Tab tab, PropertyModel model) {
        model.set(TabProperties.IS_SELECTED, isTabSelected(tab, model, tabActionState));

        model.set(
                TabProperties.TAB_ACTION_BUTTON_DATA,
                getTabActionButtonData(tab, model, tabActionState));
        model.set(
                TabProperties.TAB_CLICK_LISTENER, getTabClickListener(tab, model, tabActionState));
        model.set(TabProperties.TAB_LONG_CLICK_LISTENER, getTabLongClickListener(tabActionState));
        model.set(
                TabProperties.TAB_CONTEXT_CLICK_LISTENER,
                getTabContextClickListener(tabActionState));
        model.set(TabProperties.TAB_HOVER_CARD_LISTENER, mTabListConfig.tabHoverCardListener);

        if (mTabActionState != TabActionState.SELECTABLE) {
            updateDescriptionString(model);
            updateActionButtonDescriptionString(tab, model);
        }
    }

    private void bindTabGroupActionStateProperties(
            SavedTabGroup savedTabGroup, PropertyModel model) {
        boolean isSelectableState = mTabActionState == TabActionState.SELECTABLE;

        TabActionButtonData tabActionButtonData =
                isSelectableState
                        ? new TabActionButtonData(
                                TabActionButtonType.SELECT, mSelectableTabOnClickListener)
                        : new TabActionButtonData(TabActionButtonType.CLOSE, mTabClosedListener);
        assumeNonNull(mTabListItemOnClickListenerProvider);
        TabActionListener tabClickListener =
                isSelectableState
                        ? mSelectableTabOnClickListener
                        : mTabListItemOnClickListenerProvider.onTabGroupClicked(
                                assumeNonNull(savedTabGroup.syncId));
        TabActionListener tabLongClickListener =
                isSelectableState ? mSelectableTabOnClickListener : null;

        model.set(TabProperties.TAB_ACTION_BUTTON_DATA, tabActionButtonData);
        model.set(TabProperties.TAB_CLICK_LISTENER, tabClickListener);
        model.set(TabProperties.TAB_LONG_CLICK_LISTENER, tabLongClickListener);

        if (mTabActionState != TabActionState.SELECTABLE) {
            updateTabGroupDescriptionString(savedTabGroup, model);
            updateTabGroupActionButtonDescriptionString(savedTabGroup, model);
        }
    }

    private TabActionListener getTabActionListener(Tab tab, boolean isInTabGroup) {
        TabActionListener tabSelectedListener;
        if (mTabListItemOnClickListenerProvider == null
                || !isInTabGroup
                || !mTabListLayoutDelegate.supportsTabGroups()) {
            tabSelectedListener = mTabSelectedListener;
        } else {
            tabSelectedListener = mTabListItemOnClickListenerProvider.onTabGroupClicked(tab);
            if (tabSelectedListener == null) {
                tabSelectedListener = mTabSelectedListener;
            }
        }
        return tabSelectedListener;
    }

    private void handleTabSelection(int tabId) {
        if (mTabListItemOnClickListenerProvider != null) {
            mTabListItemOnClickListenerProvider.onTabSelecting(tabId);
        } else {
            TabModel tabModel = getCurrentTabModelChecked();
            tabModel.setIndex(
                    TabModelUtils.getTabIndexById(tabModel, tabId), TabSelectionType.FROM_USER);
        }
    }

    private boolean isTabSelected(
            Tab tab, PropertyModel model, @TabActionState int tabActionState) {
        if (tabActionState == TabActionState.SELECTABLE) {
            SelectionDelegate<TabListEditorItemSelectionId> selectionDelegate =
                    getTabSelectionDelegate();
            assert selectionDelegate != null : "Null selection delegate while in SELECTABLE state.";
            return selectionDelegate.isItemSelected(
                    TabListEditorItemSelectionId.createTabId(tab.getId()));
        } else {
            TabModel tabModel = getCurrentTabModelChecked();
            // Check if the tab group card should show as selected. The click listener provider
            // can override this behavior.
            if (TabProperties.isTabGroupHeader(model) && tab.getTabGroupId() != null) {
                if (mTabListItemOnClickListenerProvider != null) {
                    @Nullable Boolean selectedOverride =
                            mTabListItemOnClickListenerProvider.isTabGroupSelected(tab, model);
                    if (selectedOverride != null) {
                        return selectedOverride;
                    }
                }

                List<Tab> relatedTabs = getRelatedTabsForId(tab.getId());
                boolean isSelected = false;
                for (Tab relatedTab : relatedTabs) {
                    isSelected |= relatedTab == TabModelUtils.getCurrentTab(tabModel);
                }
                return isSelected;
            } else {
                return TabModelUtils.getCurrentTabId(tabModel) == tab.getId();
            }
        }
    }

    /**
     * Constructs and inserts the UI property model for a newly added tab at the specified index. If
     * the tab belongs to a tab group (in non-FLAT layouts), delegates to {@link
     * #addTabInfoToModelForGroup}; otherwise delegates to {@link #addTabInfoToModelForTab} as an
     * individual tab card.
     *
     * @param tab The {@link Tab} to add to the model list.
     * @param index The UI index in {@link #mModelList} where the card should be inserted.
     */
    void addTabCardToModel(Tab tab, int index) {
        boolean isTabGroup = isTabInTabGroup(tab) && mTabListLayoutDelegate.supportsTabGroups();
        if (isTabGroup) {
            Token tabGroupId = tab.getTabGroupId();
            assumeNonNull(tabGroupId);
            addTabInfoToModelForGroup(tab, tabGroupId, index);
        } else {
            TabModel tabModel = getCurrentTabModelChecked();
            int currentTabId = TabModelUtils.getCurrentTabId(tabModel);
            addTabInfoToModelForTab(tab, index, currentTabId == tab.getId());
        }
    }

    private PropertyModel addTabInfoToModel(
            Tab tab, int index, boolean isSelected, @CardProperties.ModelType int cardType) {
        PropertyModel tabInfo =
                new PropertyModel.Builder(TabProperties.ALL_KEYS_TAB_GRID)
                        .with(TabProperties.TAB_ACTION_STATE, mTabActionState)
                        .with(TabProperties.TAB_ID, tab.getId())
                        .with(TabProperties.IS_INCOGNITO, tab.isIncognito())
                        .with(TabProperties.FAVICON_FETCHER, null)
                        .with(TabProperties.FAVICON_FETCHED, false)
                        .with(TabProperties.IS_SELECTED, isSelected)
                        .with(CARD_ALPHA, 1f)
                        .with(CardProperties.CARD_ANIMATION_STATUS, AnimationStatus.CARD_RESTORE)
                        .with(TabProperties.TAB_SELECTION_DELEGATE, getTabSelectionDelegate())
                        .with(TabProperties.ACCESSIBILITY_DELEGATE, mAccessibilityDelegate)
                        .with(CARD_TYPE, cardType)
                        .with(TabProperties.VISIBILITY, View.VISIBLE)
                        .with(TabProperties.ACTOR_UI_STATE, null)
                        .with(TabProperties.IS_GLIC_ACTIVE, false)
                        .with(TabProperties.IS_PINNED, tab.getIsPinned())
                        .with(TabProperties.ALERT_STATE, TabAlert.NONE)
                        .build();

        ActorUiTabController controller = ActorUiTabController.from(tab);
        updateActorUiState(tabInfo, controller == null ? null : controller.getUiTabState());

        if (mRailCollapseStateSupplier != null) {
            tabInfo.set(TabProperties.RAIL_COLLAPSE_STATE, mRailCollapseStateSupplier.get());
        }

        @UiType int tabUiType = mTabListConfig.tabUiType;
        if (index >= mModelList.size()) {
            mModelList.add(new ListItem(tabUiType, tabInfo));
        } else {
            mModelList.add(index, new ListItem(tabUiType, tabInfo));
        }

        if (mThumbnailProvider != null && mDefaultGridCardSize != null) {
            if (!mDefaultGridCardSize.equals(tabInfo.get(TabProperties.GRID_CARD_SIZE))) {
                tabInfo.set(
                        TabProperties.GRID_CARD_SIZE,
                        new Size(
                                mDefaultGridCardSize.getWidth(), mDefaultGridCardSize.getHeight()));
            }
        }

        return tabInfo;
    }

    /**
     * Builds and inserts a {@link PropertyModel} for an individual tab card at the given UI index,
     * binding metadata fetchers (favicon, thumbnail, persisted tab data) and applying
     * layout-specific child properties.
     *
     * @param tab The {@link Tab} to represent in the model list.
     * @param index The target UI index where the tab card model will be inserted.
     * @param isSelected Whether the tab card should visually indicate that it is currently active.
     */
    void addTabInfoToModelForTab(Tab tab, int index, boolean isSelected) {
        assert index != TabModel.INVALID_TAB_INDEX;

        PropertyModel tabInfo = addTabInfoToModel(tab, index, isSelected, ModelType.TAB);

        mTabListLayoutDelegate.setupGroupPropertiesForChildTab(tab, tabInfo);
        tabInfo.set(
                TabProperties.TITLE,
                getLatestTitleForTabOrGroup(tab, tabInfo, /* useDefault= */ false));
        tabInfo.set(TabProperties.URL_DOMAIN, getDomainForTab(tab, tabInfo));
        @TabAlert int alertState = getTabGridAlertState(tab, tabInfo);
        tabInfo.set(TabProperties.ALERT_STATE, alertState);
        if (tabInfo.containsKey(TabProperties.MEDIA_INDICATOR)) {
            tabInfo.set(TabProperties.MEDIA_INDICATOR, TabUtils.getMediaStateForAlert(alertState));
        }
        tabInfo.set(TabProperties.SHOULD_SHOW_PRICE_DROP_TOOLTIP, false);
        tabInfo.set(TabProperties.USE_SHRINK_CLOSE_ANIMATION, false);
        tabInfo.set(
                TabProperties.QUICK_DELETE_ANIMATION_STATUS,
                QuickDeleteAnimationStatus.TAB_RESTORE);
        tabInfo.set(
                TabProperties.FAVICON_FETCHER,
                mTabListFaviconProvider.getDefaultFaviconFetcher(tab.isIncognito()));
        tabInfo.set(TabProperties.IS_LOADING, false);

        setupPersistedTabDataFetcherForTab(tab, tabInfo);

        updateFaviconForTab(tabInfo, tab, null, null);

        bindTabActionStateProperties(mTabActionState, tab, tabInfo);

        if (mShowingTabs) {
            updateThumbnailFetcher(tabInfo, tab.getId());
        }
    }

    /**
     * Builds and inserts a {@link PropertyModel} for a tab group card or header at the given UI
     * index. Configures group properties including color, title fallback, collapsed state, and sets
     * selection if the group is collapsed and contains the active tab.
     *
     * @param tab A representative {@link Tab} for the group.
     * @param tabGroupId The {@link Token} identifying the tab group.
     * @param index The target UI index where the group card or header will be inserted.
     */
    void addTabInfoToModelForGroup(Tab tab, Token tabGroupId, int index) {
        assert index != TabModel.INVALID_TAB_INDEX;
        assumeNonNull(tabGroupId);
        TabModel tabModel = getCurrentTabModelChecked();
        @TabGroupColorId int colorId = tabModel.getTabGroupColorWithFallback(tabGroupId);
        int currentTabId = TabModelUtils.getCurrentTabId(tabModel);

        boolean isCollapsed = mTabListLayoutDelegate.isGroupCollapsed(tabGroupId);
        // If the group is collapsed, the group representation card displays the selection.
        // If expanded, the group card is a header and should remain unhighlighted (child rows show
        // selection).
        boolean isSelected = isCollapsed && isSelectedTab(tab, currentTabId);

        int cardType = mTabListLayoutDelegate.getGroupCardType();
        PropertyModel groupInfo = addTabInfoToModel(tab, index, isSelected, cardType);

        // Group Header Specific properties
        groupInfo.set(TabProperties.TAB_GROUP_ID, null);
        updateTabGroupProperties(tab, groupInfo, colorId);
        groupInfo.set(
                TabProperties.TITLE,
                getLatestTitleForTabOrGroup(tab, groupInfo, /* useDefault= */ true));
        groupInfo.set(TabProperties.IS_COLLAPSED, isCollapsed);
        groupInfo.set(TabProperties.FAVICON_FETCHER, null);
        @TabAlert int alertState = getTabGridAlertState(tab, groupInfo);
        groupInfo.set(TabProperties.ALERT_STATE, alertState);
        if (groupInfo.containsKey(TabProperties.MEDIA_INDICATOR)) {
            groupInfo.set(
                    TabProperties.MEDIA_INDICATOR, TabUtils.getMediaStateForAlert(alertState));
        }

        bindTabActionStateProperties(mTabActionState, tab, groupInfo);

        if (mShowingTabs) {
            updateThumbnailFetcher(groupInfo, tab.getId());
        }
    }

    private void addSavedTabGroupInfoToModel(SavedTabGroup savedTabGroup, int index) {
        assert savedTabGroup != null;
        String title =
                TextUtils.isEmpty(savedTabGroup.title)
                        ? TabGroupTitleUtils.getDefaultTitle(
                                mActivity, savedTabGroup.savedTabs.size())
                        : savedTabGroup.title;

        int cardType =
                savedTabGroup.archivalTimeMs != null
                        ? ModelType.ARCHIVED_TAB_GROUP
                        : ModelType.TAB_GROUP;
        PropertyModel tabGroupInfo =
                new PropertyModel.Builder(TabProperties.ALL_KEYS_TAB_GROUP_GRID)
                        .with(TabProperties.TAB_ACTION_STATE, mTabActionState)
                        .with(TabProperties.TAB_GROUP_SYNC_ID, savedTabGroup.syncId)
                        .with(TabProperties.TITLE, title)
                        .with(TabProperties.IS_INCOGNITO, false)
                        .with(TabProperties.FAVICON_FETCHER, null)
                        .with(TabProperties.IS_SELECTED, false)
                        .with(CARD_ALPHA, 1f)
                        .with(CardProperties.CARD_ANIMATION_STATUS, AnimationStatus.CARD_RESTORE)
                        .with(TabProperties.TAB_SELECTION_DELEGATE, getTabSelectionDelegate())
                        .with(TabProperties.ACCESSIBILITY_DELEGATE, mAccessibilityDelegate)
                        .with(CARD_TYPE, cardType)
                        .with(
                                TabProperties.QUICK_DELETE_ANIMATION_STATUS,
                                QuickDeleteAnimationStatus.TAB_RESTORE)
                        .with(TabProperties.VISIBILITY, View.VISIBLE)
                        .with(TabProperties.USE_SHRINK_CLOSE_ANIMATION, false)
                        .with(TabProperties.ALERT_STATE, TabAlert.NONE)
                        .build();

        bindTabGroupActionStateProperties(savedTabGroup, tabGroupInfo);

        mModelList.add(index, new ListItem(UiType.TAB_GROUP, tabGroupInfo));

        String syncId = savedTabGroup.syncId;
        assumeNonNull(syncId);
        updateTabGroupColorViewProvider(
                EitherGroupId.createSyncId(syncId), tabGroupInfo, savedTabGroup.color);
        assumeNonNull(mDefaultGridCardSize);
        tabGroupInfo.set(
                TabProperties.GRID_CARD_SIZE,
                new Size(mDefaultGridCardSize.getWidth(), mDefaultGridCardSize.getHeight()));

        updateThumbnailFetcher(tabGroupInfo, savedTabGroup);
    }

    /**
     * Inserts the child tabs of a group into the list model.
     *
     * @param tabGroupId The ID of the tab group.
     * @param headerIndex The index of the group header card.
     * @return The number of child tabs added to the list.
     */
    int insertChildTabs(Token tabGroupId, int headerIndex) {
        TabModel tabModel = getCurrentTabModelChecked();
        List<Tab> children = tabModel.getTabsInGroup(tabGroupId);
        int currentTabId = TabModelUtils.getCurrentTabId(tabModel);
        for (int i = 0; i < children.size(); i++) {
            Tab childTab = children.get(i);
            int childIndex = headerIndex + 1 + i;
            addTabInfoToModelForTab(childTab, childIndex, isSelectedTab(childTab, currentTabId));
        }
        return children.size();
    }

    String getDomainForTab(Tab tab, PropertyModel model) {
        if (!TabProperties.isTabGroupHeader(model)) return getDomain(tab);
        List<Tab> relatedTabs = getRelatedTabsForId(tab.getId());

        List<String> domainNames = new ArrayList<>();

        for (int i = 0; i < relatedTabs.size(); i++) {
            String domain = getDomain(relatedTabs.get(i));
            domainNames.add(domain);
        }
        // TODO(crbug.com/40107640): Address i18n issue for the list delimiter.
        return TextUtils.join(", ", domainNames);
    }

    /**
     * Updates the accessibility content description string resolver for the given tab or group
     * card.
     *
     * @param model The {@link PropertyModel} representing the tab or group card.
     */
    void updateDescriptionString(PropertyModel model) {
        if (!mTabListLayoutDelegate.supportsTabGroups()) return;
        TextResolver contentDescriptionResolver =
                (context) -> {
                    boolean isTabGroup = TabProperties.isTabGroupHeader(model);
                    TabModel tabModel = getCurrentTabModelChecked();
                    int tabId = model.get(TabProperties.TAB_ID);
                    Tab currentTab = tabModel.getTabById(tabId);
                    if (currentTab == null) return "";
                    int numOfRelatedTabs = getRelatedTabsForId(tabId).size();
                    if (!isTabGroup) {
                        if (mComponentId == TabComponentId.ARCHIVED_TABS_DIALOG) {
                            return context.getString(
                                    R.string.accessibility_restore_tab,
                                    getTabTitleOrUrl(currentTab));
                        }
                        return "";
                    }
                    String title =
                            getLatestTitleForTabOrGroup(currentTab, model, /* useDefault= */ false);
                    Resources res = context.getResources();
                    @TabGroupColorId
                    int colorId =
                            tabModel.getTabGroupColorWithFallback(
                                    assumeNonNull(currentTab.getTabGroupId()));
                    final @StringRes int colorDescRes =
                            TabGroupColorPickerUtils
                                    .getTabGroupColorPickerItemColorAccessibilityString(colorId);
                    String colorDesc = res.getString(colorDescRes);
                    String description;
                    if (!TabProperties.isTabGroupCollapsed(model)) {
                        description =
                                TextUtils.isEmpty(title)
                                        ? res.getQuantityString(
                                                R.plurals.accessibility_dialog_back_button,
                                                numOfRelatedTabs,
                                                numOfRelatedTabs)
                                        : res.getQuantityString(
                                                R.plurals
                                                        .accessibility_dialog_back_button_with_group_name,
                                                numOfRelatedTabs,
                                                title,
                                                numOfRelatedTabs);
                    } else if (TabUiUtils.isDataSharingFunctionalityEnabled()
                            && hasCollaboration(currentTab)) {
                        TabCardLabelData tabCardLabelData =
                                model.get(TabProperties.TAB_CARD_LABEL_DATA);
                        CharSequence tabCardLabelDesc = "";
                        if (tabCardLabelData != null) {
                            tabCardLabelDesc =
                                    tabCardLabelData.resolveContentDescriptionWithTextFallback(
                                            context);
                        }
                        if (TextUtils.isEmpty(tabCardLabelDesc)) {
                            description =
                                    TextUtils.isEmpty(title)
                                            ? res.getQuantityString(
                                                    R.plurals
                                                            .accessibility_expand_shared_tab_group_with_color,
                                                    numOfRelatedTabs,
                                                    numOfRelatedTabs,
                                                    colorDesc)
                                            : res.getQuantityString(
                                                    R.plurals
                                                            .accessibility_expand_shared_tab_group_with_group_name_with_color,
                                                    numOfRelatedTabs,
                                                    title,
                                                    numOfRelatedTabs,
                                                    colorDesc);
                        } else {
                            description =
                                    TextUtils.isEmpty(title)
                                            ? res.getQuantityString(
                                                    R.plurals
                                                            .accessibility_expand_shared_tab_group_with_color_with_card_label,
                                                    numOfRelatedTabs,
                                                    numOfRelatedTabs,
                                                    colorDesc,
                                                    tabCardLabelDesc)
                                            : res.getQuantityString(
                                                    R.plurals
                                                            .accessibility_expand_shared_tab_group_with_group_name_with_color_with_card_label,
                                                    numOfRelatedTabs,
                                                    title,
                                                    numOfRelatedTabs,
                                                    colorDesc,
                                                    tabCardLabelDesc);
                        }
                    } else {
                        description =
                                TextUtils.isEmpty(title)
                                        ? res.getQuantityString(
                                                R.plurals.accessibility_expand_tab_group_with_color,
                                                numOfRelatedTabs,
                                                numOfRelatedTabs,
                                                colorDesc)
                                        : res.getQuantityString(
                                                R.plurals
                                                        .accessibility_expand_tab_group_with_group_name_with_color,
                                                numOfRelatedTabs,
                                                title,
                                                numOfRelatedTabs,
                                                colorDesc);
                    }
                    String alertStateString = getAlertStateAccessibilityString(model, res);
                    if (!TextUtils.isEmpty(alertStateString)) {
                        description += " " + alertStateString;
                    }
                    return description;
                };
        model.set(TabProperties.CONTENT_DESCRIPTION_TEXT_RESOLVER, contentDescriptionResolver);
    }

    private void updateTabGroupDescriptionString(SavedTabGroup savedTabGroup, PropertyModel model) {
        TextResolver contentDescriptionResolver =
                (context) -> {
                    Resources res = context.getResources();
                    @StringRes
                    int colorDescRes =
                            TabGroupColorPickerUtils
                                    .getTabGroupColorPickerItemColorAccessibilityString(
                                            savedTabGroup.color);
                    String colorDesc = res.getString(colorDescRes);
                    int numOfRelatedTabs = savedTabGroup.savedTabs.size();
                    // The default string to return for now with TabGroup card type and
                    // archivalTimeMs not null, indicating an archived tab group.
                    return TextUtils.isEmpty(savedTabGroup.title)
                            ? res.getQuantityString(
                                    R.plurals.accessibility_restore_tab_group_with_color,
                                    numOfRelatedTabs,
                                    numOfRelatedTabs,
                                    colorDesc)
                            : res.getQuantityString(
                                    R.plurals
                                            .accessibility_restore_tab_group_with_group_name_with_color,
                                    numOfRelatedTabs,
                                    savedTabGroup.title,
                                    numOfRelatedTabs,
                                    colorDesc);
                };
        model.set(TabProperties.CONTENT_DESCRIPTION_TEXT_RESOLVER, contentDescriptionResolver);
    }

    void updateActionButtonDescriptionString(Tab tab, PropertyModel model) {
        if (TabProperties.isTabGroupHeader(model)) {
            int numOfRelatedTabs = getRelatedTabsForId(tab.getId()).size();
            String title = getLatestTitleForTabOrGroup(tab, model, /* useDefault= */ false);

            TextResolver descriptionTextResolver =
                    getActionButtonDescriptionTextResolver(numOfRelatedTabs, title, tab);
            model.set(
                    TabProperties.ACTION_BUTTON_DESCRIPTION_TEXT_RESOLVER, descriptionTextResolver);
            return;
        }

        TextResolver descriptionTextResolver =
                (Context context) ->
                        context.getString(
                                R.string.accessibility_tabstrip_btn_close_tab,
                                getTabTitleOrUrl(tab));
        model.set(TabProperties.ACTION_BUTTON_DESCRIPTION_TEXT_RESOLVER, descriptionTextResolver);
    }

    private void updateTabGroupActionButtonDescriptionString(
            SavedTabGroup savedTabGroup, PropertyModel model) {
        TextResolver descriptionTextResolver =
                (context) -> {
                    Resources res = context.getResources();
                    @StringRes
                    int colorDescRes =
                            TabGroupColorPickerUtils
                                    .getTabGroupColorPickerItemColorAccessibilityString(
                                            savedTabGroup.color);
                    String colorDesc = res.getString(colorDescRes);
                    int numOfRelatedTabs = savedTabGroup.savedTabs.size();
                    // The default string to return for now with TabGroup card type and
                    // archivalTimeMs not null, indicating an archived tab group.
                    return TextUtils.isEmpty(savedTabGroup.title)
                            ? res.getQuantityString(
                                    R.plurals.accessibility_close_tab_group_button_with_color,
                                    numOfRelatedTabs,
                                    numOfRelatedTabs,
                                    colorDesc)
                            : res.getQuantityString(
                                    R.plurals
                                            .accessibility_close_tab_group_button_with_group_name_with_color,
                                    numOfRelatedTabs,
                                    savedTabGroup.title,
                                    numOfRelatedTabs,
                                    colorDesc);
                };
        model.set(TabProperties.ACTION_BUTTON_DESCRIPTION_TEXT_RESOLVER, descriptionTextResolver);
    }

    @VisibleForTesting
    protected static String getDomain(Tab tab) {
        // TODO(crbug.com/40144810) Investigate how uninitialized Tabs are appearing
        // here.
        assert tab.isInitialized();
        if (!tab.isInitialized()) {
            return "";
        }

        String spec = tab.getUrl().getSpec();
        if (spec == null) return "";

        // TODO(crbug.com/40549331): convert UrlUtilities to GURL
        String domain = UrlUtilities.getDomainAndRegistry(spec, false);

        if (domain == null || domain.isEmpty()) return spec;
        return domain;
    }

    @Nullable
    private SelectionDelegate<TabListEditorItemSelectionId> getTabSelectionDelegate() {
        return mSelectionDelegateProvider == null
                ? null
                : mSelectionDelegateProvider.getSelectionDelegate();
    }

    private int getCurrentSelectionCount() {
        var selectionDelegate = getTabSelectionDelegate();
        return selectionDelegate == null ? 0 : selectionDelegate.getSelectedItems().size();
    }

    private String getTabTitleOrUrl(Tab tab) {
        String title = tab.getTitle();
        if (TextUtils.isEmpty(title)) {
            String url = tab.getUrl().getSpec();
            return TextUtils.isEmpty(url) ? "" : url;
        }
        return title;
    }

    /**
     * Returns the latest title for the given tab or its tab group. If the tab is in a group (and
     * the layout supports groups), this returns the title of the tab group. If the tab is a single
     * tab and its title is empty, it falls back to the tab's URL.
     *
     * @param tab The tab to get the title for.
     * @param model The {@link PropertyModel} associated with the tab or tab group. If null, group
     *     status is determined from the tab and layout type.
     * @param useDefault Whether to use a default displayable title (e.g. "2 tabs") if the group
     *     title is empty.
     * @return The latest title for the tab or its group, or the URL fallback.
     */
    @VisibleForTesting
    String getLatestTitleForTabOrGroup(Tab tab, @Nullable PropertyModel model, boolean useDefault) {
        boolean isTabGroup;
        if (model == null) {
            isTabGroup = mTabListLayoutDelegate.supportsTabGroups() && isTabInTabGroup(tab);
        } else {
            isTabGroup = TabProperties.isTabGroupHeader(model);
        }

        if (isTabGroup) {
            Token tabGroupId = tab.getTabGroupId();
            assumeNonNull(tabGroupId);
            TabModel tabModel = getCurrentTabModelChecked();
            if (useDefault) {
                return TabGroupTitleUtils.getDisplayableTitle(mActivity, tabModel, tabGroupId);
            } else {
                return tabModel.getTabGroupTitle(tabGroupId);
            }
        }

        return getTabTitleOrUrl(tab);
    }

    private void setupPersistedTabDataFetcherForTab(Tab tab, PropertyModel model) {
        if (mTabListConfig.supportsMessageCards && !tab.isIncognito()) {
            assert mOriginalProfile != null;
            if (PriceTrackingUtilities.isTrackPricesOnTabsEnabled(mOriginalProfile)
                    && !isTabInTabGroup(tab)) {
                model.set(
                        TabProperties.SHOPPING_PERSISTED_TAB_DATA_FETCHER,
                        new ShoppingPersistedTabDataFetcher(
                                tab, mPriceWelcomeMessageControllerSupplier));
            } else {
                model.set(TabProperties.SHOPPING_PERSISTED_TAB_DATA_FETCHER, null);
            }
        } else {
            model.set(TabProperties.SHOPPING_PERSISTED_TAB_DATA_FETCHER, null);
        }
    }

    void updateFaviconForTab(
            PropertyModel model, Tab tab, @Nullable Bitmap icon, @Nullable GURL iconUrl) {
        if (TabProperties.isTabGroupHeader(model)) {
            model.set(TabProperties.FAVICON_FETCHER, null);
            return;
        }
        if (!mTabListFaviconProvider.isInitialized()) {
            return;
        }

        // If there is an available icon, we fetch favicon synchronously; otherwise asynchronously.
        if (icon != null && iconUrl != null) {
            model.set(
                    TabProperties.FAVICON_FETCHER,
                    mTabListFaviconProvider.getFaviconFromBitmapFetcher(icon, iconUrl));
            return;
        }

        TabFaviconFetcher fetcher = mTabListFaviconProvider.getFaviconForTabFetcher(tab);
        model.set(TabProperties.FAVICON_FETCHER, fetcher);
    }

    /**
     * Returns the default favicon fetcher for the given incognito state.
     *
     * @param isIncognito Whether the tab is incognito.
     * @return The default {@link TabFaviconFetcher}.
     */
    TabFaviconFetcher getDefaultFaviconFetcher(boolean isIncognito) {
        return mTabListFaviconProvider.getDefaultFaviconFetcher(isIncognito);
    }

    /**
     * Inserts a special {@link ListItem} at given index of the current {@link TabListModel}.
     *
     * @param index The index of the {@link ListItem} to be inserted, or TabList.INVALID_TAB_INDEX
     *     to ignore.
     * @param uiType The view type the model will bind to.
     * @param model The model that will be bound to a view.
     */
    void addSpecialItemToModel(int index, @UiType int uiType, PropertyModel model) {
        if (index < 0 || index > mModelList.size()) {
            return;
        }
        mModelList.add(index, new ListItem(uiType, model));
    }

    /**
     * Removes a special {@link ListItem} that has the given {@code uiType} and/or its {@link
     * PropertyModel} has the given {@code itemIdentifier} from the current {@link TabListModel}.
     *
     * @param uiType The uiType to match.
     * @param itemIdentifier The itemIdentifier to match. This can be obsoleted if the {@link
     *     ListItem} does not need additional identifier.
     */
    void removeSpecialItemFromModelList(@UiType int uiType, @MessageType int itemIdentifier) {
        int index = TabModel.INVALID_TAB_INDEX;
        if (isMessageCard(uiType)) {
            if (itemIdentifier == MessageType.ALL) {
                while (mModelList.lastIndexForMessageItem() != TabModel.INVALID_TAB_INDEX) {
                    index = mModelList.lastIndexForMessageItem();
                    mModelList.removeAt(index);
                }
                return;
            }
            index = mModelList.lastIndexForMessageItemFromType(itemIdentifier);
        }

        if (index == TabModel.INVALID_TAB_INDEX) return;

        assert validateItemAt(index, uiType, itemIdentifier);
        mModelList.removeAt(index);
    }

    /**
     * Removes a {@link ListItem} that has the given {@code uiType} and the {@link PropertyModel}
     * has the given {@link TabListEditorItemSelectionId}.
     *
     * @param uiType The uiType to match.
     * @param itemId The itemId to match.
     */
    void removeListItemFromModelList(@UiType int uiType, TabListEditorItemSelectionId itemId) {
        int index = TabModel.INVALID_TAB_INDEX;
        if (uiType == UiType.TAB_GROUP && itemId.isTabGroupSyncId()) {
            String syncId = itemId.getTabGroupSyncId();
            assumeNonNull(syncId);
            index = mModelList.indexFromArchivedTabGroupSyncId(syncId);
        }

        if (index == TabModel.INVALID_TAB_INDEX) return;
        mModelList.removeAt(index);
    }

    /**
     * Retrieves the span count in the GridLayoutManager for the item at a given index.
     *
     * @param manager The GridLayoutManager the span count is retrieved from.
     * @param index The index of the item in the model list.
     */
    int getSpanCountForItem(GridLayoutManager manager, int index) {
        @UiType int itemType = mModelList.get(index).type;

        if (isMessageCard(itemType)) {
            return manager.getSpanCount();
        }
        return 1;
    }

    private boolean validateItemAt(int index, @UiType int uiType, @MessageType int itemIdentifier) {
        PropertyModel model = mModelList.get(index).model;
        return isMessageCard(uiType)
                && mModelList.get(index).type == uiType
                && model.containsKeyEqualTo(MESSAGE_TYPE, itemIdentifier);
    }

    /**
     * The PriceWelcomeMessage should be in view when user enters the tab switcher, so we put it
     * exactly below the currently selected tab.
     *
     * @return Where the PriceWelcomeMessage should be inserted in the {@link TabListModel} when
     *     user enters the tab switcher, or TabList.INVALID_TAB_INDEX if message cards are not
     *     supported.
     */
    int getPriceWelcomeMessageInsertionIndex() {
        if (!mTabListConfig.supportsMessageCards) {
            return TabList.INVALID_TAB_INDEX;
        }
        assert mGridLayoutManager != null;
        int spanCount = mGridLayoutManager.getSpanCount();
        int selectedTabIndex =
                mModelList.indexOfNthTabCard(
                        getCurrentTabModelChecked().getCurrentRepresentativeTabIndex());
        int indexBelowSelectedTab = (selectedTabIndex / spanCount + 1) * spanCount;
        int indexAfterLastTab = mModelList.getTabIndexBefore(mModelList.size()) + 1;
        return Math.min(indexBelowSelectedTab, indexAfterLastTab);
    }

    /**
     * Update the layout of tab switcher to make it compact. Because now we have messages within the
     * tabs like PriceMessage and these messages take up the entire row, some operations like
     * closing a tab above the message card will leave a blank grid, so we need to update the
     * layout.
     */
    @VisibleForTesting
    void updateLayout() {
        // Right now we need to update layout only if there is a price welcome message card in tab
        // switcher.
        if (!mTabListConfig.supportsMessageCards) {
            return;
        }
        if (mOriginalProfile == null
                || !PriceTrackingUtilities.isPriceWelcomeMessageCardEnabled(mOriginalProfile)
                || getCurrentTabModelChecked().isIncognitoBranded()) {
            return;
        }
        assert mGridLayoutManager != null;
        int spanCount = mGridLayoutManager.getSpanCount();
        GridLayoutManager.SpanSizeLookup spanSizeLookup = mGridLayoutManager.getSpanSizeLookup();
        int spanSizeSumForCurrentRow = 0;
        int index = 0;
        for (; index < mModelList.size(); index++) {
            spanSizeSumForCurrentRow += spanSizeLookup.getSpanSize(index);
            if (spanSizeSumForCurrentRow == spanCount) {
                // This row is compact, we clear and recount the spanSize for next row.
                spanSizeSumForCurrentRow = 0;
            } else if (spanSizeSumForCurrentRow > spanCount) {
                // Find a blank grid and break.
                if (isLargeMessageCard(mModelList.get(index).type)) break;
                spanSizeSumForCurrentRow = 0;
            }
        }
        if (spanSizeSumForCurrentRow <= spanCount) return;
        int blankSize = spanCount - (spanSizeSumForCurrentRow - spanSizeLookup.getSpanSize(index));
        for (int i = index + 1; i < mModelList.size(); i++) {
            if (spanSizeLookup.getSpanSize(i) > blankSize) continue;
            mModelList.move(i, index);
            // We should return after one move because once item moved, updateLayout() will be
            // called again.
            return;
        }
    }

    @VisibleForTesting
    void recordPriceAnnotationsEnabledMetrics() {
        if (!mTabListConfig.supportsMessageCards
                || getCurrentTabModelChecked().isIncognitoBranded()
                || !mTabListLayoutDelegate.supportsTabGroups()
                || mOriginalProfile == null
                || !PriceTrackingFeatures.isPriceAnnotationsEligible(mOriginalProfile)) {
            return;
        }
        SharedPreferencesManager preferencesManager = ChromeSharedPreferences.getInstance();
        if (System.currentTimeMillis()
                        - preferencesManager.readLong(
                                ChromePreferenceKeys
                                        .PRICE_TRACKING_ANNOTATIONS_ENABLED_METRICS_TIMESTAMP,
                                -1)
                >= PriceTrackingFeatures.getAnnotationsEnabledMetricsWindowDurationMilliSeconds()) {
            RecordHistogram.recordBooleanHistogram(
                    "Commerce.PriceDrop.AnnotationsEnabled",
                    PriceTrackingUtilities.isTrackPricesOnTabsEnabled(mOriginalProfile));
            preferencesManager.writeLong(
                    ChromePreferenceKeys.PRICE_TRACKING_ANNOTATIONS_ENABLED_METRICS_TIMESTAMP,
                    System.currentTimeMillis());
        }
    }

    /**
     * @param tabId the {@link Tab} to find the group index of.
     * @return the index for the tab group within {@link mModelList}
     */
    int getIndexForTabIdWithRelatedTabs(int tabId) {
        List<Integer> relatedTabIds = getRelatedTabIds(tabId);
        if (!relatedTabIds.isEmpty()) {
            for (int i = 0; i < mModelList.size(); i++) {
                PropertyModel model = mModelList.get(i).model;
                if (!TabProperties.isTabOrTabGroup(model)) continue;

                int modelTabId = model.get(TAB_ID);
                if (relatedTabIds.contains(modelTabId)) {
                    return i;
                }
            }
        }
        return TabModel.INVALID_TAB_INDEX;
    }

    /** Provides the tab ID for the most recently swiped tab. */
    NonNullObservableSupplier<Integer> getRecentlySwipedTabSupplier() {
        return mTabGridItemTouchHelperCallback.getRecentlySwipedTabIdSupplier();
    }

    /**
     * If the specified tab is part of a tab group, returns the UI index of the corresponding group
     * header.
     *
     * @param tabId The ID of the tab to look up.
     * @return The UI index of the group header, or {@link TabModel#INVALID_TAB_INDEX} if not found.
     */
    public int getGroupHeaderIndexForTabId(int tabId) {
        Tab tab = getCurrentTabModelChecked().getTabById(tabId);
        if (tab != null && tab.getTabGroupId() != null) {
            return mModelList.indexFromTabGroupId(tab.getTabGroupId());
        }
        return TabModel.INVALID_TAB_INDEX;
    }

    /**
     * Ensures the GROUP_HEADER for the given group has a valid TAB_ID. If the previous
     * representative tab was moved or closed, this updates the header to point to another valid tab
     * currently in the group to prevent ID hijacking.
     */
    void updateTabGroupHeaderId(@Nullable Token tabGroupId) {
        if (tabGroupId == null) return;
        int headerIndex = mModelList.indexFromTabGroupId(tabGroupId);
        if (headerIndex == TabModel.INVALID_TAB_INDEX) return;

        List<Tab> tabs = getCurrentTabModelChecked().getTabsInGroup(tabGroupId);
        if (tabs != null && !tabs.isEmpty()) {
            PropertyModel headerModel = mModelList.get(headerIndex).model;
            headerModel.set(TabProperties.TAB_ID, tabs.get(0).getId());
        }
    }

    @Nullable Tab getTabForIndex(int index) {
        return getCurrentTabModelChecked()
                .getTabById(mModelList.get(index).model.get(TabProperties.TAB_ID));
    }

    private void onTabModelChanged(@Nullable TabModel newTabModel, @Nullable TabModel oldTabModel) {
        removeObservers(oldTabModel);

        // The observers will be bound to the newFilter's when the model is reset for with tabs for
        // that filter for the first time. Doing this on the first reset after changing models
        // makes sense as otherwise we will be observing updates when the mModelList contains tabs
        // for the oldFilter which can result in invalid updates.
    }

    /**
     * Attaches tab lifecycle, actor UI state, and underline management observers to the given tab
     * so that subsequent state changes synchronize to its UI model.
     *
     * @param tab The {@link Tab} to observe.
     */
    void addObserversForTab(Tab tab) {
        mObserverManager.addTabObserver(tab);

        ActorUiTabController controller = ActorUiTabController.from(tab);
        if (controller != null) {
            controller.addObserver(mActorObserver);

            @Nullable PropertyModel model = mModelList.getModelFromTabId(tab.getId());
            if (model != null) {
                updateActorUiState(model, controller.getUiTabState());
            }
        }

        if (mTabUnderlineManager != null && !tab.isIncognito()) {
            mTabUnderlineManager.registerTab(tab);
        }
    }

    private void removeObserversForTab(Tab tab) {
        mObserverManager.removeTabObserver(tab);

        ActorUiTabController controller = ActorUiTabController.from(tab);
        if (controller != null) controller.removeObserver(mActorObserver);

        if (mTabUnderlineManager != null) {
            mTabUnderlineManager.unregisterTab(tab.getId());
        }
    }

    private void addObservers(TabModel tabModel, List<Tab> tabs) {
        if (mTabListLayoutDelegate.supportsTabGroups()) {
            for (Tab rootTab : tabs) {
                for (Tab tab : tabModel.getRelatedTabList(rootTab.getId())) {
                    addObserversForTab(tab);
                }
            }
        } else {
            for (Tab tab : tabs) {
                addObserversForTab(tab);
            }
        }

        tabModel.addObserver(mTabModelObserver);
        mObserverManager.addTabGroupObserver(tabModel);
    }

    private void removeObservers(@Nullable TabModel tabModel) {
        if (tabModel == null) return;
        // Observers are added when tabs are shown via addTabInfoToModel(). When switching
        // filters the TabObservers should be removed from all the tabs in the previous model.
        // If no observer was added this will no-op. Previously this was only done in
        // destroy(), but that left observers behind on the inactive model.
        for (Tab tab : tabModel) {
            removeObserversForTab(tab);
        }
        tabModel.removeObserver(mTabModelObserver);
        mObserverManager.removeTabGroupObserver(tabModel);
    }

    /**
     * @param itemIdentifier The itemIdentifier to match.
     * @return whether a special {@link ListItem} with the given {@code itemIdentifier} for its
     *     {@link PropertyModel} exists in the current {@link TabListModel}.
     */
    boolean specialItemExistsInModel(@MessageType int itemIdentifier) {
        if (itemIdentifier == MessageType.ALL) {
            return mModelList.lastIndexForMessageItem() != TabModel.INVALID_TAB_INDEX;
        }
        return mModelList.lastIndexForMessageItemFromType(itemIdentifier)
                != TabModel.INVALID_TAB_INDEX;
    }

    /**
     * Prepare and run the Quick Delete animation on the tab list.
     *
     * @param onAnimationEnd Runnable that is invoked when the animation is completed.
     * @param tabs The tabs to fade with the animation. These tabs will get closed after the
     *     animation is complete.
     * @param recyclerView The {@link TabListRecyclerView} that is showing the tab list UI.
     */
    public void showQuickDeleteAnimation(
            Runnable onAnimationEnd, List<Tab> tabs, TabListRecyclerView recyclerView) {
        recyclerView.setBlockTouchInput(true);
        Drawable originalForeground = recyclerView.getForeground();

        // Prepare the tabs that will be hidden by the animation.
        TreeMap<Integer, List<PropertyModel>> bottomValuesToPropertyModels = new TreeMap<>();
        getOrderOfTabsForQuickDeleteAnimation(recyclerView, tabs, bottomValuesToPropertyModels);

        setQuickDeleteAnimationStatusForPropertyModels(
                CollectionUtil.flatten(bottomValuesToPropertyModels.values()),
                QuickDeleteAnimationStatus.TAB_PREPARE);

        // Create the gradient drawable and prepare the animator.
        int tabGridHeight = recyclerView.getHeight();
        int intersectionHeight =
                QuickDeleteAnimationGradientDrawable.getAnimationsIntersectionHeight(tabGridHeight);
        QuickDeleteAnimationGradientDrawable gradientDrawable =
                QuickDeleteAnimationGradientDrawable.createQuickDeleteWipeAnimationDrawable(
                        mActivity, tabGridHeight, getCurrentTabModelChecked().isIncognitoBranded());

        ObjectAnimator wipeAnimation = gradientDrawable.createWipeAnimator(tabGridHeight);

        wipeAnimation.addUpdateListener(
                valueAnimator -> {
                    if (bottomValuesToPropertyModels.isEmpty()) return;

                    float value = (float) valueAnimator.getAnimatedValue();
                    int bottomVal = bottomValuesToPropertyModels.lastKey();
                    if (bottomVal >= Math.round(value) + intersectionHeight) {
                        setQuickDeleteAnimationStatusForPropertyModels(
                                assumeNonNull(bottomValuesToPropertyModels.get(bottomVal)),
                                QuickDeleteAnimationStatus.TAB_HIDE);
                        bottomValuesToPropertyModels.remove(bottomVal);
                    }
                });

        wipeAnimation.addListener(
                new AnimatorListenerAdapter() {
                    @Override
                    public void onAnimationEnd(Animator animation) {
                        recyclerView.setBlockTouchInput(false);
                        recyclerView.setForeground(originalForeground);
                        onAnimationEnd.run();
                    }
                });

        recyclerView.setForeground(gradientDrawable);
        wipeAnimation.start();
    }

    // TabListNotificationHandler implementation.
    @Override
    public void updateTabStripNotificationBubble(
            Set<Integer> tabIdsToBeUpdated, boolean hasUpdate) {
        assert mTabListConfig.tabUiType == UiType.STRIP
                : "Notification bubbles are only supported for strip mode.";

        Callback<PropertyModel> updateTabStripItemCallback =
                (model) -> model.set(TabProperties.HAS_NOTIFICATION_BUBBLE, hasUpdate);

        forAllTabListItems(tabIdsToBeUpdated, updateTabStripItemCallback);
    }

    @Override
    public void updateTabCardLabels(Map<Integer, TabCardLabelData> labelData) {
        assert mTabListConfig.tabUiType == UiType.TAB
                : "Tab card labels are only supported for tab card UI type.";

        Callback<PropertyModel> updateTabCardLabel =
                (model) -> {
                    int tabId = TabProperties.getTabId(model);
                    model.set(TabProperties.TAB_CARD_LABEL_DATA, labelData.get(tabId));
                };
        forAllTabListItems(labelData.keySet(), updateTabCardLabel);
    }

    private void forAllTabListItems(
            Set<Integer> tabIdsToBeUpdated, Callback<PropertyModel> updateCallback) {
        for (int i = 0; i < mModelList.size(); i++) {
            PropertyModel model = mModelList.get(i).model;
            if (!TabProperties.isTabOrTabGroup(model)) continue;

            int tabId = TabProperties.getTabId(model);
            if (tabIdsToBeUpdated.contains(tabId)) {
                updateCallback.onResult(model);
                updateDescriptionString(model);
            }
        }
    }

    /**
     * Gets the order of tabs to be hidden with the animation starting from the bottom up.
     *
     * @param recyclerView to get the position of tabs within the {@link TabListRecyclerView}.
     * @param tabs The tabs to fade with the animation.
     * @param bottomValuesToTabIndexes the {@link TreeMap} to map a list of sorted bottom values to
     *     tabs that have these bottom values.
     */
    @VisibleForTesting
    void getOrderOfTabsForQuickDeleteAnimation(
            TabListRecyclerView recyclerView,
            List<Tab> tabs,
            TreeMap<Integer, List<PropertyModel>> bottomValuesToPropertyModels) {
        Set<Tab> filteredTabs = filterQuickDeleteTabsForAnimation(tabs);

        for (Tab tab : filteredTabs) {
            int id = tab.getId();
            int index = mModelList.indexFromTabId(id);
            if (index == TabModel.INVALID_TAB_INDEX) {
                continue;
            }
            Rect tabRect = recyclerView.getRectOfCurrentThumbnail(index, id);

            // Ignore tabs that are outside the screen view.
            if (tabRect == null) continue;

            int bottom = tabRect.bottom;
            PropertyModel model = mModelList.get(index).model;

            if (bottomValuesToPropertyModels.containsKey(bottom)) {
                bottomValuesToPropertyModels.get(bottom).add(model);
            } else {
                bottomValuesToPropertyModels.put(bottom, new ArrayList<>(List.of(model)));
            }
        }
    }

    /**
     * @param tabs The full list of tabs that will be closed with Quick Delete.
     * @return a filtered list of unique tabs that the animation should run on. This will ignore
     *     tabs with other related tabs unless all of it's related tabs are included in the list of
     *     tabs to be closed.
     */
    private Set<Tab> filterQuickDeleteTabsForAnimation(List<Tab> tabs) {
        TabModel tabModel = getCurrentTabModelChecked();

        Set<Tab> unfilteredTabs = new HashSet<>(tabs);
        Set<Tab> filteredTabs = new HashSet<>();
        Set<Token> checkedTabGroupIds = new HashSet<>();

        // Migrating this to tab group id requires a rewrite as the root id based logic assumes that
        // TabModel treats individual tabs similar to tab groups.
        for (Tab tab : unfilteredTabs) {
            if (!tabModel.isTabInTabGroup(tab)) {
                filteredTabs.add(tab);
                continue;
            }

            Token tabGroupId = tab.getTabGroupId();
            assumeNonNull(tabGroupId);
            if (checkedTabGroupIds.contains(tabGroupId)) continue;
            checkedTabGroupIds.add(tabGroupId);

            List<Tab> relatedTabs = tabModel.getTabsInGroup(tabGroupId);
            if (unfilteredTabs.containsAll(relatedTabs)) {
                int groupIndex = tabModel.representativeIndexOf(tab);
                Tab groupTab = tabModel.getRepresentativeTabAt(groupIndex);
                if (groupTab != null) {
                    filteredTabs.add(groupTab);
                }
            }
        }

        return filteredTabs;
    }

    private void setQuickDeleteAnimationStatusForPropertyModels(
            List<PropertyModel> models, @QuickDeleteAnimationStatus int animationStatus) {
        for (PropertyModel model : models) {
            model.set(TabProperties.QUICK_DELETE_ANIMATION_STATUS, animationStatus);
        }
    }

    @VisibleForTesting
    void onMenuItemClicked(
            @IdRes int menuId,
            Token tabGroupId,
            @Nullable String collaborationId,
            @Nullable ListViewTouchTracker listViewTouchTracker) {
        TabModel tabModel = getCurrentTabModelChecked();
        int tabId = tabModel.getGroupLastShownTabId(tabGroupId);
        EitherGroupId eitherId = EitherGroupId.createLocalId(new LocalTabGroupId(tabGroupId));
        if (tabId == Tab.INVALID_TAB_ID) return;

        if (menuId == R.id.close_tab_group || menuId == R.id.delete_tab_group) {
            boolean hideTabGroups = menuId == R.id.close_tab_group;
            if (hideTabGroups) {
                RecordUserAction.record("TabGroupItemMenu.Close");
            } else {
                RecordUserAction.record("TabGroupItemMenu.Delete");
            }

            boolean allowUndo = TabClosureParamsUtils.shouldAllowUndo(listViewTouchTracker);

            @TabClosingSource int tabClosingSource = mTabListConfig.tabClosingSource;

            setUseShrinkCloseAnimation(tabId, /* useShrinkCloseAnimation= */ true);
            onGroupClosedFrom(tabId);
            TabUiUtils.closeTabGroup(
                    tabModel,
                    tabId,
                    tabClosingSource,
                    allowUndo,
                    hideTabGroups,
                    getOnMaybeTabClosedCallback(tabId));
        } else if (menuId == R.id.edit_group_name) {
            RecordUserAction.record("TabGroupItemMenu.Rename");
            renameTabGroup(tabId);
        } else if (menuId == R.id.ungroup_tab) {
            RecordUserAction.record("TabGroupItemMenu.Ungroup");
            TabUiUtils.ungroupTabGroup(tabModel, tabGroupId);
        } else if (menuId == R.id.delete_shared_group) {
            RecordUserAction.record("TabGroupItemMenu.DeleteShared");
            assumeNonNull(mDataSharingTabManager);
            mDataSharingTabManager.leaveOrDeleteFlow(
                    eitherId,
                    CollaborationServiceLeaveOrDeleteEntryPoint.ANDROID_TAB_GROUP_ITEM_MENU_DELETE);
        } else if (menuId == R.id.leave_group) {
            RecordUserAction.record("TabGroupItemMenu.LeaveShared");
            assumeNonNull(mDataSharingTabManager);
            mDataSharingTabManager.leaveOrDeleteFlow(
                    eitherId,
                    CollaborationServiceLeaveOrDeleteEntryPoint.ANDROID_TAB_GROUP_ITEM_MENU_LEAVE);
        } else if (menuId == R.id.share_group) {
            assert mDataSharingTabManager != null;
            RecordUserAction.record("TabGroupItemMenu.ShareGroup");
            mDataSharingTabManager.createOrManageFlow(
                    eitherId,
                    CollaborationServiceShareOrManageEntryPoint.TAB_GROUP_ITEM_MENU_SHARE,
                    /* createGroupFinishedCallback= */ null);
        }
    }

    private void renameTabGroup(int tabId) {
        assert mModalDialogManager != null;

        TabModel tabModel = getCurrentTabModelChecked();
        Tab tab = tabModel.getTabById(tabId);
        assumeNonNull(tab);
        Token tabGroupId = tab.getTabGroupId();
        assumeNonNull(tabGroupId);

        var tabGroupVisualDataDialogManager =
                new TabGroupVisualDataDialogManager(
                        mActivity,
                        mModalDialogManager,
                        TabGroupVisualDataDialogManager.DialogType.TAB_GROUP_EDIT,
                        R.string.tab_group_rename_dialog_title);

        ModalDialogProperties.Controller dialogController =
                new ModalDialogProperties.Controller() {
                    @Override
                    public void onClick(PropertyModel model, int buttonType) {
                        if (buttonType == ModalDialogProperties.ButtonType.POSITIVE
                                && !tabGroupVisualDataDialogManager.validateCurrentGroupTitle()) {
                            tabGroupVisualDataDialogManager.focusCurrentGroupTitle();
                            return;
                        }

                        final @DialogDismissalCause int cause;
                        if (buttonType == ModalDialogProperties.ButtonType.POSITIVE) {
                            cause = DialogDismissalCause.POSITIVE_BUTTON_CLICKED;
                        } else {
                            cause = DialogDismissalCause.NEGATIVE_BUTTON_CLICKED;
                        }
                        assumeNonNull(mModalDialogManager);
                        mModalDialogManager.dismissDialog(model, cause);
                    }

                    @Override
                    public void onDismiss(PropertyModel model, int dismissalCause) {
                        if (dismissalCause == DialogDismissalCause.POSITIVE_BUTTON_CLICKED) {
                            boolean stillExists = tabModel.tabGroupExists(tabGroupId);
                            @TabGroupColorId
                            int oldColorId = tabModel.getTabGroupColorWithFallback(tabGroupId);
                            @TabGroupColorId
                            int currentColorId =
                                    tabGroupVisualDataDialogManager.getCurrentColorId();
                            boolean didChangeColor = oldColorId != currentColorId;
                            if (didChangeColor) {
                                if (stillExists) {
                                    tabModel.setTabGroupColor(tabGroupId, currentColorId);
                                }
                                RecordUserAction.record("TabGroup.RenameDialog.ColorChanged");
                            }

                            String initialGroupTitle =
                                    tabGroupVisualDataDialogManager.getInitialGroupTitle();
                            String inputGroupTitle =
                                    tabGroupVisualDataDialogManager.getCurrentGroupTitle();
                            boolean didChangeTitle =
                                    !Objects.equals(initialGroupTitle, inputGroupTitle);
                            // This check must be included in case the user has a null title
                            // which is displayed as a tab count and chooses not to change it.
                            if (didChangeTitle) {
                                if (stillExists) {
                                    tabModel.setTabGroupTitle(tabGroupId, inputGroupTitle);
                                }
                                RecordUserAction.record("TabGroup.RenameDialog.TitleChanged");
                            }
                        }

                        tabGroupVisualDataDialogManager.onHideDialog();
                    }
                };

        tabGroupVisualDataDialogManager.showDialog(tab.getTabGroupId(), tabModel, dialogController);
    }

    private TextResolver getActionButtonDescriptionTextResolver(
            int numOfRelatedTabs, String title, Tab tab) {
        TabModel tabModel = getCurrentTabModelChecked();
        Token tabGroupId = tab.getTabGroupId();
        assumeNonNull(tabGroupId);
        @TabGroupColorId int colorId = tabModel.getTabGroupColorWithFallback(tabGroupId);
        final @StringRes int colorDescRes =
                TabGroupColorPickerUtils.getTabGroupColorPickerItemColorAccessibilityString(
                        colorId);
        String colorDesc = mActivity.getResources().getString(colorDescRes);
        return (context) -> {
            Resources res = context.getResources();
            String descriptionTitle = title;
            if (TextUtils.isEmpty(descriptionTitle)) {
                descriptionTitle = TabGroupTitleUtils.getDefaultTitle(mActivity, numOfRelatedTabs);
            }
            if (!TabUiUtils.isDataSharingFunctionalityEnabled() || !hasCollaboration(tab)) {
                return res.getString(
                        R.string
                                .accessibility_open_tab_group_overflow_menu_with_group_name_with_color,
                        descriptionTitle,
                        colorDesc);
            } else {
                return res.getString(
                        R.string
                                .accessibility_open_shared_tab_group_overflow_menu_with_group_name_with_color,
                        descriptionTitle,
                        colorDesc);
            }
        };
    }

    /** Check if the current tab group's tab representation is being shared. */
    private boolean hasCollaboration(Tab tab) {
        TabModel tabModel = getCurrentTabModelChecked();
        if (tabModel.isIncognitoBranded()) return false;

        @Nullable TabGroupSyncService tabGroupSyncService = null;
        if (TabGroupSyncFeatures.isTabGroupSyncEnabled(tab.getProfile())) {
            assumeNonNull(mOriginalProfile);
            tabGroupSyncService = TabGroupSyncServiceFactory.getForProfile(mOriginalProfile);
        }
        @Nullable
        String collaborationId =
                TabShareUtils.getCollaborationIdOrNull(tab.getId(), tabModel, tabGroupSyncService);
        return TabShareUtils.isCollaborationIdValid(collaborationId);
    }

    private void setUseShrinkCloseAnimation(int tabId, boolean useShrinkCloseAnimation) {
        if (!mTabListConfig.supportsShrinkCloseAnimation) return;

        @Nullable PropertyModel model = mModelList.getModelFromTabId(tabId);
        if (model != null) {
            model.set(TabProperties.USE_SHRINK_CLOSE_ANIMATION, useShrinkCloseAnimation);
        }
    }

    @VisibleForTesting
    @Nullable Callback<Boolean> getOnMaybeTabClosedCallback(int tabId) {
        Tab tab = getCurrentTabModelChecked().getTabById(tabId);
        if (tab == null) return null;

        return (didClose) -> {
            if (!didClose) {
                mTabClosedFrom.delete(tabId);
                setUseShrinkCloseAnimation(tabId, /* useShrinkCloseAnimation= */ false);
                int modelIndex = mModelList.indexFromTabId(tabId);
                if (modelIndex != TabModel.INVALID_TAB_INDEX) {
                    resetSwipe(modelIndex);
                }
                return;
            }

            RecordUserAction.record(
                    "MobileTabClosed."
                            + TabUiMetricsHelper.getComponentNameForMetrics(mComponentId));

            // Special case in defense of a group not being completely closed. We need to find the
            // group by the tab's old root ID.
            int index = getIndexForTabIdWithRelatedTabs(tab.getId());
            if (mModelList.isValidIndex(index)) {
                if (mTabListConfig.supportsShrinkCloseAnimation) {
                    mModelList
                            .get(index)
                            .model
                            .set(TabProperties.USE_SHRINK_CLOSE_ANIMATION, false);
                }
                resetSwipe(index);
            }
        };
    }

    private void resetSwipe(int index) {
        if (!mModelList.isValidIndex(index)) return;
        // The view element has been removed. We need to bring that back. This is done by just
        // triggering a model update for that index.
        mModelList.update(index, mModelList.get(index));
    }

    void setThumbnailSpinnerVisibility(Tab tab, boolean isVisible) {
        assert mLayoutType == TabListLayoutType.FLAT;
        int index = mModelList.indexFromTabId(tab.getId());
        if (index == TabModel.INVALID_TAB_INDEX) return;

        PropertyModel model = mModelList.get(index).model;
        if (model == null) return;

        model.set(TabProperties.SHOW_THUMBNAIL_SPINNER, isVisible);
        if (!isVisible) {
            updateThumbnailFetcher(model, tab.getId());
        }
    }

    void updateThumbnailFetcher(PropertyModel model, int tabId) {
        if (mThumbnailProvider == null) return;

        @Nullable ThumbnailFetcher oldFetcher = model.get(THUMBNAIL_FETCHER);
        if (oldFetcher != null) oldFetcher.cancel();

        @Nullable ThumbnailFetcher newFetcher = null;
        if (tabId != Tab.INVALID_TAB_ID) {
            TabModel tabModel = getCurrentTabModelChecked();
            Tab tab = tabModel.getTabById(tabId);
            if (tab == null) return;

            boolean isInTabGroup = tabModel.tabGroupExists(tab.getTabGroupId());
            final @Nullable @TabGroupColorId Integer tabGroupColor =
                    isInTabGroup
                            ? tabModel.getTabGroupColorWithFallback(
                                    assumeNonNull(tab.getTabGroupId()))
                            : null;

            List<Integer> actingTabIds = Collections.emptyList();
            if (TabProperties.isTabGroupHeader(model) && isInTabGroup) {
                actingTabIds = new ArrayList<>();
                for (Tab groupTab : tabModel.getRelatedTabList(tabId)) {
                    ActorUiTabController controller = ActorUiTabController.from(groupTab);
                    if (controller != null) {
                        UiTabState state = controller.getUiTabState();
                        if (state != null && state.tabIndicator != TabIndicatorStatus.NONE) {
                            actingTabIds.add(groupTab.getId());
                        }
                    }
                }
            }

            newFetcher =
                    new ThumbnailFetcher(
                            mThumbnailProvider,
                            MultiThumbnailMetadata.createMetadataWithActingTabs(
                                    tabId,
                                    isInTabGroup,
                                    tabModel.isIncognitoBranded(),
                                    tabGroupColor,
                                    actingTabIds));
        }
        model.set(THUMBNAIL_FETCHER, newFetcher);
    }

    private void updateThumbnailFetcher(PropertyModel model, SavedTabGroup savedTabGroup) {
        if (mThumbnailProvider == null) return;

        ThumbnailFetcher oldFetcher = model.get(THUMBNAIL_FETCHER);
        if (oldFetcher != null) oldFetcher.cancel();

        List<GURL> urlList = new ArrayList<>();
        for (SavedTabGroupTab savedTab : savedTabGroup.savedTabs) {
            urlList.add(savedTab.url);
        }

        boolean isIncognito = getCurrentTabModelChecked().isIncognitoBranded();
        ThumbnailFetcher newFetcher =
                new ThumbnailFetcher(
                        mThumbnailProvider,
                        MultiThumbnailMetadata.createMetadataWithUrls(
                                Tab.INVALID_TAB_ID,
                                urlList,
                                /* isInTabGroup= */ true,
                                isIncognito,
                                savedTabGroup.color));
        model.set(THUMBNAIL_FETCHER, newFetcher);
    }

    /**
     * Resolves the latest title for a tab group and updates the corresponding group header card's
     * title and accessibility descriptions in {@link #mModelList}.
     *
     * @param tabGroupId The {@link Token} of the tab group to update.
     */
    void updateTabGroupTitle(Token tabGroupId) {
        @Nullable Pair<Integer, Tab> headerIndexAndTab =
                mTabListLayoutDelegate.getIndexAndTabForTabGroupId(tabGroupId);
        if (headerIndexAndTab == null) return;
        PropertyModel headerModel = mModelList.get(headerIndexAndTab.first).model;
        Tab tab = headerIndexAndTab.second;
        // Do not trust the `newTitle`, it may be necessary to apply a default/fallback.
        String title = getLatestTitleForTabOrGroup(tab, headerModel, /* useDefault= */ true);
        headerModel.set(TabProperties.TITLE, title);
        updateDescriptionString(headerModel);
        updateActionButtonDescriptionString(tab, headerModel);
    }

    /**
     * Resets all tab group visual properties on the given model and destroys any attached {@link
     * TabGroupColorViewProvider}.
     *
     * @param model The {@link PropertyModel} from which group properties should be removed.
     */
    void clearTabGroupProperties(PropertyModel model) {
        @Nullable TabGroupColorViewProvider provider = model.get(TAB_GROUP_COLOR_VIEW_PROVIDER);
        model.set(TabProperties.TAB_GROUP_ID, null);
        model.set(TabProperties.TAB_GROUP_CARD_COLOR, null);
        model.set(TabProperties.TAB_GROUP_HEADER_ID, null);
        model.set(TAB_GROUP_COLOR_VIEW_PROVIDER, null);
        if (provider != null) provider.destroy();
    }

    /**
     * Updates group visual styling (such as group color provider and group header/card IDs) on a
     * tab's property model based on its group membership and the active layout.
     *
     * @param tab The {@link Tab} whose properties are being updated.
     * @param model The {@link PropertyModel} associated with the tab.
     * @param colorId The {@link TabGroupColorId} to apply to the group indicators.
     */
    void updateTabGroupProperties(Tab tab, PropertyModel model, @TabGroupColorId int colorId) {
        @Nullable Token tabGroupId = tab.getTabGroupId();
        if (!mTabListLayoutDelegate.supportsTabGroups()
                || tabGroupId == null
                || !isTabInTabGroup(tab)) {
            clearTabGroupProperties(model);
            return;
        }

        if (model.get(TabProperties.TAB_GROUP_ID) == null) {
            model.set(TabProperties.TAB_GROUP_HEADER_ID, tabGroupId);
        } else {
            model.set(TabProperties.TAB_GROUP_HEADER_ID, null);
        }

        updateTabGroupColorViewProvider(
                EitherGroupId.createLocalId(new LocalTabGroupId(tabGroupId)), model, colorId);
    }

    void updateTabGroupColorViewProvider(
            EitherGroupId groupId, PropertyModel model, @TabGroupColorId int colorId) {
        // Dynamically created tab groups default to a collapsed state. Only initialize
        // this property if the card was not already representing a tab group.
        if (model.containsKey(TabProperties.IS_COLLAPSED)
                && model.get(TabProperties.TAB_GROUP_CARD_COLOR) == null) {
            model.set(TabProperties.IS_COLLAPSED, true);
        }
        // Set tab group color.
        model.set(TabProperties.TAB_GROUP_CARD_COLOR, colorId);
        assert colorId != TabGroupColorUtils.INVALID_COLOR_ID
                : "Tab in tab group should always have valid colors.";
        assert mTabListConfig.tabUiType != UiType.STRIP
                : "Tab group colors are not applicable to strip mode.";

        @Nullable TabGroupColorViewProvider provider = model.get(TAB_GROUP_COLOR_VIEW_PROVIDER);
        if (provider == null) {
            boolean isIncognitoBranded = getCurrentTabModelChecked().isIncognitoBranded();
            provider =
                    new TabGroupColorViewProvider(
                            mActivity,
                            groupId,
                            isIncognitoBranded,
                            colorId,
                            mTabGroupSyncService,
                            mDataSharingService,
                            assumeNonNull(mCollaborationService));
            model.set(TAB_GROUP_COLOR_VIEW_PROVIDER, provider);
        } else {
            provider.setTabGroupId(groupId);
            provider.setTabGroupColorId(colorId);
        }
    }

    private void showLimitSnackbar() {
        if (mSnackbarManager == null) return;

        String message =
                mActivity
                        .getResources()
                        .getQuantityString(
                                R.plurals.tab_item_picker_limit_reached,
                                mAllowedSelectionCount,
                                mAllowedSelectionCount);

        Snackbar snackbar =
                Snackbar.make(
                        message,
                        null,
                        Snackbar.TYPE_NOTIFICATION,
                        Snackbar.UMA_TAB_PICKER_LIMIT_REACHED);
        TabModel tabModel = getCurrentTabModelChecked();
        boolean isIncognito = tabModel.isIncognitoBranded();
        snackbar.setBackgroundColor(ChromeColors.getInverseBgColor(mActivity, isIncognito));

        int textAppearanceResId =
                isIncognito
                        ? R.style.TextAppearance_TextMedium_Primary_Baseline_Dark
                        : R.style.TextAppearance_TextMedium_OnInverseSurface;
        snackbar.setTextAppearance(textAppearanceResId);

        mSnackbarManager.showSnackbar(snackbar);
    }

    private void dismissLimitSnackbar() {
        if (mSnackbarManager == null) return;
        mSnackbarManager.dismissAllSnackbars();
    }

    // TODO(crbug.com/456216687): Refactor a11y labels.
    private String getAlertStateAccessibilityString(PropertyModel model, Resources res) {
        @TabAlert
        int alertState =
                model.containsKey(TabProperties.ALERT_STATE)
                        ? model.get(TabProperties.ALERT_STATE)
                        : TabAlert.NONE;
        return switch (alertState) {
            case TabAlert.AUDIO_PLAYING -> res.getString(R.string.accessibility_tab_group_audible);
            case TabAlert.AUDIO_MUTING -> res.getString(R.string.accessibility_tab_group_muted);
            case TabAlert.AUDIO_RECORDING, TabAlert.MEDIA_RECORDING, TabAlert.VIDEO_RECORDING ->
                    res.getString(R.string.accessibility_tab_group_recording);
            case TabAlert.TAB_CAPTURING, TabAlert.DESKTOP_CAPTURING ->
                    res.getString(R.string.accessibility_tab_group_sharing);
            case TabAlert.PIP_PLAYING ->
                    res.getString(R.string.accessibility_tab_group_picture_in_picture);
            default -> "";
        };
    }

    private void onRailCollapseStateChanged(@RailCollapseState int railCollapseState) {
        for (ListItem item : mModelList) {
            if (TabProperties.isTabOrTabGroup(item.model)) {
                item.model.set(TabProperties.RAIL_COLLAPSE_STATE, railCollapseState);
            }
        }
    }

    View.AccessibilityDelegate getAccessibilityDelegateForTesting() {
        return mAccessibilityDelegate;
    }

    @Nullable Tab getTabToAddDelayedForTesting() {
        return mTabToAddDelayed;
    }

    void setComponentIdForTesting(@TabComponentId int componentId) {
        var oldValueId = mComponentId;
        mComponentId = componentId;
        ResettersForTesting.register(() -> mComponentId = oldValueId);
    }
}

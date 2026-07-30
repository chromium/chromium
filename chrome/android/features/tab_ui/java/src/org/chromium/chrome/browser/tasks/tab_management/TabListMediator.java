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
import static org.chromium.chrome.browser.tasks.tab_management.TabSwitcherMessageManager.isOnlyArchivedMsg;
import static org.chromium.chrome.browser.tasks.tab_management.UiTypeHelper.isLargeMessageCard;
import static org.chromium.chrome.browser.tasks.tab_management.UiTypeHelper.isMessageCard;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.animation.ObjectAnimator;
import android.app.Activity;
import android.content.ComponentCallbacks;
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
import android.view.View;
import android.view.accessibility.AccessibilityNodeInfo;
import android.view.accessibility.AccessibilityNodeInfo.AccessibilityAction;

import androidx.annotation.IdRes;
import androidx.annotation.IntDef;
import androidx.annotation.StringRes;
import androidx.annotation.VisibleForTesting;
import androidx.recyclerview.widget.GridLayoutManager;

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
import org.chromium.chrome.browser.compositor.overlays.strip.StripTabUnderlineManager;
import org.chromium.chrome.browser.data_sharing.DataSharingServiceFactory;
import org.chromium.chrome.browser.data_sharing.DataSharingTabManager;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.glic.GlicEnabling;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.price_tracking.PriceTrackingFeatures;
import org.chromium.chrome.browser.price_tracking.PriceTrackingUtilities;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.quick_delete.QuickDeleteAnimationGradientDrawable;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.MediaState;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabId;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tab.TabUtils;
import org.chromium.chrome.browser.tab.state.ShoppingPersistedTabData;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncFeatures;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncServiceFactory;
import org.chromium.chrome.browser.tab_ui.TabListFaviconProvider;
import org.chromium.chrome.browser.tab_ui.TabListFaviconProvider.TabFaviconFetcher;
import org.chromium.chrome.browser.tab_ui.TabListMode;
import org.chromium.chrome.browser.tab_ui.ThumbnailProvider;
import org.chromium.chrome.browser.tab_ui.ThumbnailProvider.MultiThumbnailMetadata;
import org.chromium.chrome.browser.tabmodel.TabClosingSource;
import org.chromium.chrome.browser.tabmodel.TabClosureParams;
import org.chromium.chrome.browser.tabmodel.TabClosureParamsUtils;
import org.chromium.chrome.browser.tabmodel.TabGroupColorUtils;
import org.chromium.chrome.browser.tabmodel.TabGroupObserver;
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
import org.chromium.chrome.browser.tasks.tab_management.TabGridView.QuickDeleteAnimationStatus;
import org.chromium.chrome.browser.tasks.tab_management.TabListModel.AnimationStatus;
import org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties;
import org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.ModelType;
import org.chromium.chrome.browser.tasks.tab_management.TabProperties.TabActionState;
import org.chromium.chrome.browser.tasks.tab_management.TabProperties.UiType;
import org.chromium.chrome.browser.tasks.tab_management.TabSwitcherMessageManager.MessageType;
import org.chromium.chrome.browser.tasks.tab_management.TabUiMetricsHelper.TabListEditorActionMetricGroups;
import org.chromium.chrome.browser.tasks.tab_management.vertical_tabs.VerticalTabHoverCardController.TabHoverCardListener;
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
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.ListObservable;
import org.chromium.ui.modelutil.ListObservable.ListObserver;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;
import org.chromium.ui.recyclerview.widget.ItemTouchHelper2;
import org.chromium.url.GURL;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
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
    /** An interface to handle requests about updating TabGridDialog. */
    public interface TabGridDialogHandler {
        /**
         * This method updates the status of the ungroup bar in TabGridDialog.
         *
         * @param status The status in {@link TabGridDialogView.UngroupBarStatus} that the ungroup
         *         bar should be updated to.
         */
        void updateUngroupBarStatus(@TabGridDialogView.UngroupBarStatus int status);

        /**
         * This method updates the content of the TabGridDialog.
         *
         * @param tabId The id of the {@link Tab} that is used to update TabGridDialog.
         */
        void updateDialogContent(int tabId);
    }

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
         * @param fromActionButton Whether it is called from the Action button on the card.
         */
        void onTabSelecting(int tabId, boolean fromActionButton);
    }

    /**
     * Defines the layout structure used by the TabList. - FLAT: A linear list of tabs where groups
     * are not supported or treated as single tabs. - GROUPED: A flat list where tab groups are
     * permanently collapsed and represented as single interactive cards. - NESTED: A hierarchical
     * list where tab groups can be expanded to show their children.
     */
    @IntDef({
        TabListLayoutType.FLAT,
        TabListLayoutType.GROUPED,
        TabListLayoutType.NESTED,
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface TabListLayoutType {
        int FLAT = 0;
        int GROUPED = 1;
        int NESTED = 2;
    }

    /**
     * A delegate providing configuration policies and visual capabilities for the TabList. The
     * returned values are not allowed to change at runtime.
     */
    public interface TabListConfigDelegate {
        /** Returns the layout type used for the TabList. */
        @TabListLayoutType
        int getLayoutType();

        /** Returns whether the layout supports message card items. */
        boolean supportsMessageCards();

        /** Returns a supplier for the rail collapsed state, if applicable. */
        @Nullable NonNullObservableSupplier<@RailCollapseState Integer>
                getRailCollapseStateSupplier();

        /** Returns a listener for tab hover card events, if applicable. */
        @Nullable TabHoverCardListener getTabHoverCardListener();
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
    private static final Map<Integer, Integer> sTabClosedFromMap = new HashMap<>();

    private final Callback<@Nullable TabModel> mOnTabModelChanged =
            new ValueChangedCallback<>(this::onTabModelChanged);
    private final TabOverflowMenuCoordinator.OnItemClickedCallback<Token>
            mOnMenuItemClickedCallback = this::onMenuItemClicked;
    private final Activity mActivity;
    private final TabListModel mModelList;
    private final @TabListMode int mMode;
    private final @Nullable ModalDialogManager mModalDialogManager;
    private final NullableObservableSupplier<TabModel> mCurrentTabModelSupplier;
    private final @Nullable ThumbnailProvider mThumbnailProvider;
    private final TabListFaviconProvider mTabListFaviconProvider;
    private final @Nullable SelectionDelegateProvider<TabListEditorItemSelectionId>
            mSelectionDelegateProvider;
    private final @Nullable TabListItemOnClickListenerProvider mTabListItemOnClickListenerProvider;
    private final @Nullable TabGridDialogHandler mTabGridDialogHandler;
    private final @Nullable Supplier<@Nullable PriceWelcomeMessageController>
            mPriceWelcomeMessageControllerSupplier;
    private final @Nullable DataSharingTabManager mDataSharingTabManager;
    private final @Nullable Runnable mOnTabGroupCreation;
    private final TabModelObserver mTabModelObserver;
    private final TabListLayoutDelegate mTabListLayoutDelegate;
    private final TabActionListener mTabClosedListener;
    private final TabGridItemTouchHelperCallback mTabGridItemTouchHelperCallback;
    private final @Nullable UndoBarExplicitTrigger mUndoBarExplicitTrigger;
    private final @Nullable SnackbarManager mSnackbarManager;
    private final @Nullable NonNullObservableSupplier<@RailCollapseState Integer>
            mRailCollapseStateSupplier;
    private final @Nullable Callback<@RailCollapseState Integer> mRailCollapseStateObserver;
    private final int mAllowedSelectionCount;
    private final boolean mIsSingleContextMode;
    private final @TabListLayoutType int mLayoutType;
    private final boolean mSupportsMessageCards;
    private final @Nullable TabHoverCardListener mTabHoverCardListener;

    private int mNextTabId = Tab.INVALID_TAB_ID;
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
    private @Nullable StripTabUnderlineManager mGlicIndicatorManager;

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

                    if (mLayoutType == TabListLayoutType.GROUPED && isTabInTabGroup(tab)) {
                        int index = getIndexForTabIdWithRelatedTabs(tabId);
                        if (index != TabModel.INVALID_TAB_INDEX) {
                            PropertyModel groupModel = mModelList.get(index).model;
                            updateThumbnailFetcher(
                                    groupModel, groupModel.get(TabProperties.TAB_ID));
                        }
                    }
                }
            };

    private final StripTabUnderlineManager.Observer mGlicObserver =
            new StripTabUnderlineManager.Observer() {
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

                    mNextTabId = tabId;

                    TabModel tabModel = getCurrentTabModelChecked();
                    if (mLayoutType == TabListLayoutType.FLAT
                            || mLayoutType == TabListLayoutType.NESTED) {
                        // We filtered the tab switching related metric for components that takes
                        // actions on all related tabs (e.g. GTS) because that component can
                        // switch to different TabModel before switching tabs, while this class
                        // only contains information for all tabs that are in the same TabModel,
                        // more specifically:
                        //   * For MobileTabSwitched, as compared to the VTS, we need to account for
                        //     MobileTabReturnedToCurrentTab action. This action is defined as
                        // return to the
                        //     same tab as before entering the component, and we don't have this
                        // information
                        //     here.
                        recordTabSelection(tabId);
                    }
                    if (mTabListItemOnClickListenerProvider != null) {
                        mTabListItemOnClickListenerProvider.onTabSelecting(
                                tabId, /* fromActionButton= */ true);
                    } else {
                        tabModel.setIndex(
                                TabModelUtils.getTabIndexById(tabModel, tabId),
                                TabSelectionType.FROM_USER);
                    }
                }

                @Override
                public void run(
                        View view, String syncId, @Nullable MotionEventInfo triggeringMotion) {
                    // Intentional no-op.
                }

                /**
                 * Records MobileTabSwitched for the component. This method only records UMA for
                 * components other than TabSwitcher.
                 */
                private void recordTabSelection(int tabId) {
                    Tab tab = getCurrentTabModelChecked().getTabById(tabId);
                    if (tab != null
                            && tab.getIsPinned()
                            && mComponentId == TabComponentId.VERTICAL_TABS) {
                        RecordUserAction.record("MobileTabSwitched.VerticalTabsPinned");
                    } else {
                        RecordUserAction.record(
                                "MobileTabSwitched."
                                        + TabUiMetricsHelper.getComponentNameForMetrics(
                                                mComponentId));
                    }
                }
            };

    private final TabActionListener mSelectableTabOnClickListener =
            new TabActionListener() {
                @Override
                public void run(View view, int tabId, @Nullable MotionEventInfo triggeringMotion) {
                    @Nullable PropertyModel model = mModelList.getModelFromTabId(tabId);
                    if (model == null) return;

                    boolean selected = model.get(TabProperties.IS_SELECTED);
                    if (!mIsSingleContextMode
                            && !selected
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
                            selected
                                    ? TabListEditorActionMetricGroups.SELECTED
                                    : TabListEditorActionMetricGroups.UNSELECTED);

                    model.set(TabProperties.IS_SELECTED, !selected);

                    if (mLayoutType != TabListLayoutType.FLAT) {
                        // Reset thumbnail to ensure the color of the blank tab slots is correct.
                        TabModel tabModel = getCurrentTabModelChecked();
                        Tab tab = tabModel.getTabById(tabId);
                        if (tab != null && tabModel.isTabInTabGroup(tab)) {
                            updateThumbnailFetcher(model, tabId);
                        }
                    }
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

                    boolean selected = model.get(TabProperties.IS_SELECTED);
                    model.set(TabProperties.IS_SELECTED, !selected);

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

    private final TabObserver mTabObserver =
            new EmptyTabObserver() {
                @Override
                public void onDidStartNavigationInPrimaryMainFrame(
                        Tab tab, NavigationHandle navigationHandle) {
                    assert mShowingTabs;

                    // The URL of the tab and the navigation handle can match without it being a
                    // same document navigation if the tab had no renderer and needed to start a
                    // new one.
                    // See https://crbug.com/40862141.
                    if (navigationHandle.isSameDocument()
                            || UrlUtilities.isNtpUrl(tab.getUrl())
                            || tab.getUrl().equals(navigationHandle.getUrl())) {
                        return;
                    }
                    @Nullable PropertyModel model = mModelList.getModelFromTabId(tab.getId());
                    if (model == null
                            || (mLayoutType == TabListLayoutType.GROUPED
                                    && getCurrentTabModelChecked().isTabInTabGroup(tab))) {
                        return;
                    }

                    model.set(
                            TabProperties.FAVICON_FETCHER,
                            mTabListFaviconProvider.getDefaultFaviconFetcher(tab.isIncognito()));
                }

                @Override
                public void onTitleUpdated(Tab updatedTab) {
                    assert mShowingTabs;

                    @Nullable PropertyModel model =
                            mModelList.getModelFromTabId(updatedTab.getId());
                    // TODO(crbug.com/40136874) The null check for tab here should be redundant once
                    // we have resolved the bug.
                    if (model == null
                            || getCurrentTabModelChecked().getTabById(updatedTab.getId()) == null) {
                        return;
                    }
                    model.set(
                            TabProperties.TITLE,
                            getLatestTitleForTabOrGroup(updatedTab, model, /* useDefault= */ true));
                }

                @Override
                public void onLoadStarted(Tab tab, boolean toDifferentDocument) {
                    assert mShowingTabs;
                    if (!toDifferentDocument) return;
                    updateLoadingState(tab, true);
                }

                @Override
                public void onLoadStopped(Tab tab, boolean toDifferentDocument) {
                    assert mShowingTabs;
                    if (!toDifferentDocument) return;
                    updateLoadingState(tab, false);
                }

                @Override
                public void onCrash(Tab tab) {
                    assert mShowingTabs;
                    updateLoadingState(tab, false);
                }

                @Override
                public void onFaviconUpdated(
                        Tab updatedTab, @Nullable Bitmap icon, @Nullable GURL iconUrl) {
                    assert mShowingTabs;

                    @Nullable PropertyModel tabInfo = null;
                    @Nullable Tab tab = null;
                    if (mLayoutType == TabListLayoutType.GROUPED && isTabInTabGroup(updatedTab)) {
                        @Nullable Pair<Integer, Tab> indexAndTab =
                                getIndexAndTabForTabGroupId(updatedTab.getTabGroupId());
                        if (indexAndTab == null) return;

                        tabInfo = mModelList.get(indexAndTab.first).model;
                        tab = indexAndTab.second;

                        if (mThumbnailProvider != null) {
                            updateThumbnailFetcher(tabInfo, tab.getId());
                        }
                    } else {
                        tabInfo = mModelList.getModelFromTabId(updatedTab.getId());
                        if (tabInfo == null) return;

                        tab = updatedTab;
                    }

                    updateFaviconForTab(tabInfo, tab, icon, iconUrl);
                }

                @Override
                public void onUrlUpdated(Tab updatedTab) {
                    assert mShowingTabs;

                    @Nullable PropertyModel model =
                            mModelList.getModelFromTabId(updatedTab.getId());
                    @Nullable Tab tab = null;
                    if (model != null) {
                        tab = updatedTab;
                    } else if (mLayoutType != TabListLayoutType.FLAT) {
                        @Nullable Pair<Integer, Tab> indexAndTab =
                                getIndexAndTabForTabGroupId(updatedTab.getTabGroupId());
                        if (indexAndTab != null) {
                            tab = indexAndTab.second;
                            model = mModelList.get(indexAndTab.first).model;
                        }
                    }
                    if (TabUtils.isValid(tab) && model != null) {
                        model.set(TabProperties.URL_DOMAIN, getDomainForTab(tab, model));
                        // Changing URL will result in a thumbnail invalidation if the on-disk
                        // thumbnail doesn't match.
                        updateThumbnailFetcher(model, tab.getId());
                        // Changing URL should also invalidate the favicon.
                        updateFaviconForTab(model, tab, null, null);
                    }
                }

                @Override
                public void onMediaStateChanged(Tab updatedTab, @MediaState int mediaState) {
                    assert mShowingTabs;

                    @Nullable PropertyModel model;
                    Tab representativeTab = updatedTab;
                    boolean isTabGroupTabGrid =
                            mLayoutType == TabListLayoutType.GROUPED && isTabInTabGroup(updatedTab);
                    if (isTabGroupTabGrid) {
                        Token tabGroupId = updatedTab.getTabGroupId();
                        assumeNonNull(tabGroupId);
                        @Nullable Pair<Integer, Tab> indexAndTab =
                                getIndexAndTabForTabGroupId(tabGroupId);
                        if (indexAndTab == null) return;
                        model = mModelList.get(indexAndTab.first).model;
                        representativeTab = indexAndTab.second;
                    } else {
                        model = mModelList.getModelFromTabId(updatedTab.getId());
                    }

                    if (model == null || model.get(TabProperties.USE_SHRINK_CLOSE_ANIMATION)) {
                        return;
                    }
                    model.set(
                            TabProperties.MEDIA_INDICATOR,
                            getTabGridMediaIndicator(representativeTab, model));
                    if (isTabGroupTabGrid) {
                        updateDescriptionString(representativeTab, model);
                    }
                }

                @Override
                public void onTabPinnedStateChanged(Tab tab, boolean isPinned) {
                    int index = mModelList.indexFromTabId(tab.getId());
                    if (index == TabModel.INVALID_TAB_INDEX) return;

                    // When pinning a tab in a group it will be removed from the group so the index
                    // update is unnecessary.
                    if (mLayoutType == TabListLayoutType.FLAT) {
                        updateTab(index, tab, /* isUpdatingId= */ false, /* quickMode= */ false);
                        return;
                    }

                    int finalIndex =
                            mModelList.indexOfNthTabCard(getCurrentTabModelChecked().indexOf(tab));
                    // indexOfNthTabCard returns n + 1 if the index is higher than the number of
                    // tabs in the model list. Moving is implemented as removal then addition.
                    // The last valid index to add to is the size of the model list after the
                    // removal so we need to clamp to mModelList.size() - 1.
                    if (finalIndex == TabModel.INVALID_TAB_INDEX) {
                        mModelList.removeAt(index);
                    } else {
                        ListItem item = mModelList.get(index);
                        mModelList.removeAt(index);
                        finalIndex = Math.min(finalIndex, mModelList.size());
                        // Update properties while the item is detached to avoid temporary view type
                        // mismatch in the adapter and double-notifications (change + remove).
                        updateTab(
                                item.model,
                                finalIndex,
                                tab,
                                /* isUpdatingId= */ false,
                                /* quickMode= */ false);
                        mModelList.add(finalIndex, item);
                    }
                }
            };

    private final TabGroupObserver mTabGroupObserver =
            new TabGroupObserver() {
                @Override
                public void didChangeTabGroupTitle(Token tabGroupId, String newTitle) {
                    assert mShowingTabs;

                    mTabListLayoutDelegate.didChangeTabGroupTitle(tabGroupId, newTitle);
                }

                @Override
                public void didChangeTabGroupColor(
                        Token tabGroupId, @TabGroupColorId int newColor) {
                    assert mShowingTabs;

                    mTabListLayoutDelegate.didChangeTabGroupColor(tabGroupId, newColor);
                }

                @Override
                public void didChangeTabGroupCollapsed(
                        Token tabGroupId, boolean isCollapsed, boolean animate) {
                    assert mShowingTabs;

                    mTabListLayoutDelegate.didChangeTabGroupCollapsed(
                            tabGroupId, isCollapsed, animate);
                }

                @Override
                public void didMoveWithinGroup(
                        Tab movedTab, int tabModelOldIndex, int tabModelNewIndex) {
                    assert mShowingTabs;
                    if (tabModelNewIndex == tabModelOldIndex) return;

                    mTabListLayoutDelegate.didMoveWithinGroup(
                            movedTab, tabModelOldIndex, tabModelNewIndex);
                }

                @Override
                public void didMoveTabOutOfGroup(Tab movedTab, int prevFilterIndex) {
                    assert mShowingTabs;
                    assert mTabGridDialogHandler == null || mLayoutType == TabListLayoutType.FLAT;

                    mTabListLayoutDelegate.didMoveTabOutOfGroup(movedTab, prevFilterIndex);
                }

                @Override
                public void didMergeTabToGroup(Tab movedTab, boolean isDestinationTab) {
                    assert mShowingTabs;

                    mTabListLayoutDelegate.didMergeTabToGroup(movedTab, isDestinationTab);
                }

                @Override
                public void didMoveTabGroup(
                        Tab movedTab, int tabModelOldIndex, int tabModelNewIndex) {
                    assert mShowingTabs;
                    if (tabModelNewIndex == tabModelOldIndex) {
                        return;
                    }

                    mTabListLayoutDelegate.didMoveTabGroup(
                            movedTab, tabModelOldIndex, tabModelNewIndex);
                }

                @Override
                public void didCreateNewGroup(Tab destinationTab, TabModel tabModel) {
                    mTabListLayoutDelegate.didCreateNewGroup(destinationTab, tabModel);
                }

                @Override
                public void didRemoveTabGroup(
                        int oldRootId,
                        @Nullable Token oldTabGroupId,
                        @DidRemoveTabGroupReason int removalReason) {
                    assert mShowingTabs;

                    mTabListLayoutDelegate.didRemoveTabGroup(
                            oldRootId, oldTabGroupId, removalReason);
                }
            };

    /**
     * Construct the Mediator with the given Models and observing hooks from the given
     * ChromeActivity.
     *
     * @param activity The activity used to get some configuration information.
     * @param modelList The {@link TabListModel} to keep state about a list of {@link Tab}s.
     * @param mode The {@link TabListMode}
     * @param modalDialogManager The {@link ModalDialogManager} for managing dialog lifecycles.
     * @param tabModelSupplier Used to fetch the filter that provides tab group information.
     * @param thumbnailProvider {@link ThumbnailProvider} to provide screenshot related details.
     * @param tabListFaviconProvider Provider for all favicon related drawables.
     * @param selectionDelegateProvider Provider for a {@link SelectionDelegate} that is used for a
     *     selectable list. It's null when selection is not possible.
     * @param tabListItemOnClickListenerProvider Provides click listeners for regular tabs and tab
     *     group cards.
     * @param tabListConfigDelegate Delegate providing configuration policies and visual
     *     capabilities (e.g. nested tab groups, message cards, etc).
     * @param dialogHandler A handler to handle requests about updating TabGridDialog.
     * @param priceWelcomeMessageControllerSupplier A supplier of a controller to show
     *     PriceWelcomeMessage.
     * @param componentId The {@link TabComponentId} identifying the parent UI container hosting
     *     this tab list.
     * @param initialTabActionState The initial {@link TabActionState} to use for the shown tabs.
     *     Must always be CLOSABLE for TabListMode.BOTTOM_STRIP.
     * @param dataSharingTabManager The service used to initiate data sharing.
     * @param onTabGroupCreation Should be run when the UI is used to create a tab group.
     * @param undoBarExplicitTrigger Interface to explicitly trigger the undo closure snackbar.
     * @param snackbarManager The manager to show snackbars.
     * @param allowedSelectionCount The maximum number of tabs that can be selected at once.
     */
    public TabListMediator(
            Activity activity,
            TabListModel modelList,
            @TabListMode int mode,
            @Nullable ModalDialogManager modalDialogManager,
            NullableObservableSupplier<TabModel> tabModelSupplier,
            @Nullable ThumbnailProvider thumbnailProvider,
            TabListFaviconProvider tabListFaviconProvider,
            @Nullable SelectionDelegateProvider<TabListEditorItemSelectionId>
                    selectionDelegateProvider,
            @Nullable TabListItemOnClickListenerProvider tabListItemOnClickListenerProvider,
            TabListConfigDelegate tabListConfigDelegate,
            @Nullable TabGridDialogHandler dialogHandler,
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
        mMode = mode;
        mModalDialogManager = modalDialogManager;
        mCurrentTabModelSupplier = tabModelSupplier;
        mThumbnailProvider = thumbnailProvider;
        mTabListFaviconProvider = tabListFaviconProvider;
        mSelectionDelegateProvider = selectionDelegateProvider;
        mTabListItemOnClickListenerProvider = tabListItemOnClickListenerProvider;
        mLayoutType = tabListConfigDelegate.getLayoutType();
        mSupportsMessageCards = tabListConfigDelegate.supportsMessageCards();
        mTabHoverCardListener = tabListConfigDelegate.getTabHoverCardListener();
        mTabGridDialogHandler = dialogHandler;
        mPriceWelcomeMessageControllerSupplier = priceWelcomeMessageControllerSupplier;
        mComponentId = componentId;
        mTabActionState = initialTabActionState;
        mDataSharingTabManager = dataSharingTabManager;
        mOnTabGroupCreation = onTabGroupCreation;
        mUndoBarExplicitTrigger = undoBarExplicitTrigger;
        mSnackbarManager = snackbarManager;
        mAllowedSelectionCount = allowedSelectionCount;
        mIsSingleContextMode = isSingleContextMode;

        switch (mLayoutType) {
            case TabListLayoutType.FLAT:
                mTabListLayoutDelegate =
                        new FlatLayoutDelegate(this, mModelList, mTabGridDialogHandler);
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

        mTabModelObserver =
                new TabModelObserver() {
                    @Override
                    public void didSelectTab(Tab tab, @TabSelectionType int type, int lastId) {
                        assert mShowingTabs;

                        mNextTabId = Tab.INVALID_TAB_ID;
                        int tabId = tab.getId();
                        if (tabId == lastId) return;

                        int oldIndex = mModelList.indexFromTabId(lastId);
                        if (oldIndex == TabModel.INVALID_TAB_INDEX
                                && mLayoutType == TabListLayoutType.GROUPED) {
                            oldIndex = getIndexForTabIdWithRelatedTabs(lastId);
                        }
                        int newIndex = mModelList.indexFromTabId(tabId);
                        if (newIndex == TabModel.INVALID_TAB_INDEX
                                && mLayoutType == TabListLayoutType.GROUPED) {
                            // If a tab in tab group does not exist in model and needs to be
                            // selected, identify the related tab ids and determine newIndex
                            // based on if any of the related ids are present in model.
                            newIndex = getIndexForTabIdWithRelatedTabs(tabId);
                            // For UNDO ensure we update the representative tab in the model.
                            if (type == TabSelectionType.FROM_UNDO
                                    && newIndex != Tab.INVALID_TAB_ID) {
                                mModelList.updateTabListModelIdForGroup(tab, newIndex);
                            }
                        }

                        mLastSelectedTabListModelIndex = oldIndex;
                        if (mTabToAddDelayed != null && mTabToAddDelayed == tab) {
                            // If tab is being added later, it will be selected later.
                            return;
                        }
                        selectTab(oldIndex, newIndex);
                    }

                    @Override
                    public void tabClosureCommitted(Tab tab) {
                        sTabClosedFromMap.remove(tab.getId());
                    }

                    @Override
                    public void tabClosureUndone(Tab tab) {
                        assert mShowingTabs;

                        addObserversForTab(tab);
                        onTabAdded(tab);

                        if (sTabClosedFromMap.containsKey(tab.getId())) {
                            @TabClosedFrom int from = sTabClosedFromMap.get(tab.getId());
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
                                    RecordUserAction.record(
                                            "Android.VerticalTabs.UndoCloseTabGroup");
                                    break;
                                default:
                                    assert false
                                            : "tabClosureUndone for tab that closed from an unknown"
                                                    + " UI";
                            }
                            sTabClosedFromMap.remove(tab.getId());
                        }
                        // TODO(yuezhanggg): clean up updateTab() calls in this class.
                        if (mLayoutType == TabListLayoutType.GROUPED) {
                            TabModel tabModel = getCurrentTabModelChecked();
                            int filterIndex = tabModel.representativeIndexOf(tab);
                            if (filterIndex == TabList.INVALID_TAB_INDEX
                                    || !tabModel.isTabInTabGroup(tab)
                                    || filterIndex >= mModelList.size()) {
                                return;
                            }
                            Tab currentGroupSelectedTab =
                                    tabModel.getRepresentativeTabAt(filterIndex);
                            assumeNonNull(currentGroupSelectedTab);

                            int tabListModelIndex = mModelList.indexOfNthTabCard(filterIndex);
                            assert mModelList.indexFromTabId(currentGroupSelectedTab.getId())
                                    == tabListModelIndex;

                            updateTab(tabListModelIndex, currentGroupSelectedTab, false, false);
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
                        boolean isGridOrDialogComponent =
                                mComponentId == TabComponentId.GRID_TAB_SWITCHER
                                        || mComponentId == TabComponentId.TAB_GRID_DIALOG_FROM_STRIP
                                        || mComponentId
                                                == TabComponentId.TAB_GRID_DIALOG_IN_SWITCHER;
                        boolean delayAdd =
                                isSupportedLaunchType
                                        && markedForSelection
                                        && isGridOrDialogComponent;
                        if (delayAdd) {
                            mTabToAddDelayed = tab;
                            return;
                        }

                        onTabAdded(tab);
                        if (type == TabLaunchType.FROM_RESTORE
                                && mLayoutType != TabListLayoutType.FLAT) {
                            // When tab is restored after restoring stage (e.g. exiting multi-window
                            // mode, switching between dark/light mode in incognito), we need to
                            // update related property models.
                            int filterIndex = tabModel.representativeIndexOf(tab);
                            if (filterIndex == TabList.INVALID_TAB_INDEX) return;
                            Tab currentGroupSelectedTab =
                                    tabModel.getRepresentativeTabAt(filterIndex);
                            assumeNonNull(currentGroupSelectedTab);
                            // TabModel and TabListModel may be in the process of syncing up through
                            // restoring. Examples of this situation are switching between
                            // light/dark mode in incognito, exiting multi-window mode, etc.
                            if (mLayoutType == TabListLayoutType.NESTED) {
                                int tabUiIndex = mModelList.indexFromTabId(tab.getId());
                                if (tabUiIndex != TabModel.INVALID_TAB_INDEX) {
                                    updateTab(tabUiIndex, tab, false, false);
                                }
                                if (tab.getTabGroupId() != null) {
                                    updateTabGroupTitle(tab.getTabGroupId());
                                }
                                return;
                            }

                            int tabListModelIndex = mModelList.indexOfNthTabCard(filterIndex);
                            if (mModelList.indexFromTabId(currentGroupSelectedTab.getId())
                                    != tabListModelIndex) {
                                return;
                            }
                            updateTab(tabListModelIndex, currentGroupSelectedTab, false, false);
                        }
                    }

                    @Override
                    public void didMoveTab(Tab tab, int newIndex, int curIndex) {
                        assert mShowingTabs;

                        // Standalone tab moves triggered from external sources need to be
                        // explicitly synced to the ModelList for GROUPED and NESTED layouts.
                        if (mLayoutType == TabListLayoutType.FLAT) return;

                        // Intra-group move or merging into group.
                        if (tab.getTabGroupId() != null) {
                            return;
                        }

                        int currentUiIndex = mModelList.indexFromTabId(tab.getId());
                        if (currentUiIndex == TabModel.INVALID_TAB_INDEX) return;

                        // Moving out of a group.
                        // This assumes the move event is dispatched before the ungroup event
                        // (didMoveTabOutOfGroup) is processed, meaning the UI model still has the
                        // old grouping metadata.
                        PropertyModel model = mModelList.get(currentUiIndex).model;
                        if (TabProperties.isTabInGroup(model)
                                || TabProperties.isTabGroupHeader(model)) {
                            return;
                        }

                        // Standalone tab movement.
                        int targetUiIndex = mTabListLayoutDelegate.getInsertionIndexOfTab(tab);
                        mModelList.moveItem(currentUiIndex, targetUiIndex);
                    }

                    @Override
                    public void didRemoveTabForClosure(Tab tab) {
                        onTabClose(tab);
                    }

                    private void onTabClose(Tab tab) {
                        assert mShowingTabs;

                        removeObserversForTab(tab);

                        TabModel tabModel = mCurrentTabModelSupplier.get();
                        Token tabGroupId = tab.getTabGroupId();
                        if (tabModel != null
                                && tabGroupId != null
                                && tabModel.tabGroupExists(tabGroupId)) {
                            if (mLayoutType == TabListLayoutType.GROUPED) {
                                // If the tab closed was part of a tab group and the closure was
                                // triggered from a grouped layout, update the group to reflect the
                                // closure instead of closing the tab.
                                int groupIndex = tabModel.representativeIndexOf(tab);
                                Tab groupTab = tabModel.getRepresentativeTabAt(groupIndex);
                                assumeNonNull(groupTab);
                                if (!groupTab.isClosing()) {
                                    updateTab(
                                            mModelList.indexOfNthTabCard(groupIndex),
                                            groupTab,
                                            /* isUpdatingId= */ true,
                                            /* quickMode= */ false);
                                    return;
                                }
                            } else if (mLayoutType == TabListLayoutType.NESTED) {
                                updateTabGroupHeaderId(tabGroupId);
                                updateTabGroupTitle(tabGroupId);
                            }
                        }

                        int index = mModelList.indexFromTabId(tab.getId());
                        if (index == TabModel.INVALID_TAB_INDEX) return;

                        mModelList.removeAt(index);
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

                        if (mLayoutType == TabListLayoutType.NESTED) {
                            // The last tab is clipped from top during animation.
                            if (view != null && view.getParent() instanceof View rootItemView) {
                                boolean isLastTab = closingTabIndex == mModelList.size() - 1;
                                rootItemView.setTag(R.id.tab_clip_from_top, isLastTab);
                            }
                        }

                        TabModel tabModel = getCurrentTabModelChecked();
                        Tab closingTab = tabModel.getTabById(tabId);
                        if (closingTab == null) return;

                        @TabClosingSource
                        int tabClosingSource =
                                mComponentId == TabComponentId.VERTICAL_TABS
                                        ? TabClosingSource.VERTICAL_TAB_STRIP
                                        : TabClosingSource.UNKNOWN;

                        setUseShrinkCloseAnimation(tabId, /* useShrinkCloseAnimation= */ true);
                        if (mLayoutType == TabListLayoutType.GROUPED
                                && tabModel.isTabInTabGroup(closingTab)) {
                            onGroupClosedFrom(tabId);

                            // TODO(crbug.com/375468032): use "triggeringMotion" to determine
                            //  if the "undo" snackbar should be shown when closing a tab group.
                            TabUiUtils.closeTabGroup(
                                    tabModel,
                                    tabId,
                                    tabClosingSource,
                                    /* allowUndo= */ true,
                                    /* hideTabGroups= */ true,
                                    getOnMaybeTabClosedCallback(tabId));
                            return;
                        }

                        onTabClosedFrom(tabId, mComponentId);
                        boolean allowUndo = TabClosureParamsUtils.shouldAllowUndo(triggeringMotion);
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
                                            () -> {
                                                mRecyclerViewItemAnimationToggle
                                                        .setDisableItemAnimations(false);
                                            });
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
                        activity, assumeNonNull(modalDialogManager), mOnTabGroupCreation);
        mTabGridItemTouchHelperCallback =
                new TabGridItemTouchHelperCallback(
                        activity,
                        tabGroupCreationDialogManager,
                        mModelList,
                        () -> assertNonNull(mCurrentTabModelSupplier.get()),
                        swipeSafeTabActionListener,
                        mTabGridDialogHandler,
                        TabUiMetricsHelper.getComponentNameForMetrics(componentId),
                        mLayoutType,
                        onDragStateChangedListener);

        mRailCollapseStateSupplier = tabListConfigDelegate.getRailCollapseStateSupplier();
        if (mRailCollapseStateSupplier != null) {
            mRailCollapseStateObserver = this::onRailCollapseStateChanged;
            mRailCollapseStateSupplier.addSyncObserverAndCallIfNonNull(mRailCollapseStateObserver);
        } else {
            mRailCollapseStateObserver = null;
        }
    }

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

    private void selectTab(int oldIndex, int newIndex) {
        // TODO(crbug.com/347886633): Change the bounds check to an assert.
        if (oldIndex != TabModel.INVALID_TAB_INDEX && oldIndex < mModelList.size()) {
            PropertyModel oldModel = mModelList.get(oldIndex).model;
            int lastId = oldModel.get(TAB_ID);
            oldModel.set(TabProperties.IS_SELECTED, false);
            if (mLayoutType != TabListLayoutType.FLAT
                    && mThumbnailProvider != null
                    && mShowingTabs) {
                updateThumbnailFetcher(oldModel, lastId);
            }
        }

        if (newIndex != TabModel.INVALID_TAB_INDEX) {
            PropertyModel newModel = mModelList.get(newIndex).model;
            int newId = newModel.get(TAB_ID);
            newModel.set(TabProperties.IS_SELECTED, true);
            if (mThumbnailProvider != null && mShowingTabs) {
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
        if (mMode == TabListMode.GRID
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
        sTabClosedFromMap.put(tabId, from);
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
        sTabClosedFromMap.put(tabId, from);
    }

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

    // TODO(crbug.com/509226293): Move this to NestedLayoutDelegate.
    /**
     * Ensures that a group header exists in NESTED layout. If not, it creates and inserts one.
     *
     * @param tab A representative tab of the group.
     * @param tabGroupId The group ID.
     * @param targetUiIndex The UI index where the header should be inserted if missing.
     * @return true if a header was created and inserted, false otherwise.
     */
    boolean ensureGroupHeaderExistsInNestedLayout(Tab tab, Token tabGroupId, int targetUiIndex) {
        if (mLayoutType != TabListLayoutType.NESTED
                || tabGroupId == null
                || targetUiIndex == TabModel.INVALID_TAB_INDEX) {
            return false;
        }

        if (mModelList.indexFromTabGroupId(tabGroupId) == TabModel.INVALID_TAB_INDEX) {
            addTabInfoToModelForGroup(tab, tabGroupId, targetUiIndex);
            return true;
        }

        return false;
    }

    int onTabAdded(Tab tab) {
        int existingIndex = mModelList.indexFromTabId(tab.getId());
        if (existingIndex != TabModel.INVALID_TAB_INDEX) return existingIndex;

        int newIndex = mTabListLayoutDelegate.getInsertionIndexOfTab(tab);

        // Tabs should be inserted only after the archived message card.
        if (newIndex == 0 && isOnlyArchivedMsg(mModelList)) newIndex++;

        if (mLayoutType == TabListLayoutType.NESTED && isTabInTabGroup(tab)) {
            Token groupId = tab.getTabGroupId();
            if (groupId != null) {
                if (ensureGroupHeaderExistsInNestedLayout(tab, groupId, newIndex)) {
                    newIndex++;
                }
                updateTabGroupTitle(groupId);

                if (newIndex == TabList.INVALID_TAB_INDEX
                        || getCurrentTabModelChecked().getTabGroupCollapsed(groupId)) {
                    return newIndex;
                }

                TabModel tabModel = getCurrentTabModelChecked();
                addTabInfoToModelForTab(
                        tab, newIndex, TabModelUtils.getCurrentTabId(tabModel) == tab.getId());
                return newIndex;
            }
        }

        if (newIndex == TabList.INVALID_TAB_INDEX) return newIndex;

        addTabCardToModel(tab, newIndex);
        return newIndex;
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
                    Tab previousTab = getCurrentTabModelChecked().getTabById(modelTabId);
                    // If the tab is in the same tab group, we can just update the model's TAB_ID
                    // rather than resetting the list.
                    if (mLayoutType != TabListLayoutType.FLAT
                            && previousTab != null
                            && Objects.equals(previousTab.getTabGroupId(), tab.getTabGroupId())) {
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
                if (index < 0 || index >= mModelList.size()) continue;
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
            int index = onTabAdded(mTabToAddDelayed);
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
        if (index < 0 || index >= mModelList.size()) return;

        updateTab(mModelList.get(index).model, index, tab, isUpdatingId, quickMode);
    }

    private void updateTab(
            PropertyModel model, int index, Tab tab, boolean isUpdatingId, boolean quickMode) {
        if (isUpdatingId) {
            model.set(TabProperties.TAB_ID, tab.getId());
        } else {
            // Group Header's TAB_ID is not required to match the active child's ID when nesting is
            // supported.
            assert mLayoutType == TabListLayoutType.NESTED
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
        model.set(TabProperties.MEDIA_INDICATOR, getTabGridMediaIndicator(tab, model));

        bindTabActionStateProperties(model.get(TabProperties.TAB_ACTION_STATE), tab, model);

        model.set(TabProperties.URL_DOMAIN, getDomainForTab(tab, model));

        setupPersistedTabDataFetcherForTab(tab, model);

        updateFaviconForTab(model, tab, null, null);

        ActorUiTabController controller = ActorUiTabController.from(tab);
        updateActorUiState(model, controller == null ? null : controller.getUiTabState());

        boolean forceUpdate = isTabSelected && !quickMode;
        boolean forceUpdateLastSelected =
                mLayoutType != TabListLayoutType.FLAT
                        && index == mLastSelectedTabListModelIndex
                        && !quickMode;
        // TODO(crbug.com/40273706): Fetching thumbnail for group is expansive, we should consider
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

    @VisibleForTesting
    public boolean isTabInTabGroup(Tab tab) {
        TabModel tabModel = getCurrentTabModelChecked();
        assert tabModel.isTabModelRestored();

        return tabModel.isTabInTabGroup(tab);
    }

    // TODO(crbug.com/509226293): Delegate media state resolution to TabListLayoutDelegate.
    private @MediaState int getTabGridMediaIndicator(Tab representativeTab, PropertyModel model) {
        if (mLayoutType == TabListLayoutType.NESTED) {
            if (TabProperties.isTabGroupHeader(model)) {
                return MediaState.NONE;
            }
            // For nested layout child tabs, we do not aggregate the media state of the entire
            // group.
            return representativeTab.getMediaState();
        }

        @MediaState int stateToReturn = representativeTab.getMediaState();
        // If the tab is not in a group, or the  state has the highest priority, then return
        // the state of the representative tab.
        if (mLayoutType == TabListLayoutType.FLAT
                || !isTabInTabGroup(representativeTab)
                || stateToReturn == MediaState.MAX_VALUE) {
            return stateToReturn;
        }

        List<Tab> relatedTabs = getRelatedTabsForId(representativeTab.getId());
        for (Tab tab : relatedTabs) {
            @MediaState int currentState = tab.getMediaState();
            if (currentState > stateToReturn) {
                stateToReturn = currentState;
            }
            if (stateToReturn == MediaState.MAX_VALUE) return stateToReturn;
        }
        return stateToReturn;
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
                        if (mMode == TabListMode.GRID
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
     * Setup the {@link View.AccessibilityDelegate} for grid layout.
     *
     * @param helper The {@link TabGridAccessibilityHelper} used to setup accessibility support.
     */
    @Initializer
    void setupAccessibilityDelegate(TabGridAccessibilityHelper helper) {
        mAccessibilityDelegate =
                new View.AccessibilityDelegate() {
                    @Override
                    public void onInitializeAccessibilityNodeInfo(
                            View host, AccessibilityNodeInfo info) {
                        super.onInitializeAccessibilityNodeInfo(host, info);
                        for (AccessibilityAction action : helper.getPotentialActionsForView(host)) {
                            info.addAction(action);
                        }
                    }

                    @Override
                    public boolean performAccessibilityAction(
                            View host, int action, @Nullable Bundle args) {
                        if (!helper.isReorderAction(action)) {
                            return super.performAccessibilityAction(host, action, args);
                        }

                        Pair<Integer, Integer> positions =
                                helper.getPositionsOfReorderAction(host, action);
                        int currentPosition = positions.first;
                        int targetPosition = positions.second;
                        if (!mModelList.isValidIndex(currentPosition)
                                || !mModelList.isValidIndex(targetPosition)) {
                            return false;
                        }
                        mModelList.move(currentPosition, targetPosition);
                        RecordUserAction.record("TabGrid.AccessibilityDelegate.Reordered");
                        return true;
                    }
                };
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

        if (mGlicIndicatorManager != null) {
            mGlicIndicatorManager.destroy();
            mGlicIndicatorManager = null;
        }
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
                            () -> getCurrentTabModelChecked(),
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
        return tabActionState != TabActionState.SELECTABLE
                ? mContextClickTabItemEventListener
                : null;
    }

    /** Gets or lazily initializes the Glic underline indicator manager. */
    private @Nullable StripTabUnderlineManager getOrInitGlicIndicatorManager(Tab tab) {
        boolean isGlicOrContextualTasksEnabled =
                GlicEnabling.isEnabledByFlags() || ChromeFeatureList.sContextualTasks.isEnabled();
        if (mMode != TabListMode.VERTICAL || tab.isIncognito() || !isGlicOrContextualTasksEnabled) {
            return null;
        }
        if (mGlicIndicatorManager == null) {
            WindowAndroid windowAndroid = tab.getWindowAndroid();
            if (windowAndroid != null) {
                mGlicIndicatorManager = new StripTabUnderlineManager(windowAndroid);
                mGlicIndicatorManager.addObserver(mGlicObserver);
            }
        }
        return mGlicIndicatorManager;
    }

    @TabActionState
    int getTabActionState() {
        return mTabActionState;
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
        model.set(TabProperties.TAB_HOVER_CARD_LISTENER, mTabHoverCardListener);

        if (mTabActionState != TabActionState.SELECTABLE) {
            updateDescriptionString(tab, model);
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
                || mLayoutType == TabListLayoutType.FLAT) {
            tabSelectedListener = mTabSelectedListener;
        } else {
            tabSelectedListener = mTabListItemOnClickListenerProvider.onTabGroupClicked(tab);
            if (tabSelectedListener == null) {
                tabSelectedListener = mTabSelectedListener;
            }
        }
        return tabSelectedListener;
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

    void addTabCardToModel(Tab tab, int index) {
        boolean isTabGroup = isTabInTabGroup(tab) && mLayoutType != TabListLayoutType.FLAT;
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
                        .build();

        ActorUiTabController controller = ActorUiTabController.from(tab);
        updateActorUiState(tabInfo, controller == null ? null : controller.getUiTabState());

        // Tab group representation cards default to a collapsed state. In standard GTS, this
        // property is conceptually permanently collapsed, while in Vertical Tabs, it acts as the
        // dynamic accordion state toggle for inline child tab row display.
        boolean isTabGroup = isTabInTabGroup(tab) && mLayoutType != TabListLayoutType.FLAT;
        if (isTabGroup) {
            tabInfo.set(TabProperties.IS_COLLAPSED, true);
        }

        if (mRailCollapseStateSupplier != null) {
            tabInfo.set(TabProperties.RAIL_COLLAPSE_STATE, mRailCollapseStateSupplier.get());
        }

        @UiType int tabUiType = mMode == TabListMode.BOTTOM_STRIP ? UiType.STRIP : UiType.TAB;
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

    void addTabInfoToModelForTab(Tab tab, int index, boolean isSelected) {
        assert index != TabModel.INVALID_TAB_INDEX;

        PropertyModel tabInfo = addTabInfoToModel(tab, index, isSelected, ModelType.TAB);

        mTabListLayoutDelegate.setupGroupPropertiesForChildTab(tab, tabInfo);
        tabInfo.set(
                TabProperties.TITLE,
                getLatestTitleForTabOrGroup(tab, tabInfo, /* useDefault= */ false));
        tabInfo.set(TabProperties.URL_DOMAIN, getDomainForTab(tab, tabInfo));
        tabInfo.set(TabProperties.MEDIA_INDICATOR, getTabGridMediaIndicator(tab, tabInfo));
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

        if (mThumbnailProvider != null && mShowingTabs) {
            updateThumbnailFetcher(tabInfo, tab.getId());
        }
    }

    private void addTabInfoToModelForGroup(Tab tab, Token tabGroupId, int index) {
        assert index != TabModel.INVALID_TAB_INDEX;
        assumeNonNull(tabGroupId);
        TabModel tabModel = getCurrentTabModelChecked();
        @TabGroupColorId int colorId = tabModel.getTabGroupColorWithFallback(tabGroupId);
        int currentTabId = TabModelUtils.getCurrentTabId(tabModel);

        boolean isCollapsed =
                mLayoutType != TabListLayoutType.NESTED
                        || tabModel.getTabGroupCollapsed(tabGroupId);
        // If the group is collapsed, the group representation card displays the selection.
        // If expanded, the group card is a header and should remain unhighlighted (child rows show
        // selection).
        boolean isSelected = isCollapsed && isSelectedTab(tab, currentTabId);

        int cardType =
                mLayoutType == TabListLayoutType.NESTED ? ModelType.TAB_GROUP : ModelType.TAB;
        PropertyModel groupInfo = addTabInfoToModel(tab, index, isSelected, cardType);

        // Group Header Specific properties
        groupInfo.set(TabProperties.TAB_GROUP_ID, null);
        updateTabGroupProperties(tab, groupInfo, colorId);
        groupInfo.set(
                TabProperties.TITLE,
                getLatestTitleForTabOrGroup(tab, groupInfo, /* useDefault= */ true));
        groupInfo.set(TabProperties.IS_COLLAPSED, isCollapsed);
        groupInfo.set(TabProperties.FAVICON_FETCHER, null);

        bindTabActionStateProperties(mTabActionState, tab, groupInfo);

        if (mThumbnailProvider != null && mShowingTabs) {
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

        if (mThumbnailProvider != null) {
            updateThumbnailFetcher(tabGroupInfo, savedTabGroup);
        }
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

    private String getDomainForTab(Tab tab, PropertyModel model) {
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

    void updateDescriptionString(Tab tab, PropertyModel model) {
        if (mLayoutType == TabListLayoutType.FLAT) return;
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
                    String mediaStateString =
                            getMediaStateAccessibilityString(currentTab, model, res);
                    if (!TextUtils.isEmpty(mediaStateString)) {
                        description += " " + mediaStateString;
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
        TextResolver descriptionTextResolver;
        if (mLayoutType != TabListLayoutType.FLAT) {
            boolean isTabGroup = TabProperties.isTabGroupHeader(model);
            int numOfRelatedTabs = getRelatedTabsForId(tab.getId()).size();
            if (isTabGroup) {
                String title = getLatestTitleForTabOrGroup(tab, model, /* useDefault= */ false);

                descriptionTextResolver =
                        getActionButtonDescriptionTextResolver(numOfRelatedTabs, title, tab);
                model.set(
                        TabProperties.ACTION_BUTTON_DESCRIPTION_TEXT_RESOLVER,
                        descriptionTextResolver);
                return;
            }
        }

        descriptionTextResolver =
                (context) -> {
                    return context.getString(
                            R.string.accessibility_tabstrip_btn_close_tab, getTabTitleOrUrl(tab));
                };
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
            isTabGroup = mLayoutType != TabListLayoutType.FLAT && isTabInTabGroup(tab);
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

    int selectedTabId() {
        if (mNextTabId != Tab.INVALID_TAB_ID) {
            return mNextTabId;
        }

        return TabModelUtils.getCurrentTabId(getCurrentTabModelChecked());
    }

    private void setupPersistedTabDataFetcherForTab(Tab tab, PropertyModel model) {
        if (mSupportsMessageCards && !tab.isIncognito()) {
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

    private void updateLoadingState(Tab tab, boolean isLoading) {
        if (mMode != TabListMode.VERTICAL || !mShowingTabs) return;
        @Nullable PropertyModel model = mModelList.getModelFromTabId(tab.getId());
        if (model == null) return;
        // Suppress loading indicator for NTP. NTP loads instantly, but the brief load events can
        // trigger visible flickers in Android Views, or get stuck if background tab loading is
        // deferred.
        boolean shouldShowLoadingIndicator = !UrlUtilities.isNtpUrl(tab.getUrl()) && isLoading;
        model.set(TabProperties.IS_LOADING, shouldShowLoadingIndicator);
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
        if (!mSupportsMessageCards) {
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
        if (!mSupportsMessageCards) {
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
        if (mMode != TabListMode.GRID
                || getCurrentTabModelChecked().isIncognitoBranded()
                || mLayoutType == TabListLayoutType.FLAT
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
     * Returns the index in {@link mModelList} of the group with {@code tabGroupId} and the {@link
     * Tab} representing the group. Will be null if the entry is not present, the tab cannot be
     * found, or the tab is not part of a tab group.
     */
    @Nullable Pair<Integer, Tab> getIndexAndTabForTabGroupId(@Nullable Token tabGroupId) {
        if (tabGroupId == null) return null;

        TabModel tabModel = getCurrentTabModelChecked();
        @TabId int lastShownTabId = tabModel.getGroupLastShownTabId(tabGroupId);

        int index = getIndexForTabIdWithRelatedTabs(lastShownTabId);
        if (index == TabModel.INVALID_TAB_INDEX) return null;

        Tab tab = getTabForIndex(index);
        // If the found tab has a different group ID from the tabGroupId set in the args then the
        // update is likely for a group that no longer exists so we should drop the update.
        if (tab == null
                || !tabGroupId.equals(tab.getTabGroupId())
                || !tabModel.isTabInTabGroup(tab)) {
            return null;
        }
        return Pair.create(index, tab);
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

    private @Nullable Tab getTabForIndex(int index) {
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

    void addObserversForTab(Tab tab) {
        tab.addObserver(mTabObserver);

        ActorUiTabController controller = ActorUiTabController.from(tab);
        if (controller != null) {
            controller.addObserver(mActorObserver);

            @Nullable PropertyModel model = mModelList.getModelFromTabId(tab.getId());
            if (model != null) {
                updateActorUiState(model, controller.getUiTabState());
            }
        }

        StripTabUnderlineManager glicIndicatorManager = getOrInitGlicIndicatorManager(tab);
        if (glicIndicatorManager != null) {
            glicIndicatorManager.registerTab(tab);
        }
    }

    private void removeObserversForTab(Tab tab) {
        tab.removeObserver(mTabObserver);

        ActorUiTabController controller = ActorUiTabController.from(tab);
        if (controller != null) controller.removeObserver(mActorObserver);

        if (mGlicIndicatorManager != null) {
            mGlicIndicatorManager.unregisterTab(tab.getId());
        }
    }

    private void addObservers(TabModel tabModel, List<Tab> tabs) {
        if (mLayoutType != TabListLayoutType.FLAT) {
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
        tabModel.addTabGroupObserver(mTabGroupObserver);
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
        tabModel.removeTabGroupObserver(mTabGroupObserver);
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
        assert mMode == TabListMode.BOTTOM_STRIP;

        Callback<PropertyModel> updateTabStripItemCallback =
                (model) -> {
                    model.set(TabProperties.HAS_NOTIFICATION_BUBBLE, hasUpdate);
                };

        forAllTabListItems(tabIdsToBeUpdated, updateTabStripItemCallback);
    }

    @Override
    public void updateTabCardLabels(Map<Integer, TabCardLabelData> labelData) {
        assert mMode == TabListMode.GRID;

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
                Tab tab = getCurrentTabModelChecked().getTabById(tabId);
                assumeNonNull(tab);
                updateDescriptionString(tab, model);
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
                filteredTabs.add(groupTab);
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

            @TabClosingSource
            int tabClosingSource =
                    mComponentId == TabComponentId.VERTICAL_TABS
                            ? TabClosingSource.VERTICAL_TAB_STRIP
                            : TabClosingSource.UNKNOWN;

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
        if (mMode != TabListMode.GRID) return;

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
                sTabClosedFromMap.remove(tabId);
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
            if (index != TabModel.INVALID_TAB_INDEX) {
                if (mMode == TabListMode.GRID) {
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
        if (index < 0 || index >= mModelList.size()) return;
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

    void updateTabGroupTitle(Token tabGroupId) {
        @Nullable Pair<Integer, Tab> headerIndexAndTab = getIndexAndTabForTabGroupId(tabGroupId);
        if (headerIndexAndTab == null) return;
        PropertyModel headerModel = mModelList.get(headerIndexAndTab.first).model;
        Tab tab = headerIndexAndTab.second;
        // Do not trust the `newTitle`, it may be necessary to apply a default/fallback.
        String title = getLatestTitleForTabOrGroup(tab, headerModel, /* useDefault= */ true);
        headerModel.set(TabProperties.TITLE, title);
        updateDescriptionString(tab, headerModel);
        updateActionButtonDescriptionString(tab, headerModel);
    }

    void clearTabGroupProperties(PropertyModel model) {
        @Nullable TabGroupColorViewProvider provider = model.get(TAB_GROUP_COLOR_VIEW_PROVIDER);
        model.set(TabProperties.TAB_GROUP_ID, null);
        model.set(TabProperties.TAB_GROUP_CARD_COLOR, null);
        model.set(TabProperties.TAB_GROUP_HEADER_ID, null);
        model.set(TAB_GROUP_COLOR_VIEW_PROVIDER, null);
        if (provider != null) provider.destroy();
    }

    void updateTabGroupProperties(Tab tab, PropertyModel model, @TabGroupColorId int colorId) {
        @Nullable Token tabGroupId = tab.getTabGroupId();
        if (mLayoutType == TabListLayoutType.FLAT || tabGroupId == null || !isTabInTabGroup(tab)) {
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
        assert mMode != TabListMode.BOTTOM_STRIP
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

        int limitCount = mIsSingleContextMode ? 1 : 10;
        String message =
                mActivity
                        .getResources()
                        .getQuantityString(
                                R.plurals.tab_item_picker_limit_reached, limitCount, limitCount);

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

    private String getMediaStateAccessibilityString(Tab tab, PropertyModel model, Resources res) {
        @MediaState int mediaState = getTabGridMediaIndicator(tab, model);
        switch (mediaState) {
            case MediaState.AUDIBLE:
                return res.getString(R.string.accessibility_tab_group_audible);
            case MediaState.MUTED:
                return res.getString(R.string.accessibility_tab_group_muted);
            case MediaState.RECORDING:
                return res.getString(R.string.accessibility_tab_group_recording);
            case MediaState.SHARING:
                return res.getString(R.string.accessibility_tab_group_sharing);
            case MediaState.PICTURE_IN_PICTURE:
                return res.getString(R.string.accessibility_tab_group_picture_in_picture);
            default:
                return "";
        }
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

    @TabListMode
    int getTabListModeForTesting() {
        return mMode;
    }

    @Nullable Tab getTabToAddDelayedForTesting() {
        return mTabToAddDelayed;
    }

    void setComponentIdForTesting(@TabComponentId int componentId) {
        var oldValueId = mComponentId;
        mComponentId = componentId;
        ResettersForTesting.register(() -> mComponentId = oldValueId);
    }

    @Nullable StripTabUnderlineManager getOrInitGlicIndicatorManagerForTesting(Tab tab) {
        return getOrInitGlicIndicatorManager(tab);
    }

    StripTabUnderlineManager.Observer getGlicObserverForTesting() {
        return mGlicObserver;
    }
}

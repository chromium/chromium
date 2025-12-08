// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.build.NullUtil.assumeNonNull;
import static org.chromium.chrome.browser.tasks.tab_management.MessageCardViewProperties.MESSAGE_TYPE;
import static org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.CARD_ALPHA;
import static org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.CARD_TYPE;
import static org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.ModelType.TAB;
import static org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.ModelType.TAB_GROUP;
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
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.build.annotations.Initializer;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.collaboration.CollaborationServiceFactory;
import org.chromium.chrome.browser.data_sharing.DataSharingServiceFactory;
import org.chromium.chrome.browser.data_sharing.DataSharingTabManager;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.price_tracking.PriceTrackingFeatures;
import org.chromium.chrome.browser.price_tracking.PriceTrackingUtilities;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.quick_delete.QuickDeleteAnimationGradientDrawable;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.Tab.MediaState;
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
import org.chromium.chrome.browser.tab_ui.ThumbnailProvider;
import org.chromium.chrome.browser.tab_ui.ThumbnailProvider.MultiThumbnailMetadata;
import org.chromium.chrome.browser.tabmodel.TabClosingSource;
import org.chromium.chrome.browser.tabmodel.TabClosureParams;
import org.chromium.chrome.browser.tabmodel.TabClosureParamsUtils;
import org.chromium.chrome.browser.tabmodel.TabGroupColorUtils;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilterObserver;
import org.chromium.chrome.browser.tabmodel.TabGroupTitleUtils;
import org.chromium.chrome.browser.tabmodel.TabGroupUtils;
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
import org.chromium.chrome.browser.tasks.tab_management.TabListCoordinator.TabListMode;
import org.chromium.chrome.browser.tasks.tab_management.TabListModel.AnimationStatus;
import org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties;
import org.chromium.chrome.browser.tasks.tab_management.TabProperties.TabActionState;
import org.chromium.chrome.browser.tasks.tab_management.TabProperties.UiType;
import org.chromium.chrome.browser.tasks.tab_management.TabSwitcherMessageManager.MessageType;
import org.chromium.chrome.browser.tasks.tab_management.TabUiMetricsHelper.TabListEditorActionMetricGroups;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.undo_tab_close_snackbar.UndoBarExplicitTrigger;
import org.chromium.chrome.tab_ui.R;
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
class TabListMediator implements TabListNotificationHandler {
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
    public interface SelectionDelegateProvider {
        SelectionDelegate getSelectionDelegate();
    }

    /** An interface to get the onClickListener when clicking on a grid card. */
    interface GridCardOnClickListenerProvider {
        /**
         * @return {@link TabActionListener} to open Tab Grid dialog. If the given {@link Tab} is
         *     not able to create group, return null;
         */
        @Nullable TabActionListener openTabGridDialog(Tab tab);

        /**
         * @return {@link TabActionListener} to open Tab Grid dialog. If the given syncId is not
         *     able to create group, return null;
         */
        @Nullable TabActionListener openTabGridDialog(String syncId);

        /**
         * Run additional actions on tab selection.
         *
         * @param tabId The ID of selected {@link Tab}.
         * @param fromActionButton Whether it is called from the Action button on the card.
         */
        void onTabSelecting(int tabId, boolean fromActionButton);
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
        TabClosedFrom.GRID_TAB_SWITCHER_GROUP
    })
    @Retention(RetentionPolicy.SOURCE)
    private @interface TabClosedFrom {
        int TAB_STRIP = 0;
        // int TAB_GRID_SHEET = 1;  // Obsolete
        int GRID_TAB_SWITCHER = 2;
        int GRID_TAB_SWITCHER_GROUP = 3;
    }

    private static final String TAG = "TabListMediator";
    private static final Map<Integer, Integer> sTabClosedFromMapTabClosedFromMap = new HashMap<>();

    private final Callback<@Nullable TabGroupModelFilter> mOnTabGroupModelFilterChanged =
            new ValueChangedCallback<>(this::onTabGroupModelFilterChanged);
    private final TabOverflowMenuCoordinator.OnItemClickedCallback<Token>
            mOnMenuItemClickedCallback = this::onMenuItemClicked;
    private final Activity mActivity;
    private final TabListModel mModelList;
    private final @TabListMode int mMode;
    private final @Nullable ModalDialogManager mModalDialogManager;
    private final ObservableSupplier<@Nullable TabGroupModelFilter>
            mCurrentTabGroupModelFilterSupplier;
    private final @Nullable ThumbnailProvider mThumbnailProvider;
    private final TabListFaviconProvider mTabListFaviconProvider;
    private final @Nullable SelectionDelegateProvider mSelectionDelegateProvider;
    private final @Nullable GridCardOnClickListenerProvider mGridCardOnClickListenerProvider;
    private final @Nullable TabGridDialogHandler mTabGridDialogHandler;
    private final @Nullable Supplier<@Nullable PriceWelcomeMessageController>
            mPriceWelcomeMessageControllerSupplier;
    private final @Nullable DataSharingTabManager mDataSharingTabManager;
    private final @Nullable Runnable mOnTabGroupCreation;
    private final TabModelObserver mTabModelObserver;
    private final TabActionListener mTabClosedListener;
    private final TabGridItemTouchHelperCallback mTabGridItemTouchHelperCallback;
    private final @Nullable UndoBarExplicitTrigger mUndoBarExplicitTrigger;
    private final @Nullable SnackbarManager mSnackbarManager;
    private final int mAllowedSelectionCount;

    private int mCurrentSelectionCount;
    private int mNextTabId = Tab.INVALID_TAB_ID;
    private int mLastSelectedTabListModelIndex = TabList.INVALID_TAB_INDEX;
    private boolean mActionsOnAllRelatedTabs;
    private String mComponentName;
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

    private final TabActionListener mTabSelectedListener =
            new TabActionListener() {
                @Override
                public void run(View view, int tabId, @Nullable MotionEventInfo triggeringMotion) {
                    if (mModelList.indexFromTabId(tabId) == TabModel.INVALID_TAB_INDEX) return;

                    mNextTabId = tabId;

                    TabModel tabModel =
                            assumeNonNull(mCurrentTabGroupModelFilterSupplier.get()).getTabModel();
                    if (!mActionsOnAllRelatedTabs) {
                        Tab currentTab = TabModelUtils.getCurrentTab(tabModel);
                        assumeNonNull(currentTab);
                        Tab newlySelectedTab = tabModel.getTabById(tabId);
                        assumeNonNull(newlySelectedTab);

                        // We filtered the tab switching related metric for components that takes
                        // actions on all related tabs (e.g. GTS) because that component can
                        // switch to different TabModel before switching tabs, while this class
                        // only contains information for all tabs that are in the same TabModel,
                        // more specifically:
                        //   * For Tabs.TabOffsetOfSwitch, we do not want to log anything if the
                        // user
                        //     switched from normal to incognito or vice-versa.
                        //   * For MobileTabSwitched, as compared to the VTS, we need to account for
                        //     MobileTabReturnedToCurrentTab action. This action is defined as
                        // return to the
                        //     same tab as before entering the component, and we don't have this
                        // information
                        //     here.
                        recordUserSwitchedTab(currentTab, newlySelectedTab);
                    }
                    if (mGridCardOnClickListenerProvider != null) {
                        mGridCardOnClickListenerProvider.onTabSelecting(
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
                 * Records MobileTabSwitched for the component. Also, records Tabs.TabOffsetOfSwitch
                 * but only when fromTab and toTab are within the same group. This method only
                 * records UMA for components other than TabSwitcher.
                 *
                 * @param fromTab The previous selected tab.
                 * @param toTab The new selected tab.
                 */
                private void recordUserSwitchedTab(Tab fromTab, Tab toTab) {
                    TabGroupModelFilter filter = mCurrentTabGroupModelFilterSupplier.get();
                    assumeNonNull(filter);
                    int fromFilterIndex = filter.representativeIndexOf(fromTab);
                    int toFilterIndex = filter.representativeIndexOf(toTab);

                    RecordUserAction.record("MobileTabSwitched." + mComponentName);

                    if (fromFilterIndex != toFilterIndex) return;

                    TabModel tabModel = filter.getTabModel();
                    int fromIndex = TabModelUtils.getTabIndexById(tabModel, fromTab.getId());
                    int toIndex = TabModelUtils.getTabIndexById(tabModel, toTab.getId());

                    RecordHistogram.recordSparseHistogram(
                            "Tabs.TabOffsetOfSwitch." + mComponentName, fromIndex - toIndex);
                }
            };

    private final TabActionListener mSelectableTabOnClickListener =
            new TabActionListener() {
                @Override
                public void run(View view, int tabId, @Nullable MotionEventInfo triggeringMotion) {
                    @Nullable PropertyModel model = mModelList.getModelFromTabId(tabId);
                    if (model == null) return;

                    boolean selected = model.get(TabProperties.IS_SELECTED);
                    if (!selected
                            && mAllowedSelectionCount > 0
                            && mCurrentSelectionCount >= mAllowedSelectionCount) {
                        showLimitSnackbar();
                        return;
                    }
                    dismissLimitSnackbar();
                    SelectionDelegate<TabListEditorItemSelectionId> selectionDelegate =
                            getTabSelectionDelegate();
                    assert selectionDelegate != null;
                    selectionDelegate.toggleSelectionForItem(
                            TabListEditorItemSelectionId.createTabId(tabId));

                    if (selected) {
                        TabUiMetricsHelper.recordSelectionEditorActionMetrics(
                                TabListEditorActionMetricGroups.UNSELECTED);
                        mCurrentSelectionCount -= 1;
                    } else {
                        TabUiMetricsHelper.recordSelectionEditorActionMetrics(
                                TabListEditorActionMetricGroups.SELECTED);
                        mCurrentSelectionCount += 1;
                    }
                    model.set(TabProperties.IS_SELECTED, !selected);
                    // Reset thumbnail to ensure the color of the blank tab slots is correct.
                    TabGroupModelFilter filter = mCurrentTabGroupModelFilterSupplier.get();
                    assumeNonNull(filter);
                    Tab tab = filter.getTabModel().getTabById(tabId);
                    if (tab != null && filter.isTabInTabGroup(tab)) {
                        updateThumbnailFetcher(model, tabId);
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

                    @Nullable PropertyModel model = mModelList.getModelFromSyncId(syncId);
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
                    // See https://crbug.com/1359002.
                    if (navigationHandle.isSameDocument()
                            || UrlUtilities.isNtpUrl(tab.getUrl())
                            || tab.getUrl().equals(navigationHandle.getUrl())) {
                        return;
                    }
                    @Nullable PropertyModel model = mModelList.getModelFromTabId(tab.getId());
                    if (model == null
                            || (mActionsOnAllRelatedTabs
                                    && assumeNonNull(mCurrentTabGroupModelFilterSupplier.get())
                                            .isTabInTabGroup(tab))) {
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
                            || assumeNonNull(mCurrentTabGroupModelFilterSupplier.get())
                                            .getTabModel()
                                            .getTabById(updatedTab.getId())
                                    == null) {
                        return;
                    }
                    model.set(
                            TabProperties.TITLE,
                            getLatestTitleForTab(updatedTab, /* useDefault= */ true));
                }

                @Override
                public void onFaviconUpdated(
                        Tab updatedTab, @Nullable Bitmap icon, @Nullable GURL iconUrl) {
                    assert mShowingTabs;

                    @Nullable PropertyModel tabInfo = null;
                    @Nullable Tab tab = null;
                    if (mActionsOnAllRelatedTabs && isTabInTabGroup(updatedTab)) {
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
                    } else if (mActionsOnAllRelatedTabs) {
                        @Nullable Pair<Integer, Tab> indexAndTab =
                                getIndexAndTabForTabGroupId(updatedTab.getTabGroupId());
                        if (indexAndTab != null) {
                            tab = indexAndTab.second;
                            model = mModelList.get(indexAndTab.first).model;
                        }
                    }
                    if (TabUtils.isValid(tab) && model != null) {
                        model.set(TabProperties.URL_DOMAIN, getDomainForTab(tab));
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
                            mActionsOnAllRelatedTabs && isTabInTabGroup(updatedTab);
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
                            getTabGridMediaIndicator(representativeTab));
                    if (isTabGroupTabGrid) {
                        updateDescriptionString(representativeTab, model);
                    }
                }

                @Override
                public void onTabPinnedStateChanged(Tab tab, boolean isPinned) {
                    int index = mModelList.indexFromTabId(tab.getId());
                    updateTab(index, tab, /* isUpdatingId= */ false, /* quickMode= */ false);

                    // When pinning a tab in a group it will be removed from the group so the index
                    // update is unnecessary.
                    if (!mActionsOnAllRelatedTabs) return;

                    int finalIndex =
                            mModelList.indexOfNthTabCard(
                                    mCurrentTabGroupModelFilterSupplier
                                            .get()
                                            .getTabModel()
                                            .indexOf(tab));
                    // indexOfNthTabCard returns n + 1 if the index is higher than the number of
                    // tabs in the model list. Moving is implemented as removal then addition.
                    // The last valid index to add to is the size of the model list after the
                    // removal so we need to clamp to mModelList.size() - 1.
                    finalIndex = Math.min(finalIndex, mModelList.size() - 1);
                    if (index != finalIndex
                            && index != TabModel.INVALID_TAB_INDEX
                            && finalIndex != TabModel.INVALID_TAB_INDEX) {
                        mModelList.move(index, finalIndex);
                    }
                }
            };

    private final TabGroupModelFilterObserver mTabGroupObserver =
            new TabGroupModelFilterObserver() {
                @Override
                public void didChangeTabGroupTitle(Token tabGroupId, @Nullable String newTitle) {
                    assert mShowingTabs;

                    if (!mActionsOnAllRelatedTabs) return;

                    @Nullable Pair<Integer, Tab> indexAndTab =
                            getIndexAndTabForTabGroupId(tabGroupId);
                    if (indexAndTab == null) return;
                    Tab tab = indexAndTab.second;
                    PropertyModel model = mModelList.get(indexAndTab.first).model;

                    // Do not trust the `newTitle`, it may be necessary to apply a default/fallback.
                    newTitle = getLatestTitleForTab(tab, /* useDefault= */ true);

                    model.set(TabProperties.TITLE, newTitle);
                    updateDescriptionString(tab, model);
                    updateActionButtonDescriptionString(tab, model);
                }

                @Override
                public void didChangeTabGroupColor(
                        Token tabGroupId, @TabGroupColorId int newColor) {
                    assert mShowingTabs;

                    if (!mActionsOnAllRelatedTabs) return;

                    @Nullable Pair<Integer, Tab> indexAndTab =
                            getIndexAndTabForTabGroupId(tabGroupId);
                    if (indexAndTab == null) return;
                    Tab tab = indexAndTab.second;
                    PropertyModel model = mModelList.get(indexAndTab.first).model;

                    updateFaviconForTab(model, tab, null, null);
                    updateTabGroupColorViewProvider(model, tab, newColor);
                    updateDescriptionString(tab, model);
                    updateActionButtonDescriptionString(tab, model);
                    updateThumbnailFetcher(model, tab.getId());
                }

                @Override
                public void didMoveWithinGroup(
                        Tab movedTab, int tabModelOldIndex, int tabModelNewIndex) {
                    assert mShowingTabs;

                    if (tabModelNewIndex == tabModelOldIndex) return;

                    TabGroupModelFilter filter = mCurrentTabGroupModelFilterSupplier.get();
                    assumeNonNull(filter);
                    TabModel tabModel = filter.getTabModel();

                    // For the tab switcher update the tab card correctly.
                    int movedTabId = movedTab.getId();
                    if (mActionsOnAllRelatedTabs && mThumbnailProvider != null) {
                        int indexInModel = getIndexForTabIdWithRelatedTabs(movedTabId);
                        if (indexInModel == TabModel.INVALID_TAB_INDEX) return;

                        Tab lastShownTab =
                                filter.getRepresentativeTabAt(
                                        filter.representativeIndexOf(movedTab));
                        assumeNonNull(lastShownTab);
                        PropertyModel model = mModelList.get(indexInModel).model;
                        updateThumbnailFetcher(model, lastShownTab.getId());
                        return;
                    }

                    // For the grid dialog or tab strip maintain order.
                    int curPosition = mModelList.indexFromTabId(movedTabId);

                    if (!isValidMovePosition(curPosition)) return;

                    Tab destinationTab =
                            tabModel.getTabAt(
                                    tabModelNewIndex > tabModelOldIndex
                                            ? tabModelNewIndex - 1
                                            : tabModelNewIndex + 1);
                    assumeNonNull(destinationTab);
                    int newPosition = mModelList.indexFromTabId(destinationTab.getId());

                    if (!isValidMovePosition(newPosition)) return;
                    mModelList.move(curPosition, newPosition);
                }

                @Override
                public void didMoveTabOutOfGroup(Tab movedTab, int prevFilterIndex) {
                    assert mShowingTabs;

                    assert !(mActionsOnAllRelatedTabs && mTabGridDialogHandler != null);

                    TabGroupModelFilter filter = mCurrentTabGroupModelFilterSupplier.get();
                    assumeNonNull(filter);
                    Tab previousGroupTab = filter.getRepresentativeTabAt(prevFilterIndex);
                    assumeNonNull(previousGroupTab);
                    if (mActionsOnAllRelatedTabs) {
                        Token movedTabGroupId = movedTab.getTabGroupId();
                        if (filter.getTabCountForGroup(movedTabGroupId) <= 1
                                && movedTab != previousGroupTab) {
                            // Add a tab to the model if it represents a new card. This happens if
                            // the tab is either not in a group or in a group by itself. We do this
                            // first so that the indices for the filter and the model match when
                            // doing the update afterwards. When moving a tab between groups, the
                            // new tab being added to an existing group is handled in
                            // didMergeTabToGroup().
                            int currentSelectedTabId =
                                    TabModelUtils.getCurrentTabId(filter.getTabModel());
                            int filterIndex = filter.representativeIndexOf(movedTab);
                            addTabInfoToModel(
                                    movedTab,
                                    mModelList.indexOfNthTabCard(filterIndex),
                                    currentSelectedTabId == movedTab.getId());
                        } else if (ChromeFeatureList.sTabCollectionAndroid.isEnabled()
                                && movedTabGroupId != null
                                && movedTabGroupId.equals(previousGroupTab.getTabGroupId())) {
                            // Despite being ungrouped we are still in a tab group this could mean
                            // the previous tab card this tab was associated with no longer contains
                            // tabs. If we have the same tab group id as the previous group tab then
                            // this was possibly the last tab in its group. Remove the tab card if
                            // it exists.
                            int previousIndex = mModelList.indexFromTabId(movedTab.getId());
                            if (previousIndex != TabModel.INVALID_TAB_INDEX) {
                                mModelList.removeAt(previousIndex);
                                return;
                            }
                        }
                        // Always update the previous group to clean up old state e.g. thumbnail,
                        // title, etc.
                        updateTab(
                                mModelList.indexOfNthTabCard(prevFilterIndex),
                                previousGroupTab,
                                true,
                                false);
                    } else {
                        int previousGroupTabId = previousGroupTab.getId();
                        int movedTabId = movedTab.getId();
                        int previousTabListModelIndex =
                                mModelList.indexFromTabId(previousGroupTabId);
                        // Invalid means the previous group tab isn't visible. Either:
                        // 1. The moved tab isn't in this model list.
                        // 2. The moved tab is meant to stay in the model list as this is the
                        //    destination group.
                        // In either case no-op.
                        if (previousTabListModelIndex == TabList.INVALID_TAB_INDEX) {
                            return;
                        }

                        // The moved tab isn't here, or it is out-of-bounds no-op.
                        int curTabListModelIndex = mModelList.indexFromTabId(movedTabId);
                        if (!isValidMovePosition(curTabListModelIndex)) return;

                        mModelList.removeAt(curTabListModelIndex);
                        if (mTabGridDialogHandler != null) {
                            boolean isUngroupingLastTabInGroup = previousGroupTabId == movedTabId;
                            mTabGridDialogHandler.updateDialogContent(
                                    isUngroupingLastTabInGroup
                                            ? Tab.INVALID_TAB_ID
                                            : previousGroupTabId);
                        }
                    }
                }

                @Override
                public void didMergeTabToGroup(Tab movedTab, boolean isDestinationTab) {
                    assert mShowingTabs;

                    TabGroupModelFilter filter = mCurrentTabGroupModelFilterSupplier.get();
                    assumeNonNull(filter);
                    TabModel tabModel = filter.getTabModel();
                    if (mActionsOnAllRelatedTabs) {
                        List<Tab> relatedTabs = getRelatedTabsForId(movedTab.getId());
                        Pair<Integer, Integer> positions =
                                mModelList.getIndexesForMergeToGroup(
                                        tabModel, movedTab, isDestinationTab, relatedTabs);
                        int srcIndex = positions.second;
                        int desIndex = positions.first;

                        // If only the desIndex is valid then just update the destination index to
                        // the last shown tab in its group.
                        if (desIndex != TabModel.INVALID_TAB_INDEX
                                && srcIndex == TabModel.INVALID_TAB_INDEX) {
                            @TabId
                            int desIndexTabId =
                                    mModelList.get(desIndex).model.get(TabProperties.TAB_ID);
                            Tab desTab = tabModel.getTabById(desIndexTabId);
                            assumeNonNull(desTab);
                            if (!ChromeFeatureList.sTabCollectionAndroid.isEnabled()) {
                                updateTab(desIndex, desTab, false, false);
                                return;
                            }
                            Token desTabGroupId = desTab.getTabGroupId();
                            Tab lastShownTab = desTab;
                            if (desTabGroupId != null) {
                                @TabId
                                int lastShownTabId = filter.getGroupLastShownTabId(desTabGroupId);
                                if (lastShownTabId != Tab.INVALID_TAB_ID) {
                                    lastShownTab = tabModel.getTabById(lastShownTabId);
                                }
                            }
                            assert lastShownTab != null;
                            updateTab(desIndex, lastShownTab, true, false);
                            return;
                        }

                        if (!isValidMovePosition(srcIndex) || !isValidMovePosition(desIndex)) {
                            return;
                        }

                        // We merged the source group to the destination group. Remove the source
                        // group and update the destination group.
                        mModelList.removeAt(srcIndex);
                        desIndex =
                                srcIndex > desIndex
                                        ? desIndex
                                        : mModelList.getTabIndexBefore(desIndex);
                        Tab newSelectedTabInMergedGroup =
                                filter.getRepresentativeTabAt(
                                        mModelList.getTabCardCountsBefore(desIndex));
                        assumeNonNull(newSelectedTabInMergedGroup);
                        updateTab(desIndex, newSelectedTabInMergedGroup, true, false);

                        // TODO(crbug.com/434246302): These metrics are probably wrong as it looks
                        // like they get emitted per-tab merged, rather than per-group merged.
                        if (getRelatedTabsForId(movedTab.getId()).size() == 2) {
                            // When users use drop-to-merge to create a group.
                            RecordUserAction.record("TabGroup.Created.DropToMerge");
                        } else {
                            RecordUserAction.record("TabGrid.Drag.DropToMerge");
                        }
                    } else {
                        // If no tab is present we can't check if the added tab is part of the
                        // current group. Assume it isn't since a group state with 0 tab should be
                        // impossible.
                        @Nullable PropertyModel model = mModelList.getFirstTabPropertyModel();
                        if (model == null) return;

                        // If the added tab is part of the group add it and update the dialog.
                        int firstTabId = model.get(TabProperties.TAB_ID);
                        Tab firstTab = tabModel.getTabById(firstTabId);
                        if (firstTab == null
                                || !Objects.equals(
                                        firstTab.getTabGroupId(), movedTab.getTabGroupId())) {
                            return;
                        }

                        movedTab.addObserver(mTabObserver);
                        onTabAdded(movedTab, /* onlyShowRelatedTabs= */ true);
                        if (mTabGridDialogHandler != null) {
                            mTabGridDialogHandler.updateDialogContent(
                                    filter.getGroupLastShownTabId(firstTab.getTabGroupId()));
                        }
                    }
                }

                @Override
                public void didMoveTabGroup(
                        Tab movedTab, int tabModelOldIndex, int tabModelNewIndex) {
                    assert mShowingTabs;

                    if (!mActionsOnAllRelatedTabs || tabModelNewIndex == tabModelOldIndex) {
                        return;
                    }
                    List<Tab> relatedTabs = getRelatedTabsForId(movedTab.getId());
                    TabGroupModelFilter filter = mCurrentTabGroupModelFilterSupplier.get();
                    assumeNonNull(filter);
                    Tab currentGroupSelectedTab =
                            TabGroupUtils.getSelectedTabInGroupForTab(filter, movedTab);
                    TabModel tabModel = filter.getTabModel();
                    int curPosition = mModelList.indexFromTabId(currentGroupSelectedTab.getId());
                    if (curPosition == TabModel.INVALID_TAB_INDEX) {
                        // Sync TabListModel with updated TabGroupModelFilter.
                        int indexToUpdate =
                                mModelList.indexOfNthTabCard(
                                        filter.representativeIndexOf(
                                                tabModel.getTabAt(tabModelOldIndex)));
                        mModelList.updateTabListModelIdForGroup(
                                currentGroupSelectedTab, indexToUpdate);
                        curPosition = mModelList.indexFromTabId(currentGroupSelectedTab.getId());
                    }
                    if (!isValidMovePosition(curPosition)) return;

                    // Find the tab which was in the destination index before this move. Use
                    // that tab to figure out the new position.
                    int destinationTabIndex =
                            tabModelNewIndex > tabModelOldIndex
                                    ? tabModelNewIndex - relatedTabs.size()
                                    : tabModelNewIndex + 1;
                    Tab destinationTab = tabModel.getTabAt(destinationTabIndex);
                    assumeNonNull(destinationTab);
                    Tab destinationGroupSelectedTab =
                            TabGroupUtils.getSelectedTabInGroupForTab(filter, destinationTab);
                    int newPosition =
                            mModelList.indexFromTabId(destinationGroupSelectedTab.getId());
                    if (newPosition == TabModel.INVALID_TAB_INDEX) {
                        int indexToUpdate =
                                mModelList.indexOfNthTabCard(
                                        filter.representativeIndexOf(destinationTab)
                                                + (tabModelNewIndex > tabModelOldIndex ? 1 : -1));
                        mModelList.updateTabListModelIdForGroup(
                                destinationGroupSelectedTab, indexToUpdate);
                        newPosition =
                                mModelList.indexFromTabId(destinationGroupSelectedTab.getId());
                    }
                    if (!isValidMovePosition(newPosition)) return;

                    mModelList.move(curPosition, newPosition);
                }

                @Override
                public void didCreateNewGroup(Tab destinationTab, TabGroupModelFilter filter) {
                    // On new group creation for the tab group representation in the GTS, update
                    // the tab group color icon.
                    int groupIndex = filter.representativeIndexOf(destinationTab);
                    Tab groupTab = filter.getRepresentativeTabAt(groupIndex);
                    assumeNonNull(groupTab);
                    PropertyModel model = mModelList.getModelFromTabId(groupTab.getId());

                    if (model != null) {
                        Token tabGroupId = destinationTab.getTabGroupId();
                        assumeNonNull(tabGroupId);
                        @TabGroupColorId
                        int colorId = filter.getTabGroupColorWithFallback(tabGroupId);
                        updateFaviconForTab(model, groupTab, null, null);
                        updateTabGroupColorViewProvider(model, destinationTab, colorId);
                    }
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
     * @param tabGroupModelFilterSupplier Used to fetch the filter that provides tab group
     *     information.
     * @param thumbnailProvider {@link ThumbnailProvider} to provide screenshot related details.
     * @param tabListFaviconProvider Provider for all favicon related drawables.
     * @param actionOnRelatedTabs Whether tab-related actions should be operated on all related
     *     tabs.
     * @param selectionDelegateProvider Provider for a {@link SelectionDelegate} that is used for a
     *     selectable list. It's null when selection is not possible.
     * @param gridCardOnClickListenerProvider Provides the onClickListener for opening dialog when
     *     click on a grid card.
     * @param dialogHandler A handler to handle requests about updating TabGridDialog.
     * @param priceWelcomeMessageControllerSupplier A supplier of a controller to show
     *     PriceWelcomeMessage.
     * @param componentName This is a unique string to identify different components.
     * @param initialTabActionState The initial {@link TabActionState} to use for the shown tabs.
     *     Must always be CLOSABLE for TabListMode.STRIP.
     * @param dataSharingTabManager The service used to initiate data sharing.
     * @param onTabGroupCreation Should be run when the UI is used to create a tab group.
     * @param undoBarExplicitTrigger Interface to explicitly trigger the undo closure snackbar.
     * @param snackbarManager The manager to show snackbars.
     * @param allowedSelectionCount The maximum number of tabs that can be selected at once.
     */
    TabListMediator(
            Activity activity,
            TabListModel modelList,
            @TabListMode int mode,
            @Nullable ModalDialogManager modalDialogManager,
            ObservableSupplier<@Nullable TabGroupModelFilter> tabGroupModelFilterSupplier,
            @Nullable ThumbnailProvider thumbnailProvider,
            TabListFaviconProvider tabListFaviconProvider,
            boolean actionOnRelatedTabs,
            @Nullable SelectionDelegateProvider selectionDelegateProvider,
            @Nullable GridCardOnClickListenerProvider gridCardOnClickListenerProvider,
            @Nullable TabGridDialogHandler dialogHandler,
            @Nullable Supplier<@Nullable PriceWelcomeMessageController>
                    priceWelcomeMessageControllerSupplier,
            String componentName,
            @TabActionState int initialTabActionState,
            @Nullable DataSharingTabManager dataSharingTabManager,
            @Nullable Runnable onTabGroupCreation,
            @Nullable UndoBarExplicitTrigger undoBarExplicitTrigger,
            @Nullable SnackbarManager snackbarManager,
            int allowedSelectionCount) {
        mActivity = activity;
        mModelList = modelList;
        mMode = mode;
        mModalDialogManager = modalDialogManager;
        mCurrentTabGroupModelFilterSupplier = tabGroupModelFilterSupplier;
        mThumbnailProvider = thumbnailProvider;
        mTabListFaviconProvider = tabListFaviconProvider;
        mActionsOnAllRelatedTabs = actionOnRelatedTabs;
        mSelectionDelegateProvider = selectionDelegateProvider;
        mGridCardOnClickListenerProvider = gridCardOnClickListenerProvider;
        mTabGridDialogHandler = dialogHandler;
        mPriceWelcomeMessageControllerSupplier = priceWelcomeMessageControllerSupplier;
        mComponentName = componentName;
        mTabActionState = initialTabActionState;
        mDataSharingTabManager = dataSharingTabManager;
        mOnTabGroupCreation = onTabGroupCreation;
        mUndoBarExplicitTrigger = undoBarExplicitTrigger;
        mSnackbarManager = snackbarManager;
        mAllowedSelectionCount = allowedSelectionCount;

        mTabModelObserver =
                new TabModelObserver() {
                    @Override
                    public void didSelectTab(Tab tab, @TabSelectionType int type, int lastId) {
                        assert mShowingTabs;

                        mNextTabId = Tab.INVALID_TAB_ID;
                        int tabId = tab.getId();
                        if (tabId == lastId) return;

                        int oldIndex = mModelList.indexFromTabId(lastId);
                        if (oldIndex == TabModel.INVALID_TAB_INDEX && mActionsOnAllRelatedTabs) {
                            oldIndex = getIndexForTabIdWithRelatedTabs(lastId);
                        }
                        int newIndex = mModelList.indexFromTabId(tabId);
                        if (newIndex == TabModel.INVALID_TAB_INDEX && mActionsOnAllRelatedTabs) {
                            // If a tab in tab group does not exist in model and needs to be
                            // selected, identify the related tab ids and determine newIndex
                            // based on if any of the related ids are present in model.
                            newIndex = getIndexForTabIdWithRelatedTabs(tabId);
                            // For UNDO ensure we update the representative tab in the model.
                            if (type == TabSelectionType.FROM_UNDO
                                    && newIndex != Tab.INVALID_TAB_ID) {
                                modelList.updateTabListModelIdForGroup(tab, newIndex);
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
                        sTabClosedFromMapTabClosedFromMap.remove(tab.getId());
                    }

                    @Override
                    public void tabClosureUndone(Tab tab) {
                        assert mShowingTabs;

                        tab.addObserver(mTabObserver);
                        onTabAdded(tab, !mActionsOnAllRelatedTabs);

                        if (sTabClosedFromMapTabClosedFromMap.containsKey(tab.getId())) {
                            @TabClosedFrom
                            int from = sTabClosedFromMapTabClosedFromMap.get(tab.getId());
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
                                default:
                                    assert false
                                            : "tabClosureUndone for tab that closed from an unknown"
                                                    + " UI";
                            }
                            sTabClosedFromMapTabClosedFromMap.remove(tab.getId());
                        }
                        // TODO(yuezhanggg): clean up updateTab() calls in this class.
                        if (mActionsOnAllRelatedTabs) {
                            TabGroupModelFilter filter = mCurrentTabGroupModelFilterSupplier.get();
                            assumeNonNull(filter);
                            int filterIndex = filter.representativeIndexOf(tab);
                            if (filterIndex == TabList.INVALID_TAB_INDEX
                                    || !filter.isTabInTabGroup(tab)
                                    || filterIndex >= mModelList.size()) {
                                return;
                            }
                            Tab currentGroupSelectedTab =
                                    filter.getRepresentativeTabAt(filterIndex);
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

                        TabGroupModelFilter filter = mCurrentTabGroupModelFilterSupplier.get();
                        if (filter == null || !filter.isTabModelRestored()) {
                            return;
                        }

                        tab.addObserver(mTabObserver);

                        // Check if we need to delay tab addition to model.
                        boolean delayAdd =
                                (type == TabLaunchType.FROM_TAB_SWITCHER_UI
                                                || type == TabLaunchType.FROM_TAB_GROUP_UI)
                                        && markedForSelection
                                        && (mComponentName.equals(
                                                        TabSwitcherPaneCoordinator.COMPONENT_NAME)
                                                || mComponentName.startsWith(
                                                        TabGridDialogCoordinator
                                                                .COMPONENT_NAME_PREFIX));
                        if (delayAdd) {
                            mTabToAddDelayed = tab;
                            return;
                        }

                        onTabAdded(tab, !mActionsOnAllRelatedTabs);
                        if (type == TabLaunchType.FROM_RESTORE && mActionsOnAllRelatedTabs) {
                            // When tab is restored after restoring stage (e.g. exiting multi-window
                            // mode, switching between dark/light mode in incognito), we need to
                            // update related property models.
                            int filterIndex = filter.representativeIndexOf(tab);
                            if (filterIndex == TabList.INVALID_TAB_INDEX) return;
                            Tab currentGroupSelectedTab =
                                    filter.getRepresentativeTabAt(filterIndex);
                            assumeNonNull(currentGroupSelectedTab);
                            // TabModel and TabListModel may be in the process of syncing up through
                            // restoring. Examples of this situation are switching between
                            // light/dark mode in incognito, exiting multi-window mode, etc.
                            int tabListModelIndex = mModelList.indexOfNthTabCard(filterIndex);
                            if (mModelList.indexFromTabId(currentGroupSelectedTab.getId())
                                    != tabListModelIndex) {
                                return;
                            }
                            updateTab(tabListModelIndex, currentGroupSelectedTab, false, false);
                        }
                    }

                    @Override
                    public void willCloseTab(Tab tab, boolean didCloseAlone) {
                        if (ChromeFeatureList.sTabCollectionAndroid.isEnabled()) return;

                        onTabClose(tab);
                    }

                    @Override
                    public void didRemoveTabForClosure(Tab tab) {
                        if (!ChromeFeatureList.sTabCollectionAndroid.isEnabled()) return;

                        onTabClose(tab);
                    }

                    private void onTabClose(Tab tab) {
                        assert mShowingTabs;

                        tab.removeObserver(mTabObserver);

                        // If the tab closed was part of a tab group and the closure was triggered
                        // from the tab switcher, update the group to reflect the closure instead of
                        // closing the tab.
                        TabGroupModelFilter filter = mCurrentTabGroupModelFilterSupplier.get();
                        if (mActionsOnAllRelatedTabs
                                && filter != null
                                && filter.tabGroupExists(tab.getTabGroupId())) {
                            int groupIndex = filter.representativeIndexOf(tab);
                            Tab groupTab = filter.getRepresentativeTabAt(groupIndex);
                            assumeNonNull(groupTab);
                            if (!groupTab.isClosing()) {
                                updateTab(
                                        mModelList.indexOfNthTabCard(groupIndex),
                                        groupTab,
                                        true,
                                        false);

                                return;
                            }
                        }

                        int index = mModelList.indexFromTabId(tab.getId());
                        if (index == TabModel.INVALID_TAB_INDEX) return;

                        mModelList.removeAt(index);
                    }

                    @Override
                    public void tabRemoved(Tab tab) {
                        assert mShowingTabs;

                        tab.removeObserver(mTabObserver);

                        int index = mModelList.indexFromTabId(tab.getId());
                        if (index == TabModel.INVALID_TAB_INDEX) return;

                        mModelList.removeAt(index);
                    }
                };

        mTabClosedListener =
                new TabActionListener() {
                    @Override
                    public void run(
                            View view, int tabId, @Nullable MotionEventInfo triggeringMotion) {
                        // TODO(crbug.com/40638921): Consider disabling all touch events during
                        // animation.
                        if (mModelList.indexFromTabId(tabId) == TabModel.INVALID_TAB_INDEX) return;

                        TabGroupModelFilter filter = mCurrentTabGroupModelFilterSupplier.get();
                        assumeNonNull(filter);
                        TabModel tabModel = filter.getTabModel();
                        Tab closingTab = tabModel.getTabById(tabId);
                        if (closingTab == null) return;

                        setUseShrinkCloseAnimation(tabId, /* useShrinkCloseAnimation= */ true);
                        if (mActionsOnAllRelatedTabs && filter.isTabInTabGroup(closingTab)) {
                            onGroupClosedFrom(tabId);

                            // TODO(crbug.com/375468032): use "triggeringMotion" to determine
                            //  if the "undo" snackbar should be shown when closing a tab group.
                            TabUiUtils.closeTabGroup(
                                    filter,
                                    tabId,
                                    /* tabClosingSource */ TabClosingSource.UNKNOWN,
                                    /* allowUndo= */ true,
                                    /* hideTabGroups= */ true,
                                    getOnMaybeTabClosedCallback(tabId));
                            return;
                        }

                        onTabClosedFrom(tabId, mComponentName);
                        Tab currentTab = TabModelUtils.getCurrentTab(tabModel);
                        Tab nextTab = currentTab == closingTab ? getNextTab(tabId) : null;
                        boolean allowUndo = TabClosureParamsUtils.shouldAllowUndo(triggeringMotion);
                        TabClosureParams closureParams =
                                TabClosureParams.closeTab(closingTab)
                                        .recommendedNextTab(nextTab)
                                        .allowUndo(allowUndo)
                                        .build();

                        @Nullable TabModelActionListener listener =
                                TabUiUtils.buildMaybeDidCloseTabListener(
                                        getOnMaybeTabClosedCallback(tabId));
                        tabModel.getTabRemover()
                                .closeTabs(closureParams, /* allowDialog= */ true, listener);

                        if (mComponentName.equals(ArchivedTabsDialogCoordinator.COMPONENT_NAME)
                                && mUndoBarExplicitTrigger != null) {
                            mUndoBarExplicitTrigger.triggerSnackbarForTab(closingTab);
                        }
                    }

                    @Override
                    public void run(
                            View view, String syncId, @Nullable MotionEventInfo triggeringMotion) {
                        int index = mModelList.indexFromSyncId(syncId);
                        if (index == TabModel.INVALID_TAB_INDEX) return;

                        @Nullable PropertyModel model = mModelList.getModelFromSyncId(syncId);
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

                    private @Nullable Tab getNextTab(int closingTabId) {
                        int closingTabIndex = mModelList.indexFromTabId(closingTabId);

                        if (closingTabIndex == TabModel.INVALID_TAB_INDEX) {
                            assert false;
                            return null;
                        }

                        int nextTabId = Tab.INVALID_TAB_ID;
                        if (mModelList.size() > 1) {
                            int nextTabIndex =
                                    closingTabIndex == 0
                                            ? mModelList.getTabIndexAfter(closingTabIndex)
                                            : mModelList.getTabIndexBefore(closingTabIndex);
                            nextTabId =
                                    nextTabIndex == TabModel.INVALID_TAB_INDEX
                                            ? Tab.INVALID_TAB_ID
                                            : mModelList
                                                    .get(nextTabIndex)
                                                    .model
                                                    .get(TabProperties.TAB_ID);
                        }

                        TabGroupModelFilter filter = mCurrentTabGroupModelFilterSupplier.get();
                        assumeNonNull(filter);
                        return filter.getTabModel().getTabById(nextTabId);
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
                        // tab. This is a framework issue. For more details see crbug.com/1319859.
                        TabGroupModelFilter filter = mCurrentTabGroupModelFilterSupplier.get();

                        boolean shouldDisableItemAnimations =
                                filter != null && filter.getTabModel().getCount() <= 1;
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
                        () -> assumeNonNull(mCurrentTabGroupModelFilterSupplier.get()),
                        swipeSafeTabActionListener,
                        mTabGridDialogHandler,
                        mComponentName,
                        mActionsOnAllRelatedTabs,
                        mMode);
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
            if (mActionsOnAllRelatedTabs && mThumbnailProvider != null && mShowingTabs) {
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

        mCurrentTabGroupModelFilterSupplier.addSyncObserverAndCallIfNonNull(
                mOnTabGroupModelFilterChanged);

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

    private void onTabClosedFrom(int tabId, String fromComponent) {
        @TabClosedFrom int from;
        if (fromComponent.equals(TabGroupUiCoordinator.COMPONENT_NAME)) {
            from = TabClosedFrom.TAB_STRIP;
        } else if (fromComponent.equals(TabSwitcherPaneCoordinator.COMPONENT_NAME)) {
            from = TabClosedFrom.GRID_TAB_SWITCHER;
        } else {
            Log.w(TAG, "Attempting to close tab from Unknown UI");
            return;
        }
        sTabClosedFromMapTabClosedFromMap.put(tabId, from);
    }

    private void onGroupClosedFrom(int tabId) {
        sTabClosedFromMapTabClosedFromMap.put(tabId, TabClosedFrom.GRID_TAB_SWITCHER_GROUP);
    }

    private List<Tab> getRelatedTabsForId(int id) {
        TabGroupModelFilter filter = mCurrentTabGroupModelFilterSupplier.get();
        return filter == null ? new ArrayList<>() : filter.getRelatedTabList(id);
    }

    private List<Integer> getRelatedTabIds(int id) {
        List<Tab> relatedTabs = getRelatedTabsForId(id);
        List<@TabId Integer> tabIds = new ArrayList<>(relatedTabs.size());
        for (Tab tab : relatedTabs) {
            tabIds.add(tab.getId());
        }
        return tabIds;
    }

    private int getInsertionIndexOfTab(Tab tab, boolean onlyShowRelatedTabs) {
        if (tab == null) return TabList.INVALID_TAB_INDEX;

        int tabIndex = TabList.INVALID_TAB_INDEX;
        if (onlyShowRelatedTabs) {
            // Compute the index of the tab within the tab's group.
            @Nullable PropertyModel model = mModelList.getFirstTabPropertyModel();
            if (model == null) return TabList.INVALID_TAB_INDEX;

            List<Tab> related = getRelatedTabsForId(model.get(TabProperties.TAB_ID));
            tabIndex = related.indexOf(tab);
        } else {
            // Compute the index of the tab out of all tabs in the filter (ignore tabs that are not
            // the representative tab in a group).
            TabGroupModelFilter filter = mCurrentTabGroupModelFilterSupplier.get();
            for (int i = 0; i < filter.getIndividualTabAndGroupCount(); i++) {
                @Nullable Tab representativeTab = filter.getRepresentativeTabAt(i);
                if (representativeTab != null && tab.getId() == representativeTab.getId()) {
                    tabIndex = i;
                    break;
                }
            }
        }

        // The current implementation of TAB_GROUP card types places all groups at the beginning of
        // the model list. As a result, if any tab group cards exist, adjust the index for tab
        // insertion to start after the allotted count of tab groups in the model list.
        tabIndex += mModelList.getTabGroupCardCount();

        // Get the position of the nth tab card ignoring any other CARD_TYPE entries present in the
        // model list outside of TAB and TAB_GROUP.
        return mModelList.indexOfNthTabCard(tabIndex);
    }

    private int onTabAdded(Tab tab, boolean onlyShowRelatedTabs) {
        int existingIndex = mModelList.indexFromTabId(tab.getId());
        if (existingIndex != TabModel.INVALID_TAB_INDEX) return existingIndex;

        int newIndex = getInsertionIndexOfTab(tab, onlyShowRelatedTabs);
        if (newIndex == TabList.INVALID_TAB_INDEX) return newIndex;

        Tab currentTab =
                TabModelUtils.getCurrentTab(
                        mCurrentTabGroupModelFilterSupplier.get().getTabModel());
        addTabInfoToModel(tab, newIndex, currentTab == tab);
        return newIndex;
    }

    private boolean isValidMovePosition(int position) {
        return position != TabModel.INVALID_TAB_INDEX && position < mModelList.size();
    }

    private boolean areTabsUnchanged(@Nullable List<Tab> tabs) {
        int tabsCount = 0;
        for (int i = 0; i < mModelList.size(); i++) {
            if (mModelList.get(i).model.get(CARD_TYPE) == TAB) {
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
            if (model.get(CARD_TYPE) == TAB) {
                Tab tab = tabs.get(tabsIndex++);
                int modelTabId = model.get(TabProperties.TAB_ID);

                if (modelTabId != tab.getId()) {
                    Tab previousTab =
                            mCurrentTabGroupModelFilterSupplier
                                    .get()
                                    .getTabModel()
                                    .getTabById(modelTabId);
                    // If the tab is in the same tab group, we can just update the model's TAB_ID
                    // rather than resetting the list.
                    if (mActionsOnAllRelatedTabs
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
    boolean resetWithListOfTabs(
            @Nullable List<Tab> tabs, @Nullable List<String> tabGroupSyncIds, boolean quickMode) {
        // Update the selected count.
        mCurrentSelectionCount =
                mSelectionDelegateProvider == null
                        ? 0
                        : mSelectionDelegateProvider
                                .getSelectionDelegate()
                                .getSelectedItems()
                                .size();

        mShowingTabs = tabs != null;
        // The reset supersedes any delayed tab additions, don't add the tab.
        mTabToAddDelayed = null;
        TabGroupModelFilter filter = mCurrentTabGroupModelFilterSupplier.get();
        if (mShowingTabs) {
            addObservers(filter, assumeNonNull(tabs));
        } else {
            removeObservers(filter);
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
            int currentTabId = TabModelUtils.getCurrentTabId(filter.getTabModel());
            for (int i = 0; i < tabs.size(); i++) {
                Tab tab = tabs.get(i);
                addTabInfoToModel(tab, i, isSelectedTab(tab, currentTabId));
            }
        }

        // The current design has tab groups types inserted at the start of the model list.
        assumeNonNull(mTabGroupSyncService);
        if (tabGroupSyncIds != null) {
            for (int i = 0; i < tabGroupSyncIds.size(); i++) {
                SavedTabGroup savedTabGroup = mTabGroupSyncService.getGroup(tabGroupSyncIds.get(i));
                assumeNonNull(savedTabGroup);
                addTabGroupInfoToModel(savedTabGroup, i);
            }
        }

        return false;
    }

    void postHiding() {
        removeObservers(mCurrentTabGroupModelFilterSupplier.get());
        mShowingTabs = false;
        // if tab was marked for add later, add to model and mark as selected.
        if (mTabToAddDelayed != null) {
            int index = onTabAdded(mTabToAddDelayed, !mActionsOnAllRelatedTabs);
            selectTab(mLastSelectedTabListModelIndex, index);
            mTabToAddDelayed = null;
        }
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
            if (model.get(CARD_TYPE) == TAB || model.get(CARD_TYPE) == TAB_GROUP) {
                updateThumbnailFetcher(model, Tab.INVALID_TAB_ID);
                model.set(TabProperties.FAVICON_FETCHER, null);
            }
        }
    }

    private void updateTab(int index, Tab tab, boolean isUpdatingId, boolean quickMode) {
        if (index < 0 || index >= mModelList.size()) return;

        PropertyModel model = mModelList.get(index).model;
        if (isUpdatingId) {
            model.set(TabProperties.TAB_ID, tab.getId());
        } else {
            assert model.get(TabProperties.TAB_ID) == tab.getId();
        }

        boolean isTabSelected = isTabSelected(mTabActionState, tab);
        boolean isInTabGroup = isTabInTabGroup(tab);
        @TabGroupColorId int tabGroupColorId = TabGroupColorId.GREY;
        // Only update the color if the tab is a representation of a tab group, otherwise
        // hide the icon by setting the color to INVALID.
        if (isInTabGroup) {
            Token tabGroupId = tab.getTabGroupId();
            assumeNonNull(tabGroupId);
            TabGroupModelFilter filter = mCurrentTabGroupModelFilterSupplier.get();
            tabGroupColorId = filter.getTabGroupColorWithFallback(tabGroupId);
        }

        updateTabGroupColorViewProvider(model, tab, tabGroupColorId);
        model.set(TabProperties.TAB_CLICK_LISTENER, getTabActionListener(tab, isInTabGroup));
        model.set(TabProperties.IS_SELECTED, isTabSelected);
        model.set(TabProperties.SHOULD_SHOW_PRICE_DROP_TOOLTIP, false);
        model.set(TabProperties.TITLE, getLatestTitleForTab(tab, /* useDefault= */ true));
        model.set(TabProperties.MEDIA_INDICATOR, getTabGridMediaIndicator(tab));
        model.set(TabProperties.IS_PINNED, tab.getIsPinned());

        bindTabActionStateProperties(model.get(TabProperties.TAB_ACTION_STATE), tab, model);

        model.set(TabProperties.URL_DOMAIN, getDomainForTab(tab));

        setupPersistedTabDataFetcherForTab(tab, index);

        updateFaviconForTab(model, tab, null, null);
        boolean forceUpdate = isTabSelected && !quickMode;
        boolean forceUpdateLastSelected =
                mActionsOnAllRelatedTabs && index == mLastSelectedTabListModelIndex && !quickMode;
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

    @VisibleForTesting
    public boolean isTabInTabGroup(Tab tab) {
        TabGroupModelFilter filter = mCurrentTabGroupModelFilterSupplier.get();
        assert filter.isTabModelRestored();

        return filter.isTabInTabGroup(tab);
    }

    private @MediaState int getTabGridMediaIndicator(Tab representativeTab) {
        if (!ChromeFeatureList.sMediaIndicatorsAndroid.isEnabled()) return MediaState.NONE;

        @MediaState int stateToReturn = representativeTab.getMediaState();
        // If the tab is not in a group, or the  state has the highest priority, then return
        // the state of the representative tab.
        if (!mActionsOnAllRelatedTabs
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
                        if (!isValidMovePosition(currentPosition)
                                || !isValidMovePosition(targetPosition)) {
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
        removeObservers(mCurrentTabGroupModelFilterSupplier.get());
        mCurrentTabGroupModelFilterSupplier.removeObserver(mOnTabGroupModelFilterChanged);

        if (mComponentCallbacks != null) {
            mActivity.unregisterComponentCallbacks(mComponentCallbacks);
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
                assumeNonNull(mTabGroupSyncService);
                SavedTabGroup savedTabGroup =
                        mTabGroupSyncService.getGroup(model.get(TabProperties.TAB_GROUP_SYNC_ID));
                if (savedTabGroup != null) {
                    bindTabGroupActionStateProperties(savedTabGroup, model);
                }
            } else {
                assert false : "Unexpected itemId type.";
            }
        }
    }

    private void unbindTabActionStateProperties(PropertyModel model) {
        model.set(TabProperties.IS_SELECTED, false);
        for (WritableObjectPropertyKey propertyKey : TabProperties.TAB_ACTION_STATE_OBJECT_KEYS) {
            model.set(propertyKey, null);
        }
    }

    private TabActionButtonData getTabActionButtonData(
            Tab tab, @TabActionState int tabActionState) {
        if (tabActionState == TabActionState.SELECTABLE) {
            return new TabActionButtonData(
                    TabActionButtonType.SELECT, mSelectableTabOnClickListener);
        }
        // A tab is deemed a tab group card representation if it is part of a tab group and
        // based in the tab switcher.
        boolean isTabGroup = isTabInTabGroup(tab) && mActionsOnAllRelatedTabs;
        if (isTabGroup) {
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
            TabModel tabModel = mCurrentTabGroupModelFilterSupplier.get().getTabModel();
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
                            () -> mCurrentTabGroupModelFilterSupplier.get().getTabModel(),
                            tabGroupSyncService,
                            collaborationService,
                            mActivity);
        }
        return mTabListGroupMenuCoordinator.getTabActionListener();
    }

    private @Nullable TabActionListener getTabClickListener(
            Tab tab, @TabActionState int tabActionState) {
        if (tabActionState == TabActionState.SELECTABLE) {
            return mSelectableTabOnClickListener;
        } else {
            if (isTabInTabGroup(tab)
                    && mActionsOnAllRelatedTabs
                    && mGridCardOnClickListenerProvider != null) {
                return mGridCardOnClickListenerProvider.openTabGridDialog(tab);
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

    private void bindTabActionStateProperties(
            @TabActionState int tabActionState, Tab tab, PropertyModel model) {
        model.set(TabProperties.IS_SELECTED, isTabSelected(tabActionState, tab));

        model.set(
                TabProperties.TAB_ACTION_BUTTON_DATA, getTabActionButtonData(tab, tabActionState));
        model.set(TabProperties.TAB_CLICK_LISTENER, getTabClickListener(tab, tabActionState));
        model.set(TabProperties.TAB_LONG_CLICK_LISTENER, getTabLongClickListener(tabActionState));
        model.set(
                TabProperties.TAB_CONTEXT_CLICK_LISTENER,
                getTabContextClickListener(tabActionState));
        model.set(TabProperties.TAB_CARD_LABEL_DATA, model.get(TabProperties.TAB_CARD_LABEL_DATA));

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
        assumeNonNull(mGridCardOnClickListenerProvider);
        TabActionListener tabClickListener =
                isSelectableState
                        ? mSelectableTabOnClickListener
                        : mGridCardOnClickListenerProvider.openTabGridDialog(
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
        if (mGridCardOnClickListenerProvider == null
                || !isInTabGroup
                || !mActionsOnAllRelatedTabs) {
            tabSelectedListener = mTabSelectedListener;
        } else {
            tabSelectedListener = mGridCardOnClickListenerProvider.openTabGridDialog(tab);
            if (tabSelectedListener == null) {
                tabSelectedListener = mTabSelectedListener;
            }
        }
        return tabSelectedListener;
    }

    private boolean isTabSelected(@TabActionState int tabActionState, Tab tab) {
        if (tabActionState == TabActionState.SELECTABLE) {
            SelectionDelegate selectionDelegate = getTabSelectionDelegate();
            assert selectionDelegate != null : "Null selection delegate while in SELECTABLE state.";
            return selectionDelegate.isItemSelected(
                    TabListEditorItemSelectionId.createTabId(tab.getId()));
        } else {
            TabModel tabModel = mCurrentTabGroupModelFilterSupplier.get().getTabModel();
            // If the tab is part of a group and also being displayed with single tabs, then there
            // is extra work needed to determine if it's selected. That is - go through all related
            // tabs, and if any is the selected tabs then the tab group is selected.
            if (mActionsOnAllRelatedTabs && tab.getTabGroupId() != null) {
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

    private void addTabInfoToModel(Tab tab, int index, boolean isSelected) {
        assert index != TabModel.INVALID_TAB_INDEX;
        boolean isInTabGroup = isTabInTabGroup(tab);

        PropertyModel tabInfo =
                new PropertyModel.Builder(TabProperties.ALL_KEYS_TAB_GRID)
                        .with(TabProperties.TAB_ACTION_STATE, mTabActionState)
                        .with(TabProperties.TAB_ID, tab.getId())
                        .with(TabProperties.IS_INCOGNITO, tab.isIncognito())
                        .with(
                                TabProperties.TITLE,
                                getLatestTitleForTab(tab, /* useDefault= */ true))
                        .with(TabProperties.URL_DOMAIN, getDomainForTab(tab))
                        .with(TabProperties.FAVICON_FETCHER, null)
                        .with(TabProperties.FAVICON_FETCHED, false)
                        .with(TabProperties.IS_SELECTED, isSelected)
                        .with(CARD_ALPHA, 1f)
                        .with(CardProperties.CARD_ANIMATION_STATUS, AnimationStatus.CARD_RESTORE)
                        .with(TabProperties.TAB_SELECTION_DELEGATE, getTabSelectionDelegate())
                        .with(TabProperties.ACCESSIBILITY_DELEGATE, mAccessibilityDelegate)
                        .with(TabProperties.SHOULD_SHOW_PRICE_DROP_TOOLTIP, false)
                        .with(CARD_TYPE, TAB)
                        .with(
                                TabProperties.QUICK_DELETE_ANIMATION_STATUS,
                                QuickDeleteAnimationStatus.TAB_RESTORE)
                        .with(TabProperties.VISIBILITY, View.VISIBLE)
                        .with(TabProperties.USE_SHRINK_CLOSE_ANIMATION, false)
                        .with(TabProperties.MEDIA_INDICATOR, getTabGridMediaIndicator(tab))
                        .with(TabProperties.IS_PINNED, tab.getIsPinned())
                        .build();

        if (!mActionsOnAllRelatedTabs || isInTabGroup) {
            tabInfo.set(
                    TabProperties.FAVICON_FETCHER,
                    mTabListFaviconProvider.getDefaultFaviconFetcher(tab.isIncognito()));
        }

        bindTabActionStateProperties(mTabActionState, tab, tabInfo);

        @UiType
        int tabUiType =
                mMode == TabListMode.STRIP ? TabProperties.UiType.STRIP : TabProperties.UiType.TAB;
        if (index >= mModelList.size()) {
            mModelList.add(new ListItem(tabUiType, tabInfo));
        } else {
            mModelList.add(index, new ListItem(tabUiType, tabInfo));
        }

        setupPersistedTabDataFetcherForTab(tab, index);

        updateFaviconForTab(tabInfo, tab, null, null);

        @TabGroupColorId int colorId = TabGroupColorId.GREY;
        if (isInTabGroup && mActionsOnAllRelatedTabs) {
            Token tabGroupId = tab.getTabGroupId();
            assumeNonNull(tabGroupId);
            TabGroupModelFilter filter = mCurrentTabGroupModelFilterSupplier.get();
            colorId = filter.getTabGroupColorWithFallback(tabGroupId);
        }
        updateTabGroupColorViewProvider(tabInfo, tab, colorId);

        if (mThumbnailProvider != null && mDefaultGridCardSize != null) {
            if (!mDefaultGridCardSize.equals(tabInfo.get(TabProperties.GRID_CARD_SIZE))) {
                tabInfo.set(
                        TabProperties.GRID_CARD_SIZE,
                        new Size(
                                mDefaultGridCardSize.getWidth(), mDefaultGridCardSize.getHeight()));
            }
        }
        if (mThumbnailProvider != null && mShowingTabs) {
            updateThumbnailFetcher(tabInfo, tab.getId());
        }
    }

    private void addTabGroupInfoToModel(SavedTabGroup savedTabGroup, int index) {
        assert savedTabGroup != null;
        String title =
                TextUtils.isEmpty(savedTabGroup.title)
                        ? TabGroupTitleUtils.getDefaultTitle(
                                mActivity, savedTabGroup.savedTabs.size())
                        : savedTabGroup.title;

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
                        .with(CARD_TYPE, TAB_GROUP)
                        .with(
                                TabProperties.QUICK_DELETE_ANIMATION_STATUS,
                                QuickDeleteAnimationStatus.TAB_RESTORE)
                        .with(TabProperties.VISIBILITY, View.VISIBLE)
                        .with(TabProperties.USE_SHRINK_CLOSE_ANIMATION, false)
                        .build();

        bindTabGroupActionStateProperties(savedTabGroup, tabGroupInfo);

        mModelList.add(index, new ListItem(TabProperties.UiType.TAB_GROUP, tabGroupInfo));

        String syncId = savedTabGroup.syncId;
        assumeNonNull(syncId);
        updateTabGroupColorViewProvider(
                tabGroupInfo, EitherGroupId.createSyncId(syncId), savedTabGroup.color);
        assumeNonNull(mDefaultGridCardSize);
        tabGroupInfo.set(
                TabProperties.GRID_CARD_SIZE,
                new Size(mDefaultGridCardSize.getWidth(), mDefaultGridCardSize.getHeight()));

        if (mThumbnailProvider != null) {
            updateThumbnailFetcher(tabGroupInfo, savedTabGroup);
        }
    }

    private String getDomainForTab(Tab tab) {
        if (!mActionsOnAllRelatedTabs) return getDomain(tab);
        List<Tab> relatedTabs = getRelatedTabsForId(tab.getId());

        List<String> domainNames = new ArrayList<>();

        for (int i = 0; i < relatedTabs.size(); i++) {
            String domain = getDomain(relatedTabs.get(i));
            domainNames.add(domain);
        }
        // TODO(crbug.com/40107640): Address i18n issue for the list delimiter.
        return TextUtils.join(", ", domainNames);
    }

    private void updateDescriptionString(Tab tab, PropertyModel model) {
        if (!mActionsOnAllRelatedTabs) return;
        boolean isInTabGroup = isTabInTabGroup(tab);
        int numOfRelatedTabs = getRelatedTabsForId(tab.getId()).size();
        TextResolver contentDescriptionResolver =
                (context) -> {
                    if (!isInTabGroup) {
                        if (mComponentName.equals(ArchivedTabsDialogCoordinator.COMPONENT_NAME)) {
                            return context.getString(
                                    R.string.accessibility_restore_tab, tab.getTitle());
                        }
                        return "";
                    }
                    String title = getLatestTitleForTab(tab, /* useDefault= */ false);
                    Resources res = context.getResources();
                    TabGroupModelFilter filter = mCurrentTabGroupModelFilterSupplier.get();
                    @TabGroupColorId
                    int colorId =
                            filter.getTabGroupColorWithFallback(assumeNonNull(tab.getTabGroupId()));
                    final @StringRes int colorDescRes =
                            TabGroupColorPickerUtils
                                    .getTabGroupColorPickerItemColorAccessibilityString(colorId);
                    String colorDesc = res.getString(colorDescRes);
                    String description;
                    if (TabUiUtils.isDataSharingFunctionalityEnabled() && hasCollaboration(tab)) {
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
                    String mediaStateString = getMediaStateAccessibilityString(tab, res);
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

    private void updateActionButtonDescriptionString(Tab tab, PropertyModel model) {
        TextResolver descriptionTextResolver;
        if (mActionsOnAllRelatedTabs) {
            boolean isInTabGroup = isTabInTabGroup(tab);
            int numOfRelatedTabs = getRelatedTabsForId(tab.getId()).size();
            if (isInTabGroup) {
                String title = getLatestTitleForTab(tab, /* useDefault= */ false);

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
                            R.string.accessibility_tabstrip_btn_close_tab, tab.getTitle());
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

    @VisibleForTesting
    String getLatestTitleForTab(Tab tab, boolean useDefault) {
        if (!mActionsOnAllRelatedTabs || !isTabInTabGroup(tab)) {
            String originalTitle = tab.getTitle();
            if (TextUtils.isEmpty(originalTitle)) {
                String url = tab.getUrl().getSpec();
                return TextUtils.isEmpty(url) ? "" : url;
            }
            return originalTitle;
        }

        Token tabGroupId = tab.getTabGroupId();
        assumeNonNull(tabGroupId);
        TabGroupModelFilter filter = mCurrentTabGroupModelFilterSupplier.get();
        if (useDefault) {
            return TabGroupTitleUtils.getDisplayableTitle(mActivity, filter, tabGroupId);
        } else {
            String storedTitle = filter.getTabGroupTitle(tabGroupId);
            return TextUtils.isEmpty(storedTitle) ? "" : storedTitle;
        }
    }

    int selectedTabId() {
        if (mNextTabId != Tab.INVALID_TAB_ID) {
            return mNextTabId;
        }

        return TabModelUtils.getCurrentTabId(
                mCurrentTabGroupModelFilterSupplier.get().getTabModel());
    }

    private void setupPersistedTabDataFetcherForTab(Tab tab, int index) {
        PropertyModel model = mModelList.get(index).model;
        if (mMode == TabListMode.GRID && !tab.isIncognito()) {
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

    private void updateFaviconForTab(
            PropertyModel model, Tab tab, @Nullable Bitmap icon, @Nullable GURL iconUrl) {
        if (mActionsOnAllRelatedTabs && isTabInTabGroup(tab)) {
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
     * Inserts a special {@link org.chromium.ui.modelutil.MVCListAdapter.ListItem} at given index of
     * the current {@link TabListModel}.
     *
     * @param index The index of the {@link org.chromium.ui.modelutil.MVCListAdapter.ListItem} to be
     *     inserted.
     * @param uiType The view type the model will bind to.
     * @param model The model that will be bound to a view.
     */
    void addSpecialItemToModel(int index, @UiType int uiType, PropertyModel model) {
        mModelList.add(index, new ListItem(uiType, model));
    }

    /**
     * Removes a special {@link org.chromium.ui.modelutil.MVCListAdapter.ListItem} that has the
     * given {@code uiType} and/or its {@link PropertyModel} has the given {@code itemIdentifier}
     * from the current {@link TabListModel}.
     *
     * @param uiType The uiType to match.
     * @param itemIdentifier The itemIdentifier to match. This can be obsoleted if the {@link
     *     org.chromium.ui.modelutil.MVCListAdapter.ListItem} does not need additional identifier.
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
     * Removes a {@link org.chromium.ui.modelutil.MVCListAdapter.ListItem} that has the given {@code
     * uiType} and the {@link PropertyModel} has the given {@link TabListEditorItemSelectionId}.
     *
     * @param uiType The uiType to match.
     * @param itemId The itemId to match.
     */
    void removeListItemFromModelList(@UiType int uiType, TabListEditorItemSelectionId itemId) {
        int index = TabModel.INVALID_TAB_INDEX;
        if (uiType == UiType.TAB_GROUP && itemId.isTabGroupSyncId()) {
            String syncId = itemId.getTabGroupSyncId();
            assumeNonNull(syncId);
            index = mModelList.indexFromSyncId(syncId);
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
     *         user enters the tab switcher.
     */
    int getPriceWelcomeMessageInsertionIndex() {
        assert mGridLayoutManager != null;
        int spanCount = mGridLayoutManager.getSpanCount();
        int selectedTabIndex =
                mModelList.indexOfNthTabCard(
                        mCurrentTabGroupModelFilterSupplier
                                .get()
                                .getCurrentRepresentativeTabIndex());
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
        if (mOriginalProfile == null
                || !PriceTrackingUtilities.isPriceWelcomeMessageCardEnabled(mOriginalProfile)
                || mCurrentTabGroupModelFilterSupplier.get().getTabModel().isIncognitoBranded()) {
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
                || mCurrentTabGroupModelFilterSupplier.get().getTabModel().isIncognitoBranded()
                || !mActionsOnAllRelatedTabs
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
                if (model.get(CARD_TYPE) != TAB) continue;

                int modelTabId = model.get(TAB_ID);
                if (relatedTabIds.contains(modelTabId)) {
                    return i;
                }
            }
        }
        return TabModel.INVALID_TAB_INDEX;
    }

    /** Provides the tab ID for the most recently swiped tab. */
    ObservableSupplier<Integer> getRecentlySwipedTabSupplier() {
        return mTabGridItemTouchHelperCallback.getRecentlySwipedTabIdSupplier();
    }

    /**
     * Returns the index in {@link mModelList} of the group with {@code tabGroupId} and the {@link
     * Tab} representing the group. Will be null if the entry is not present, the tab cannot be
     * found, or the tab is not part of a tab group.
     */
    private @Nullable Pair<Integer, Tab> getIndexAndTabForTabGroupId(@Nullable Token tabGroupId) {
        if (tabGroupId == null) return null;

        TabGroupModelFilter filter = mCurrentTabGroupModelFilterSupplier.get();
        assumeNonNull(filter);
        @TabId int lastShownTabId = filter.getGroupLastShownTabId(tabGroupId);

        int index = getIndexForTabIdWithRelatedTabs(lastShownTabId);
        if (index == TabModel.INVALID_TAB_INDEX) return null;

        Tab tab = getTabForIndex(index);
        // If the found tab has a different group ID from the tabGroupId set in the args then the
        // update is likely for a group that no longer exists so we should drop the update.
        if (tab == null
                || !tabGroupId.equals(tab.getTabGroupId())
                || !filter.isTabInTabGroup(tab)) {
            return null;
        }
        return Pair.create(index, tab);
    }

    private @Nullable Tab getTabForIndex(int index) {
        return mCurrentTabGroupModelFilterSupplier
                .get()
                .getTabModel()
                .getTabById(mModelList.get(index).model.get(TabProperties.TAB_ID));
    }

    private void onTabGroupModelFilterChanged(
            @Nullable TabGroupModelFilter newFilter, @Nullable TabGroupModelFilter oldFilter) {
        removeObservers(oldFilter);

        // The observers will be bound to the newFilter's when the model is reset for with tabs for
        // that filter for the first time. Doing this on the first reset after changing models
        // makes sense as otherwise we will be observing updates when the mModelList contains tabs
        // for the oldFilter which can result in invalid updates.
    }

    private void addObservers(TabGroupModelFilter filter, List<Tab> tabs) {
        assert filter != null;

        if (mActionsOnAllRelatedTabs) {
            for (Tab rootTab : tabs) {
                for (Tab tab : filter.getRelatedTabList(rootTab.getId())) {
                    tab.addObserver(mTabObserver);
                }
            }
        } else {
            for (Tab tab : tabs) {
                tab.addObserver(mTabObserver);
            }
        }

        filter.addObserver(mTabModelObserver);
        filter.addTabGroupObserver(mTabGroupObserver);
    }

    private void removeObservers(@Nullable TabGroupModelFilter filter) {
        if (filter == null) return;

        TabModel tabModel = filter.getTabModel();
        if (tabModel != null) {
            // Observers are added when tabs are shown via addTabInfoToModel(). When switching
            // filters the TabObservers should be removed from all the tabs in the previous model.
            // If no observer was added this will no-op. Previously this was only done in
            // destroy(), but that left observers behind on the inactive model.
            for (Tab tab : tabModel) {
                tab.removeObserver(mTabObserver);
            }
        }
        filter.removeObserver(mTabModelObserver);
        filter.removeTabGroupObserver(mTabGroupObserver);
    }

    /**
     * @param itemIdentifier The itemIdentifier to match.
     * @return whether a special {@link org.chromium.ui.modelutil.MVCListAdapter.ListItem} with the
     *     given {@code itemIdentifier} for its {@link PropertyModel} exists in the current {@link
     *     TabListModel}.
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
        TreeMap<Integer, List<Integer>> bottomValuesToTabIndexes = new TreeMap<>();
        getOrderOfTabsForQuickDeleteAnimation(recyclerView, tabs, bottomValuesToTabIndexes);

        setQuickDeleteAnimationStatusForTabIndexes(
                CollectionUtil.flatten(bottomValuesToTabIndexes.values()),
                QuickDeleteAnimationStatus.TAB_PREPARE);

        // Create the gradient drawable and prepare the animator.
        int tabGridHeight = recyclerView.getHeight();
        int intersectionHeight =
                QuickDeleteAnimationGradientDrawable.getAnimationsIntersectionHeight(tabGridHeight);
        QuickDeleteAnimationGradientDrawable gradientDrawable =
                QuickDeleteAnimationGradientDrawable.createQuickDeleteWipeAnimationDrawable(
                        mActivity,
                        tabGridHeight,
                        mCurrentTabGroupModelFilterSupplier
                                .get()
                                .getTabModel()
                                .isIncognitoBranded());

        ObjectAnimator wipeAnimation = gradientDrawable.createWipeAnimator(tabGridHeight);

        wipeAnimation.addUpdateListener(
                valueAnimator -> {
                    if (bottomValuesToTabIndexes.isEmpty()) return;

                    float value = (float) valueAnimator.getAnimatedValue();
                    int bottomVal = bottomValuesToTabIndexes.lastKey();
                    if (bottomVal >= Math.round(value) + intersectionHeight) {
                        setQuickDeleteAnimationStatusForTabIndexes(
                                assumeNonNull(bottomValuesToTabIndexes.get(bottomVal)),
                                QuickDeleteAnimationStatus.TAB_HIDE);
                        bottomValuesToTabIndexes.remove(bottomVal);
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
        assert mMode == TabListMode.STRIP;

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
                    int tabId = model.get(TabProperties.TAB_ID);
                    model.set(TabProperties.TAB_CARD_LABEL_DATA, labelData.get(tabId));
                };
        forAllTabListItems(labelData.keySet(), updateTabCardLabel);
    }

    private void forAllTabListItems(
            Set<Integer> tabIdsToBeUpdated, Callback<PropertyModel> updateCallback) {
        for (int i = 0; i < mModelList.size(); i++) {
            PropertyModel model = mModelList.get(i).model;
            if (model.get(CARD_TYPE) != TAB) continue;

            int tabId = model.get(TabProperties.TAB_ID);
            if (tabIdsToBeUpdated.contains(tabId)) {
                updateCallback.onResult(model);
                Tab tab = mCurrentTabGroupModelFilterSupplier.get().getTabModel().getTabById(tabId);
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
            TreeMap<Integer, List<Integer>> bottomValuesToTabIndexes) {
        Set<Tab> filteredTabs = filterQuickDeleteTabsForAnimation(tabs);

        for (Tab tab : filteredTabs) {
            int id = tab.getId();
            int index = mModelList.indexFromTabId(id);
            Rect tabRect = recyclerView.getRectOfCurrentThumbnail(index, id);

            // Ignore tabs that are outside the screen view.
            if (tabRect == null) continue;

            int bottom = tabRect.bottom;

            if (bottomValuesToTabIndexes.containsKey(bottom)) {
                bottomValuesToTabIndexes.get(bottom).add(index);
            } else {
                bottomValuesToTabIndexes.put(bottom, new ArrayList<>(List.of(index)));
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
        TabGroupModelFilter filter = mCurrentTabGroupModelFilterSupplier.get();
        assert filter != null;

        Set<Tab> unfilteredTabs = new HashSet<>(tabs);
        Set<Tab> filteredTabs = new HashSet<>();
        Set<Token> checkedTabGroupIds = new HashSet<>();

        // Migrating this to tab group id requires a rewrite as the root id based logic assumes that
        // TabGroupModelFilter treats individual tabs similar to tab groups.
        for (Tab tab : unfilteredTabs) {
            if (!filter.isTabInTabGroup(tab)) {
                filteredTabs.add(tab);
                continue;
            }

            Token tabGroupId = tab.getTabGroupId();
            assumeNonNull(tabGroupId);
            if (checkedTabGroupIds.contains(tabGroupId)) continue;
            checkedTabGroupIds.add(tabGroupId);

            List<Tab> relatedTabs = filter.getTabsInGroup(tabGroupId);
            if (unfilteredTabs.containsAll(relatedTabs)) {
                int groupIndex = filter.representativeIndexOf(tab);
                Tab groupTab = filter.getRepresentativeTabAt(groupIndex);
                filteredTabs.add(groupTab);
            }
        }

        return filteredTabs;
    }

    private void setQuickDeleteAnimationStatusForTabIndexes(
            List<Integer> indexes, @QuickDeleteAnimationStatus int animationStatus) {
        for (int index : indexes) {
            mModelList
                    .get(index)
                    .model
                    .set(TabProperties.QUICK_DELETE_ANIMATION_STATUS, animationStatus);
        }
    }

    @VisibleForTesting
    void onMenuItemClicked(
            @IdRes int menuId,
            Token tabGroupId,
            @Nullable String collaborationId,
            @Nullable ListViewTouchTracker listViewTouchTracker) {
        TabGroupModelFilter filter = mCurrentTabGroupModelFilterSupplier.get();
        int tabId = filter.getGroupLastShownTabId(tabGroupId);
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

            setUseShrinkCloseAnimation(tabId, /* useShrinkCloseAnimation= */ true);
            onGroupClosedFrom(tabId);
            TabUiUtils.closeTabGroup(
                    filter,
                    tabId,
                    TabClosingSource.UNKNOWN,
                    allowUndo,
                    hideTabGroups,
                    getOnMaybeTabClosedCallback(tabId));
        } else if (menuId == R.id.edit_group_name) {
            RecordUserAction.record("TabGroupItemMenu.Rename");
            renameTabGroup(tabId);
        } else if (menuId == R.id.ungroup_tab) {
            RecordUserAction.record("TabGroupItemMenu.Ungroup");
            TabUiUtils.ungroupTabGroup(filter, tabGroupId);
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

        TabModel tabModel = mCurrentTabGroupModelFilterSupplier.get().getTabModel();
        Tab tab = tabModel.getTabById(tabId);
        assumeNonNull(tab);
        Token tabGroupId = tab.getTabGroupId();
        assumeNonNull(tabGroupId);
        TabGroupModelFilter filter = mCurrentTabGroupModelFilterSupplier.get();

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
                            boolean stillExists = filter.tabGroupExists(tabGroupId);
                            @TabGroupColorId
                            int oldColorId = filter.getTabGroupColorWithFallback(tabGroupId);
                            @TabGroupColorId
                            int currentColorId =
                                    tabGroupVisualDataDialogManager.getCurrentColorId();
                            boolean didChangeColor = oldColorId != currentColorId;
                            if (didChangeColor) {
                                if (stillExists) {
                                    filter.setTabGroupColor(tabGroupId, currentColorId);
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
                                    filter.setTabGroupTitle(tabGroupId, inputGroupTitle);
                                }
                                RecordUserAction.record("TabGroup.RenameDialog.TitleChanged");
                            }
                        }

                        tabGroupVisualDataDialogManager.onHideDialog();
                    }
                };

        tabGroupVisualDataDialogManager.showDialog(tab.getTabGroupId(), filter, dialogController);
    }

    private TextResolver getActionButtonDescriptionTextResolver(
            int numOfRelatedTabs, String title, Tab tab) {
        TabGroupModelFilter filter = mCurrentTabGroupModelFilterSupplier.get();
        Token tabGroupId = tab.getTabGroupId();
        assumeNonNull(tabGroupId);
        @TabGroupColorId int colorId = filter.getTabGroupColorWithFallback(tabGroupId);
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
        TabGroupModelFilter filter = mCurrentTabGroupModelFilterSupplier.get();
        TabModel tabModel = filter.getTabModel();
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
    @Nullable
    Callback<Boolean> getOnMaybeTabClosedCallback(int tabId) {
        TabGroupModelFilter filter = mCurrentTabGroupModelFilterSupplier.get();

        Tab tab = filter.getTabModel().getTabById(tabId);
        if (tab == null) return null;

        return (didClose) -> {
            if (!didClose) {
                sTabClosedFromMapTabClosedFromMap.remove(tabId);
                setUseShrinkCloseAnimation(tabId, /* useShrinkCloseAnimation= */ false);
                int modelIndex = mModelList.indexFromTabId(tabId);
                if (modelIndex != TabModel.INVALID_TAB_INDEX) {
                    resetSwipe(modelIndex);
                }
                return;
            }

            RecordUserAction.record("MobileTabClosed." + mComponentName);

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

    private void updateThumbnailFetcher(PropertyModel model, int tabId) {
        if (mThumbnailProvider == null) return;

        @Nullable ThumbnailFetcher oldFetcher = model.get(THUMBNAIL_FETCHER);
        if (oldFetcher != null) oldFetcher.cancel();

        @Nullable ThumbnailFetcher newFetcher = null;
        if (tabId != Tab.INVALID_TAB_ID) {
            TabGroupModelFilter filter = mCurrentTabGroupModelFilterSupplier.get();
            Tab tab = filter.getTabModel().getTabById(tabId);
            if (tab == null) return;

            boolean isInTabGroup = filter.tabGroupExists(tab.getTabGroupId());
            final @Nullable @TabGroupColorId Integer tabGroupColor =
                    isInTabGroup
                            ? filter.getTabGroupColorWithFallback(
                                    assumeNonNull(tab.getTabGroupId()))
                            : null;
            newFetcher =
                    new ThumbnailFetcher(
                            mThumbnailProvider,
                            MultiThumbnailMetadata.createMetadataWithoutUrls(
                                    tabId,
                                    isInTabGroup,
                                    filter.getTabModel().isIncognitoBranded(),
                                    tabGroupColor));
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

        boolean isIncognito =
                mCurrentTabGroupModelFilterSupplier.get().getTabModel().isIncognitoBranded();
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

    private void updateTabGroupColorViewProvider(
            PropertyModel model, Tab tab, @TabGroupColorId int colorId) {
        @Nullable TabGroupColorViewProvider provider = model.get(TAB_GROUP_COLOR_VIEW_PROVIDER);

        @Nullable Token tabGroupId = tab.getTabGroupId();
        if (!mActionsOnAllRelatedTabs || tabGroupId == null || !isTabInTabGroup(tab)) {
            // Not a group or not in group display mode.
            model.set(TabProperties.TAB_GROUP_CARD_COLOR, null);
            model.set(TAB_GROUP_COLOR_VIEW_PROVIDER, null);
            if (provider != null) provider.destroy();

            return;
        }

        updateTabGroupColorViewProvider(
                model, EitherGroupId.createLocalId(new LocalTabGroupId(tabGroupId)), colorId);
    }

    private void updateTabGroupColorViewProvider(
            PropertyModel model, EitherGroupId groupId, @TabGroupColorId int colorId) {
        // Set tab group color.
        model.set(TabProperties.TAB_GROUP_CARD_COLOR, colorId);
        assert colorId != TabGroupColorUtils.INVALID_COLOR_ID
                : "Tab in tab group should always have valid colors.";
        assert mMode != TabListMode.STRIP : "Tab group colors are not applicable to strip mode.";

        @Nullable TabGroupColorViewProvider provider = model.get(TAB_GROUP_COLOR_VIEW_PROVIDER);
        if (provider == null) {
            boolean isIncognitoBranded =
                    mCurrentTabGroupModelFilterSupplier.get().getTabModel().isIncognitoBranded();
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
        Snackbar snackbar =
                Snackbar.make(
                        mActivity.getString(R.string.tab_item_picker_limit_reached),
                        null,
                        Snackbar.TYPE_NOTIFICATION,
                        Snackbar.UMA_TAB_PICKER_LIMIT_REACHED);
        mSnackbarManager.showSnackbar(snackbar);
    }

    private void dismissLimitSnackbar() {
        if (mSnackbarManager == null) return;
        mSnackbarManager.dismissAllSnackbars();
    }

    private String getMediaStateAccessibilityString(Tab tab, Resources res) {
        @MediaState int mediaState = getTabGridMediaIndicator(tab);
        switch (mediaState) {
            case MediaState.AUDIBLE:
                return res.getString(R.string.accessibility_tab_group_audible);
            case MediaState.MUTED:
                return res.getString(R.string.accessibility_tab_group_muted);
            case MediaState.RECORDING:
                return res.getString(R.string.accessibility_tab_group_recording);
            case MediaState.SHARING:
                return res.getString(R.string.accessibility_tab_group_sharing);
            default:
                return "";
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

    void setComponentNameForTesting(String name) {
        var oldValue = mComponentName;
        mComponentName = name;
        ResettersForTesting.register(() -> mComponentName = oldValue);
    }

    void setActionOnAllRelatedTabsForTesting(boolean actionOnAllRelatedTabs) {
        var oldValue = mActionsOnAllRelatedTabs;
        mActionsOnAllRelatedTabs = actionOnAllRelatedTabs;
        ResettersForTesting.register(() -> mActionsOnAllRelatedTabs = oldValue);
    }
}

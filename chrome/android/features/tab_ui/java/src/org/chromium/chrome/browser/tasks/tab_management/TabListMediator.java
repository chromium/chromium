// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.chrome.browser.tasks.tab_management.MessageCardViewProperties.MESSAGE_TYPE;
import static org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.CARD_ALPHA;
import static org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.CARD_TYPE;
import static org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.ModelType.TAB;
import static org.chromium.chrome.browser.tasks.tab_management.TabProperties.ACTION_BUTTON_DESCRIPTION_STRING;
import static org.chromium.chrome.browser.tasks.tab_management.TabProperties.TAB_ID;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.animation.ObjectAnimator;
import android.content.ComponentCallbacks;
import android.content.Context;
import android.content.res.ColorStateList;
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
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.StringRes;
import androidx.annotation.VisibleForTesting;
import androidx.appcompat.content.res.AppCompatResources;
import androidx.recyclerview.widget.GridLayoutManager;
import androidx.recyclerview.widget.ItemTouchHelper;
import androidx.recyclerview.widget.RecyclerView;
import androidx.recyclerview.widget.RecyclerView.OnScrollListener;

import org.chromium.base.Callback;
import org.chromium.base.Log;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.ValueChangedCallback;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.Supplier;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.price_tracking.PriceTrackingFeatures;
import org.chromium.chrome.browser.price_tracking.PriceTrackingUtilities;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.quick_delete.QuickDeleteAnimationGradientDrawable;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tab.state.ShoppingPersistedTabData;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncFeatures;
import org.chromium.chrome.browser.tab_ui.TabListFaviconProvider;
import org.chromium.chrome.browser.tab_ui.TabListFaviconProvider.TabFaviconFetcher;
import org.chromium.chrome.browser.tab_ui.ThumbnailProvider;
import org.chromium.chrome.browser.tabmodel.TabList;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelFilter;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupColorUtils;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilterObserver;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupUtils;
import org.chromium.chrome.browser.tasks.tab_management.ActionConfirmationManager.ConfirmationResult;
import org.chromium.chrome.browser.tasks.tab_management.PriceMessageService.PriceTabData;
import org.chromium.chrome.browser.tasks.tab_management.TabGridView.QuickDeleteAnimationStatus;
import org.chromium.chrome.browser.tasks.tab_management.TabListCoordinator.TabListMode;
import org.chromium.chrome.browser.tasks.tab_management.TabProperties.TabActionState;
import org.chromium.chrome.browser.tasks.tab_management.TabProperties.UiType;
import org.chromium.chrome.browser.tasks.tab_management.TabUiMetricsHelper.TabListEditorActionMetricGroups;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.browser_ui.widget.selectable_list.SelectionDelegate;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.tab_groups.TabGroupColorId;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.ListObservable;
import org.chromium.ui.modelutil.ListObservable.ListObserver;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;
import org.chromium.url.GURL;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.Collection;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Objects;
import java.util.Set;
import java.util.TreeMap;
import java.util.stream.Collectors;

/**
 * Mediator for business logic for the tab grid. This class should be initialized with a list of
 * tabs and a TabModel to observe for changes and should not have any logic around what the list
 * signifies. TODO(yusufo): Move some of the logic here to a parent component to make the above
 * true.
 */
class TabListMediator {
    // Set to true after a `resetWithListOfTabs` that used a non-null list of tabs. Remains true
    // until `postHiding` is invoked or the mediator is destroyed. While true, this mediator is
    // actively tracking updates to a TabModel.
    private boolean mShowingTabs;
    private boolean mShownIPH;
    private Tab mTabToAddDelayed;

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

    /** Provides capability to asynchronously acquire {@link ShoppingPersistedTabData} */
    static class ShoppingPersistedTabDataFetcher {
        protected final Tab mTab;
        protected final Supplier<PriceWelcomeMessageController>
                mPriceWelcomeMessageControllerSupplier;

        /**
         * @param tab {@link Tab} {@link ShoppingPersistedTabData} will be acquired for.
         * @param priceWelcomeMessageControllerSupplier to show the price welcome message.
         */
        ShoppingPersistedTabDataFetcher(
                Tab tab,
                @NonNull
                        Supplier<PriceWelcomeMessageController>
                                priceWelcomeMessageControllerSupplier) {
            mTab = tab;
            mPriceWelcomeMessageControllerSupplier = priceWelcomeMessageControllerSupplier;
        }

        /**
         * Asynchronously acquire {@link ShoppingPersistedTabData}
         * @param callback {@link Callback} to pass {@link ShoppingPersistedTabData} back in
         */
        public void fetch(Callback<ShoppingPersistedTabData> callback) {
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

    /**
     * The object to set to {@link TabProperties#THUMBNAIL_FETCHER} for the TabGridViewBinder to
     * obtain the thumbnail asynchronously.
     */
    static class ThumbnailFetcher {
        static Callback<Bitmap> sBitmapCallbackForTesting;
        static int sFetchCountForTesting;
        private ThumbnailProvider mThumbnailProvider;
        private int mId;
        private boolean mForceUpdate;
        private boolean mWriteToCache;

        ThumbnailFetcher(
                ThumbnailProvider provider, int id, boolean forceUpdate, boolean writeToCache) {
            mThumbnailProvider = provider;
            mId = id;
            mForceUpdate = forceUpdate;
            mWriteToCache = writeToCache;
        }

        void fetch(Callback<Bitmap> callback, Size thumbnailSize, boolean isSelected) {
            Callback<Bitmap> forking =
                    (bitmap) -> {
                        if (sBitmapCallbackForTesting != null) {
                            sBitmapCallbackForTesting.onResult(bitmap);
                        }
                        callback.onResult(bitmap);
                    };
            sFetchCountForTesting++;
            mThumbnailProvider.getTabThumbnailWithCallback(
                    mId, thumbnailSize, forking, mForceUpdate, mWriteToCache, isSelected);
        }
    }

    /** An interface to show IPH for a tab. */
    public interface IphProvider {
        void showIPH(View anchor);
    }

    private final IphProvider mIphProvider =
            new IphProvider() {
                private static final int IPH_DELAY_MS = 1000;

                @Override
                public void showIPH(View anchor) {
                    if (mShownIPH) return;
                    mShownIPH = true;

                    PostTask.postDelayedTask(
                            TaskTraits.UI_DEFAULT,
                            () -> {
                                TabGroupUtils.maybeShowIPH(
                                        mProfile,
                                        FeatureConstants.TAB_GROUPS_YOUR_TABS_ARE_TOGETHER_FEATURE,
                                        anchor,
                                        null);
                            },
                            IPH_DELAY_MS);
                }
            };

    /**
     * An interface to get a SelectionDelegate that contains the selected items for a selectable
     * tab list.
     */
    public interface SelectionDelegateProvider {
        SelectionDelegate getSelectionDelegate();
    }

    /** An interface to get the onClickListener when clicking on a grid card. */
    interface GridCardOnClickListenerProvider {
        /**
         * @return {@link TabActionListener} to open Tab Grid dialog.
         * If the given {@link Tab} is not able to create group, return null;
         */
        @Nullable
        TabActionListener openTabGridDialog(@NonNull Tab tab);

        /**
         * Run additional actions on tab selection.
         * @param tabId The ID of selected {@link Tab}.
         * @param fromActionButton Whether it is called from the Action button on the card.
         */
        void onTabSelecting(int tabId, boolean fromActionButton);
    }

    /** A class that stores shared info regarding a tab group's state. */
    static class TabGroupInfo {
        private boolean mShouldShowDeleteTabGroup;
        private boolean mIsTabGroup;

        TabGroupInfo(boolean shouldShowDeleteGroup, boolean isTabGroup) {
            mShouldShowDeleteTabGroup = shouldShowDeleteGroup;
            mIsTabGroup = isTabGroup;
        }

        boolean getShouldShowDeleteTabGroup() {
            return mShouldShowDeleteTabGroup;
        }

        boolean getIsTabGroup() {
            return mIsTabGroup;
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
        int NUM_ENTRIES = 4;
    }

    private static final String TAG = "TabListMediator";
    private static Map<Integer, Integer> sTabClosedFromMapTabClosedFromMap = new HashMap<>();
    private static Set<Integer> sViewedTabIds = new HashSet<>();

    private final Context mContext;
    private final TabListModel mModel;
    private final @TabListMode int mMode;
    private final ModalDialogManager mModalDialogManager;
    private final ObservableSupplier<TabModelFilter> mCurrentTabModelFilterSupplier;
    private final ValueChangedCallback<TabModelFilter> mOnTabModelFilterChanged =
            new ValueChangedCallback<>(this::onTabModelFilterChanged);
    private final TabActionListener mTabClosedListener;
    private final SelectionDelegateProvider mSelectionDelegateProvider;
    private final GridCardOnClickListenerProvider mGridCardOnClickListenerProvider;
    private final TabGridDialogHandler mTabGridDialogHandler;
    private final TabListFaviconProvider mTabListFaviconProvider;
    private final Supplier<PriceWelcomeMessageController> mPriceWelcomeMessageControllerSupplier;
    private final TabGroupColorFaviconProvider mTabGroupColorFaviconProvider;
    private final TabListGroupMenuCoordinator.OnItemClickedCallback mOnMenuItemClickedCallback =
            this::onMenuItemClicked;
    private final ActionConfirmationManager mActionConfirmationManager;
    private final Runnable mOnTabGroupCreation;

    private @Nullable Profile mProfile;
    private Size mDefaultGridCardSize;
    private String mComponentName;
    private ThumbnailProvider mThumbnailProvider;
    private boolean mActionsOnAllRelatedTabs;
    private ComponentCallbacks mComponentCallbacks;
    private TabGridItemTouchHelperCallback mTabGridItemTouchHelperCallback;
    private int mNextTabId = Tab.INVALID_TAB_ID;
    private @TabActionState int mTabActionState;
    private GridLayoutManager mGridLayoutManager;
    // mRecyclerView and mOnScrollListener are null, unless the the price drop IPH or badge is
    // enabled.
    private @Nullable RecyclerView mRecyclerView;
    private @Nullable OnScrollListener mOnScrollListener;

    private final TabActionListener mTabSelectedListener =
            new TabActionListener() {
                @Override
                public void run(int tabId) {
                    if (mModel.indexFromId(tabId) == TabModel.INVALID_TAB_INDEX) return;

                    mNextTabId = tabId;

                    TabModel tabModel = mCurrentTabModelFilterSupplier.get().getTabModel();
                    if (!mActionsOnAllRelatedTabs) {
                        Tab currentTab = TabModelUtils.getCurrentTab(tabModel);
                        Tab newlySelectedTab = TabModelUtils.getTabById(tabModel, tabId);

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
                                TabSelectionType.FROM_USER,
                                false);
                    }
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
                    TabModelFilter filter = mCurrentTabModelFilterSupplier.get();
                    int fromFilterIndex = filter.indexOf(fromTab);
                    int toFilterIndex = filter.indexOf(toTab);

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
                public void run(int tabId) {
                    int index = mModel.indexFromId(tabId);
                    if (index == TabModel.INVALID_TAB_INDEX) return;
                    boolean selected = mModel.get(index).model.get(TabProperties.IS_SELECTED);
                    if (selected) {
                        TabUiMetricsHelper.recordSelectionEditorActionMetrics(
                                TabListEditorActionMetricGroups.UNSELECTED);
                    } else {
                        TabUiMetricsHelper.recordSelectionEditorActionMetrics(
                                TabListEditorActionMetricGroups.SELECTED);
                    }
                    mModel.get(index).model.set(TabProperties.IS_SELECTED, !selected);
                    // Reset thumbnail to ensure the color of the blank tab slots is correct.
                    TabModelFilter filter = mCurrentTabModelFilterSupplier.get();
                    Tab tab = TabModelUtils.getTabById(filter.getTabModel(), tabId);
                    if (tab != null && filter.isTabInTabGroup(tab)) {
                        mModel.get(index)
                                .model
                                .set(
                                        TabProperties.THUMBNAIL_FETCHER,
                                        new ThumbnailFetcher(
                                                mThumbnailProvider, tabId, false, false));
                    }
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
                    if (mModel.indexFromId(tab.getId()) == TabModel.INVALID_TAB_INDEX
                            || (mActionsOnAllRelatedTabs
                                    && mCurrentTabModelFilterSupplier.get().isTabInTabGroup(tab))) {
                        return;
                    }

                    mModel.get(mModel.indexFromId(tab.getId()))
                            .model
                            .set(
                                    TabProperties.FAVICON_FETCHER,
                                    mTabListFaviconProvider.getDefaultFaviconFetcher(
                                            tab.isIncognito()));
                }

                @Override
                public void onTitleUpdated(Tab updatedTab) {
                    assert mShowingTabs;

                    int index = mModel.indexFromId(updatedTab.getId());
                    // TODO(crbug.com/40136874) The null check for tab here should be redundant once
                    // we have resolved the bug.
                    if (index == TabModel.INVALID_TAB_INDEX
                            || TabModelUtils.getTabById(
                                            mCurrentTabModelFilterSupplier.get().getTabModel(),
                                            updatedTab.getId())
                                    == null) {
                        return;
                    }
                    mModel.get(index)
                            .model
                            .set(
                                    TabProperties.TITLE,
                                    getLatestTitleForTab(updatedTab, /* useDefault= */ true));
                }

                @Override
                public void onFaviconUpdated(Tab updatedTab, Bitmap icon, GURL iconUrl) {
                    assert mShowingTabs;

                    if (!mActionsOnAllRelatedTabs) {
                        updateFaviconForTab(updatedTab, icon, iconUrl);
                        return;
                    }

                    Tab tab = null;
                    if (isTabInTabGroup(updatedTab)) {
                        @Nullable
                        Pair<Integer, Tab> indexAndTab =
                                getIndexAndTabForRootId(updatedTab.getRootId());
                        if (indexAndTab == null) return;

                        tab = indexAndTab.second;

                        if (mThumbnailProvider != null) {
                            PropertyModel model = mModel.get(indexAndTab.first).model;
                            model.set(
                                    TabProperties.THUMBNAIL_FETCHER,
                                    new ThumbnailFetcher(
                                            mThumbnailProvider, tab.getId(), false, false));
                        }
                    } else {
                        tab = updatedTab;
                    }

                    updateFaviconForTab(tab, icon, iconUrl);
                }

                @Override
                public void onUrlUpdated(Tab updatedTab) {
                    assert mShowingTabs;

                    int index = mModel.indexFromId(updatedTab.getId());

                    if (index != TabModel.INVALID_TAB_INDEX) {
                        mModel.get(index)
                                .model
                                .set(TabProperties.URL_DOMAIN, getDomainForTab(updatedTab));
                    } else if (mActionsOnAllRelatedTabs) {
                        @Nullable
                        Pair<Integer, Tab> indexAndTab =
                                getIndexAndTabForRootId(updatedTab.getRootId());
                        if (indexAndTab == null) return;
                        Tab tab = indexAndTab.second;
                        PropertyModel model = mModel.get(indexAndTab.first).model;

                        model.set(TabProperties.URL_DOMAIN, getDomainForTab(tab));
                    }
                }
            };

    /** Interface for toggling whether item animations will run on the recycler view. */
    interface RecyclerViewItemAnimationToggle {
        void setDisableItemAnimations(boolean state);
    }

    private RecyclerViewItemAnimationToggle mRecyclerViewItemAnimationToggle;

    private final TabModelObserver mTabModelObserver;

    private ListObserver<Void> mListObserver;

    private TabGroupTitleEditor mTabGroupTitleEditor;

    private View.AccessibilityDelegate mAccessibilityDelegate;

    private int mLastSelectedTabListModelIndex = TabList.INVALID_TAB_INDEX;

    private final TabGroupModelFilterObserver mTabGroupObserver =
            new TabGroupModelFilterObserver() {
                @Override
                public void didChangeTabGroupTitle(int rootId, String newTitle) {
                    assert mShowingTabs;

                    if (!mActionsOnAllRelatedTabs) return;

                    @Nullable Pair<Integer, Tab> indexAndTab = getIndexAndTabForRootId(rootId);
                    if (indexAndTab == null) return;
                    Tab tab = indexAndTab.second;
                    PropertyModel model = mModel.get(indexAndTab.first).model;

                    // Do not trust the `newTitle`, it may be necessary to apply a default/fallback.
                    newTitle = getLatestTitleForTab(tab, /* useDefault= */ true);

                    model.set(TabProperties.TITLE, newTitle);
                    updateDescriptionString(tab, model);
                    updateActionButtonDescriptionString(tab, model);
                }

                @Override
                public void didChangeTabGroupColor(int rootId, @TabGroupColorId int newColor) {
                    assert mShowingTabs;

                    if (!ChromeFeatureList.sTabGroupParityAndroid.isEnabled()) return;

                    if (!mActionsOnAllRelatedTabs) return;

                    @Nullable Pair<Integer, Tab> indexAndTab = getIndexAndTabForRootId(rootId);
                    if (indexAndTab == null) return;
                    Tab tab = indexAndTab.second;
                    PropertyModel model = mModel.get(indexAndTab.first).model;

                    if (mMode == TabListMode.LIST) {
                        model.set(TabProperties.TAB_GROUP_COLOR_ID, newColor);
                    } else if (mMode == TabListMode.GRID) {
                        updateFaviconForTab(tab, null, null);
                    }
                    updateDescriptionString(tab, model);
                    updateActionButtonDescriptionString(tab, model);
                }

                @Override
                public void didMoveWithinGroup(
                        Tab movedTab, int tabModelOldIndex, int tabModelNewIndex) {
                    assert mShowingTabs;

                    if (tabModelNewIndex == tabModelOldIndex) return;

                    TabModelFilter filter = mCurrentTabModelFilterSupplier.get();
                    TabModel tabModel = filter.getTabModel();

                    // For the tab switcher update the tab card correctly.
                    if (mActionsOnAllRelatedTabs && mThumbnailProvider != null) {
                        int indexInModel = getIndexForTabWithRelatedTabs(movedTab);
                        if (indexInModel == TabModel.INVALID_TAB_INDEX) return;

                        Tab lastShownTab = filter.getTabAt(filter.indexOf(movedTab));
                        mModel.get(indexInModel)
                                .model
                                .set(
                                        TabProperties.THUMBNAIL_FETCHER,
                                        new ThumbnailFetcher(
                                                mThumbnailProvider,
                                                lastShownTab.getId(),
                                                true,
                                                false));
                        return;
                    }

                    // For the grid dialog or tab strip maintain order.
                    int curPosition = mModel.indexFromId(movedTab.getId());

                    if (!isValidMovePosition(curPosition)) return;

                    Tab destinationTab =
                            tabModel.getTabAt(
                                    tabModelNewIndex > tabModelOldIndex
                                            ? tabModelNewIndex - 1
                                            : tabModelNewIndex + 1);

                    int newPosition = mModel.indexFromId(destinationTab.getId());

                    if (!isValidMovePosition(newPosition)) return;
                    mModel.move(curPosition, newPosition);
                }

                @Override
                public void didMoveTabOutOfGroup(Tab movedTab, int prevFilterIndex) {
                    assert mShowingTabs;

                    assert !(mActionsOnAllRelatedTabs && mTabGridDialogHandler != null);

                    TabGroupModelFilter filter =
                            (TabGroupModelFilter) mCurrentTabModelFilterSupplier.get();
                    Tab previousGroupTab = filter.getTabAt(prevFilterIndex);
                    boolean isUngroupingLastTabInGroup =
                            previousGroupTab.getId() == movedTab.getId();
                    if (mActionsOnAllRelatedTabs) {
                        // When ungrouping the last tab in a group no update was needed. However,
                        // with tab groups of size 1 an update is still needed to ensure the
                        // thumbnail, title, etc. get correctly represented.
                        if (isUngroupingLastTabInGroup
                                && !ChromeFeatureList.sAndroidTabGroupStableIds.isEnabled()) {
                            return;
                        }

                        final int currentSelectedTabId =
                                TabModelUtils.getCurrentTabId(filter.getTabModel());
                        // Only add a tab to the model if it represents a new card (new group or new
                        // singular tab). However, always update the previous group to clean up old
                        // state. The addition of the new tab to an existing group is handled in
                        // didMergeTabToGroup().
                        if (filter.getRelatedTabCountForRootId(movedTab.getRootId()) == 1
                                && movedTab != previousGroupTab) {
                            int filterIndex = filter.indexOf(movedTab);
                            addTabInfoToModel(
                                    movedTab,
                                    mModel.indexOfNthTabCard(filterIndex),
                                    currentSelectedTabId == movedTab.getId());
                        }
                        boolean isSelected = currentSelectedTabId == previousGroupTab.getId();
                        updateTab(
                                mModel.indexOfNthTabCard(prevFilterIndex),
                                previousGroupTab,
                                isSelected,
                                true,
                                false);
                    } else {
                        int curTabListModelIndex = mModel.indexFromId(movedTab.getId());
                        if (!isValidMovePosition(curTabListModelIndex)) return;
                        mModel.removeAt(curTabListModelIndex);
                        if (mTabGridDialogHandler != null) {
                            mTabGridDialogHandler.updateDialogContent(
                                    isUngroupingLastTabInGroup
                                            ? Tab.INVALID_TAB_ID
                                            : filter.getTabAt(prevFilterIndex).getId());
                        }
                    }
                }

                @Override
                public void didMergeTabToGroup(Tab movedTab, int selectedTabIdInGroup) {
                    assert mShowingTabs;

                    TabGroupModelFilter filter =
                            (TabGroupModelFilter) mCurrentTabModelFilterSupplier.get();
                    TabModel tabModel = filter.getTabModel();
                    if (mActionsOnAllRelatedTabs) {
                        // When merging Tab 1 to Tab 2 as a new group, or merging Tab 1 to an
                        // existing group 1, we can always find the current indexes of 1) Tab 1
                        // and 2) Tab 2 or group 1 in the model. The method
                        // getIndexesForMergeToGroup() returns these two ids by using Tab 1's
                        // related Tabs, which have been updated in
                        // TabModel.
                        List<Tab> relatedTabs = getRelatedTabsForId(movedTab.getId());
                        Pair<Integer, Integer> positions =
                                mModel.getIndexesForMergeToGroup(tabModel, relatedTabs);
                        int srcIndex = positions.second;
                        int desIndex = positions.first;

                        // If only the desIndex is valid then the movedTab was already part of
                        // another group and is not present in the model. This happens only during
                        // an undo.
                        // Refresh just the desIndex tab card in the model. The removal of the
                        // movedTab from its previous group was already handled by
                        // didMoveTabOutOfGroup.
                        if (desIndex != TabModel.INVALID_TAB_INDEX
                                && srcIndex == TabModel.INVALID_TAB_INDEX) {
                            boolean isSelected = false;
                            for (Tab tab : relatedTabs) {
                                isSelected |= tab == TabModelUtils.getCurrentTab(tabModel);
                            }
                            Tab tab =
                                    TabModelUtils.getTabById(
                                            tabModel,
                                            mModel.get(desIndex).model.get(TabProperties.TAB_ID));
                            updateTab(desIndex, tab, isSelected, false, false);
                            return;
                        }

                        if (!isValidMovePosition(srcIndex) || !isValidMovePosition(desIndex)) {
                            return;
                        }

                        Tab newSelectedTabInMergedGroup = null;
                        mModel.removeAt(srcIndex);
                        if (getRelatedTabsForId(movedTab.getId()).size() == 2) {
                            // When users use drop-to-merge to create a group.
                            RecordUserAction.record("TabGroup.Created.DropToMerge");
                        } else {
                            RecordUserAction.record("TabGrid.Drag.DropToMerge");
                        }
                        desIndex =
                                srcIndex > desIndex ? desIndex : mModel.getTabIndexBefore(desIndex);
                        newSelectedTabInMergedGroup =
                                filter.getTabAt(mModel.getTabCardCountsBefore(desIndex));

                        boolean isSelected =
                                TabModelUtils.getCurrentTab(tabModel)
                                        == newSelectedTabInMergedGroup;
                        updateTab(desIndex, newSelectedTabInMergedGroup, isSelected, true, false);
                    } else {
                        // If the model is empty we can't check if the added tab is part of the
                        // current group. Assume it isn't since a group state with 0 tab should be
                        // impossible.
                        if (mModel.size() == 0) return;

                        // If the added tab is part of the group add it and update the dialog.
                        int firstTabId = mModel.get(0).model.get(TabProperties.TAB_ID);
                        Tab firstTab = TabModelUtils.getTabById(tabModel, firstTabId);
                        if (firstTab == null || firstTab.getRootId() != movedTab.getRootId()) {
                            return;
                        }

                        movedTab.addObserver(mTabObserver);
                        onTabAdded(movedTab, /* onlyShowRelatedTabs= */ true);
                        if (mTabGridDialogHandler != null) {
                            mTabGridDialogHandler.updateDialogContent(
                                    filter.getGroupLastShownTabId(firstTab.getRootId()));
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
                    TabModelFilter filter = mCurrentTabModelFilterSupplier.get();
                    Tab currentGroupSelectedTab =
                            TabGroupUtils.getSelectedTabInGroupForTab(
                                    (TabGroupModelFilter) filter, movedTab);
                    TabModel tabModel = filter.getTabModel();
                    int curPosition = mModel.indexFromId(currentGroupSelectedTab.getId());
                    if (curPosition == TabModel.INVALID_TAB_INDEX) {
                        // Sync TabListModel with updated TabGroupModelFilter.
                        int indexToUpdate =
                                mModel.indexOfNthTabCard(
                                        filter.indexOf(tabModel.getTabAt(tabModelOldIndex)));
                        mModel.updateTabListModelIdForGroup(currentGroupSelectedTab, indexToUpdate);
                        curPosition = mModel.indexFromId(currentGroupSelectedTab.getId());
                    }
                    if (!isValidMovePosition(curPosition)) return;

                    // Find the tab which was in the destination index before this move. Use
                    // that tab to figure out the new position.
                    int destinationTabIndex =
                            tabModelNewIndex > tabModelOldIndex
                                    ? tabModelNewIndex - relatedTabs.size()
                                    : tabModelNewIndex + 1;
                    Tab destinationTab = tabModel.getTabAt(destinationTabIndex);
                    Tab destinationGroupSelectedTab =
                            TabGroupUtils.getSelectedTabInGroupForTab(
                                    (TabGroupModelFilter) filter, destinationTab);
                    int newPosition = mModel.indexFromId(destinationGroupSelectedTab.getId());
                    if (newPosition == TabModel.INVALID_TAB_INDEX) {
                        int indexToUpdate =
                                mModel.indexOfNthTabCard(
                                        filter.indexOf(destinationTab)
                                                + (tabModelNewIndex > tabModelOldIndex ? 1 : -1));
                        mModel.updateTabListModelIdForGroup(
                                destinationGroupSelectedTab, indexToUpdate);
                        newPosition = mModel.indexFromId(destinationGroupSelectedTab.getId());
                    }
                    if (!isValidMovePosition(newPosition)) return;

                    mModel.move(curPosition, newPosition);
                }

                @Override
                public void didCreateNewGroup(Tab destinationTab, TabGroupModelFilter filter) {
                    // On new group creation for the tab group representation in the GTS, update
                    // the tab group color icon.
                    if (ChromeFeatureList.sTabGroupParityAndroid.isEnabled()
                            && mMode == TabListMode.LIST) {
                        int groupIndex = filter.indexOf(destinationTab);
                        Tab groupTab = filter.getTabAt(groupIndex);
                        PropertyModel model = getModelFromId(groupTab.getId());

                        if (model != null) {
                            @TabGroupColorId
                            int colorId =
                                    filter.getTabGroupColorWithFallback(destinationTab.getRootId());
                            model.set(TabProperties.TAB_GROUP_COLOR_ID, colorId);
                        }
                    }
                }
            };

    /** Interface for implementing a {@link Runnable} that takes a tabId for a generic action. */
    public interface TabActionListener {
        void run(int tabId);
    }

    /**
     * Construct the Mediator with the given Models and observing hooks from the given
     * ChromeActivity.
     *
     * @param context The context used to get some configuration information.
     * @param model The Model to keep state about a list of {@link Tab}s.
     * @param mode The {@link TabListMode}
     * @param modalDialogManager The {@link ModalDialogManager} for managing dialog lifecycles.
     * @param thumbnailProvider {@link ThumbnailProvider} to provide screenshot related details.
     * @param tabListFaviconProvider Provider for all favicon related drawables.
     * @param tabGroupColorFaviconProvider Provider for tab group color favicon related drawables.
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
     * @param onTabGroupCreation Should be run when the UI is used to create a tab group.
     */
    public TabListMediator(
            Context context,
            TabListModel model,
            @TabListMode int mode,
            @Nullable ModalDialogManager modalDialogManager,
            @NonNull ObservableSupplier<TabModelFilter> tabModelFilterSupplier,
            @Nullable ThumbnailProvider thumbnailProvider,
            TabListFaviconProvider tabListFaviconProvider,
            @NonNull TabGroupColorFaviconProvider tabGroupColorFaviconProvider,
            boolean actionOnRelatedTabs,
            @Nullable SelectionDelegateProvider selectionDelegateProvider,
            @Nullable GridCardOnClickListenerProvider gridCardOnClickListenerProvider,
            @Nullable TabGridDialogHandler dialogHandler,
            @NonNull Supplier<PriceWelcomeMessageController> priceWelcomeMessageControllerSupplier,
            String componentName,
            @TabActionState int initialTabActionState,
            @NonNull ActionConfirmationManager actionConfirmationManager,
            @Nullable Runnable onTabGroupCreation) {
        mContext = context;
        mModalDialogManager = modalDialogManager;
        mCurrentTabModelFilterSupplier = tabModelFilterSupplier;
        mThumbnailProvider = thumbnailProvider;
        mModel = model;
        mMode = mode;
        mTabListFaviconProvider = tabListFaviconProvider;
        mTabGroupColorFaviconProvider = tabGroupColorFaviconProvider;
        mComponentName = componentName;
        mSelectionDelegateProvider = selectionDelegateProvider;
        mGridCardOnClickListenerProvider = gridCardOnClickListenerProvider;
        mTabGridDialogHandler = dialogHandler;
        mActionsOnAllRelatedTabs = actionOnRelatedTabs;
        mTabActionState = initialTabActionState;
        mPriceWelcomeMessageControllerSupplier = priceWelcomeMessageControllerSupplier;
        mProfile = mCurrentTabModelFilterSupplier.get().getTabModel().getProfile();
        mActionConfirmationManager = actionConfirmationManager;
        mOnTabGroupCreation = onTabGroupCreation;

        mTabModelObserver =
                new TabModelObserver() {
                    @Override
                    public void didSelectTab(Tab tab, int type, int lastId) {
                        assert mShowingTabs;

                        mNextTabId = Tab.INVALID_TAB_ID;
                        if (tab.getId() == lastId) return;

                        int oldIndex = mModel.indexFromId(lastId);
                        int newIndex = mModel.indexFromId(tab.getId());
                        if (newIndex == TabModel.INVALID_TAB_INDEX && mActionsOnAllRelatedTabs) {
                            // If a tab in tab group does not exist in model and needs to be
                            // selected, identify the related tab ids and determine newIndex
                            // based on if any of the related ids are present in model.
                            newIndex = getIndexForTabWithRelatedTabs(tab);
                            // For UNDO ensure we update the representative tab in the model.
                            if (type == TabSelectionType.FROM_UNDO
                                    && newIndex != Tab.INVALID_TAB_ID) {
                                model.updateTabListModelIdForGroup(tab, newIndex);
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
                            TabModelFilter filter = mCurrentTabModelFilterSupplier.get();
                            int filterIndex = filter.indexOf(tab);
                            if (filterIndex == TabList.INVALID_TAB_INDEX
                                    || !filter.isTabInTabGroup(tab)
                                    || filterIndex >= mModel.size()) {
                                return;
                            }
                            Tab currentGroupSelectedTab = filter.getTabAt(filterIndex);

                            int tabListModelIndex = mModel.indexOfNthTabCard(filterIndex);
                            assert mModel.indexFromId(currentGroupSelectedTab.getId())
                                    == tabListModelIndex;

                            updateTab(
                                    tabListModelIndex,
                                    currentGroupSelectedTab,
                                    mModel.get(tabListModelIndex)
                                            .model
                                            .get(TabProperties.IS_SELECTED),
                                    false,
                                    false);
                        }
                    }

                    @Override
                    public void didAddTab(
                            Tab tab,
                            @TabLaunchType int type,
                            @TabCreationState int creationState,
                            boolean markedForSelection) {
                        assert mShowingTabs;

                        TabModelFilter filter = mCurrentTabModelFilterSupplier.get();
                        if (filter == null || !filter.isTabModelRestored()) {
                            return;
                        }

                        tab.addObserver(mTabObserver);

                        // Check if we need to delay tab addition to model.
                        boolean delayAdd =
                                (type == TabLaunchType.FROM_TAB_SWITCHER_UI)
                                        && markedForSelection
                                        && TabSwitcherCoordinator.COMPONENT_NAME.equals(
                                                mComponentName);
                        if (delayAdd) {
                            mTabToAddDelayed = tab;
                            return;
                        }

                        onTabAdded(tab, !mActionsOnAllRelatedTabs);
                        if (type == TabLaunchType.FROM_RESTORE && mActionsOnAllRelatedTabs) {
                            // When tab is restored after restoring stage (e.g. exiting multi-window
                            // mode, switching between dark/light mode in incognito), we need to
                            // update related property models.
                            int filterIndex = filter.indexOf(tab);
                            if (filterIndex == TabList.INVALID_TAB_INDEX) return;
                            Tab currentGroupSelectedTab = filter.getTabAt(filterIndex);
                            // TabModel and TabListModel may be in the process of syncing up through
                            // restoring. Examples of this situation are switching between
                            // light/dark mode in incognito, exiting multi-window mode, etc.
                            int tabListModelIndex = mModel.indexOfNthTabCard(filterIndex);
                            if (mModel.indexFromId(currentGroupSelectedTab.getId())
                                    != tabListModelIndex) {
                                return;
                            }
                            updateTab(
                                    tabListModelIndex,
                                    currentGroupSelectedTab,
                                    mModel.get(tabListModelIndex)
                                            .model
                                            .get(TabProperties.IS_SELECTED),
                                    false,
                                    false);
                        }
                    }

                    @Override
                    public void willCloseTab(Tab tab, boolean didCloseAlone) {
                        assert mShowingTabs;

                        tab.removeObserver(mTabObserver);

                        // If the tab closed was part of a tab group and the closure was triggered
                        // from the tab switcher, update the group to reflect the closure instead of
                        // closing the tab.
                        if (mActionsOnAllRelatedTabs
                                && (mCurrentTabModelFilterSupplier.get()
                                        instanceof TabGroupModelFilter groupFilter)
                                && groupFilter.tabGroupExistsForRootId(tab.getRootId())) {
                            int groupIndex = groupFilter.indexOf(tab);
                            Tab groupTab = groupFilter.getTabAt(groupIndex);
                            if (!groupTab.isClosing()) {
                                final int currentSelectedTabId =
                                        TabModelUtils.getCurrentTabId(groupFilter.getTabModel());
                                boolean isSelected = currentSelectedTabId == groupTab.getId();
                                updateTab(groupIndex, groupTab, isSelected, true, false);

                                return;
                            }
                        }

                        if (mModel.indexFromId(tab.getId()) == TabModel.INVALID_TAB_INDEX) return;
                        mModel.removeAt(mModel.indexFromId(tab.getId()));
                    }

                    @Override
                    public void tabRemoved(Tab tab) {
                        assert mShowingTabs;

                        tab.removeObserver(mTabObserver);

                        if (mModel.indexFromId(tab.getId()) == TabModel.INVALID_TAB_INDEX) return;
                        mModel.removeAt(mModel.indexFromId(tab.getId()));
                    }
                };

        // TODO(meiliang): follow up with unit tests to test the close signal is sent correctly with
        // the recommendedNextTab.
        mTabClosedListener =
                new TabActionListener() {
                    @Override
                    public void run(int tabId) {
                        // TODO(crbug.com/40638921): Consider disabling all touch events during
                        // animation.
                        if (mModel.indexFromId(tabId) == TabModel.INVALID_TAB_INDEX) return;

                        TabGroupModelFilter filter =
                                (TabGroupModelFilter) mCurrentTabModelFilterSupplier.get();
                        TabModel tabModel = filter.getTabModel();
                        Tab closingTab = TabModelUtils.getTabById(tabModel, tabId);
                        if (closingTab == null) return;

                        RecordUserAction.record("MobileTabClosed." + mComponentName);

                        if (mActionsOnAllRelatedTabs && filter.isTabInTabGroup(closingTab)) {
                            List<Tab> related = getRelatedTabsForId(tabId);
                            onGroupClosedFrom(tabId);
                            filter.closeMultipleTabs(
                                    related, /* canUndo= */ true, /* hideTabGroups= */ true);
                            return;
                        }
                        onTabClosedFrom(tabId, mComponentName);

                        Tab currentTab = TabModelUtils.getCurrentTab(tabModel);
                        Tab nextTab = currentTab == closingTab ? getNextTab(tabId) : null;

                        tabModel.closeTab(closingTab, nextTab, false, true);
                    }

                    private Tab getNextTab(int closingTabId) {
                        int closingTabIndex = mModel.indexFromId(closingTabId);

                        if (closingTabIndex == TabModel.INVALID_TAB_INDEX) {
                            assert false;
                            return null;
                        }

                        int nextTabId = Tab.INVALID_TAB_ID;
                        if (mModel.size() > 1) {
                            int nextTabIndex =
                                    closingTabIndex == 0
                                            ? mModel.getTabIndexAfter(closingTabIndex)
                                            : mModel.getTabIndexBefore(closingTabIndex);
                            nextTabId =
                                    nextTabIndex == TabModel.INVALID_TAB_INDEX
                                            ? Tab.INVALID_TAB_ID
                                            : mModel.get(nextTabIndex)
                                                    .model
                                                    .get(TabProperties.TAB_ID);
                        }

                        return TabModelUtils.getTabById(
                                mCurrentTabModelFilterSupplier.get().getTabModel(), nextTabId);
                    }
                };

        TabActionListener swipeSafeTabActionListener =
                (id) -> {
                    // The DefaultItemAnimator is prone to crashing in combination with the swipe
                    // animation.
                    // Avoid this issue by disabling the default item animation for the duration of
                    // the tab removal. This is a framework issue. For more details see
                    // crbug/1319859.
                    mRecyclerViewItemAnimationToggle.setDisableItemAnimations(true);
                    mTabClosedListener.run(id);
                    // It is necessary to post the restoration as otherwise any animation triggered
                    // by removing the tab will still use the animator as they are also posted to
                    // the UI thread.
                    new Handler()
                            .post(
                                    () -> {
                                        mRecyclerViewItemAnimationToggle.setDisableItemAnimations(
                                                false);
                                    });
                };

        var tabGroupCreationDialogManager =
                new TabGroupCreationDialogManager(context, modalDialogManager, mOnTabGroupCreation);
        mTabGridItemTouchHelperCallback =
                new TabGridItemTouchHelperCallback(
                        context,
                        tabGroupCreationDialogManager,
                        mModel,
                        mCurrentTabModelFilterSupplier,
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
            @Nullable
                    TabGridItemTouchHelperCallback.OnLongPressTabItemEventListener
                            onLongPressTabItemEventListener) {
        mTabGridItemTouchHelperCallback.setOnLongPressTabItemEventListener(
                onLongPressTabItemEventListener);
    }

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
    Size getDefaultGridCardSize() {
        return mDefaultGridCardSize;
    }

    private void selectTab(int oldIndex, int newIndex) {
        // TODO(crbug.com/347886633): Change the bounds check to an assert.
        if (oldIndex != TabModel.INVALID_TAB_INDEX && oldIndex < mModel.size()) {
            int lastId = mModel.get(oldIndex).model.get(TAB_ID);
            mModel.get(oldIndex).model.set(TabProperties.IS_SELECTED, false);
            if (mActionsOnAllRelatedTabs && mThumbnailProvider != null && mShowingTabs) {
                mModel.get(oldIndex)
                        .model
                        .set(
                                TabProperties.THUMBNAIL_FETCHER,
                                new ThumbnailFetcher(mThumbnailProvider, lastId, true, false));
            }
        }

        if (newIndex != TabModel.INVALID_TAB_INDEX) {
            int newId = mModel.get(newIndex).model.get(TAB_ID);
            mModel.get(newIndex).model.set(TabProperties.IS_SELECTED, true);
            if (mThumbnailProvider != null && mShowingTabs) {
                mModel.get(newIndex)
                        .model
                        .set(
                                TabProperties.THUMBNAIL_FETCHER,
                                new ThumbnailFetcher(mThumbnailProvider, newId, true, false));
            }
        }
    }

    public void initWithNative(Profile profile) {
        assert !profile.isOffTheRecord() : "Expecting a non-incognito profile.";
        mProfile = profile;
        mTabListFaviconProvider.initWithNative(profile);

        mOnTabModelFilterChanged.onResult(
                mCurrentTabModelFilterSupplier.addObserver(mOnTabModelFilterChanged));

        mTabGroupTitleEditor =
                new TabGroupTitleEditor(mContext) {
                    @Override
                    protected void updateTabGroupTitle(Tab tab, String title) {
                        // Only update title in PropertyModel for tab switcher.
                        if (!mActionsOnAllRelatedTabs) return;
                        Tab currentGroupSelectedTab =
                                TabGroupUtils.getSelectedTabInGroupForTab(
                                        (TabGroupModelFilter) mCurrentTabModelFilterSupplier.get(),
                                        tab);
                        int index = mModel.indexFromId(currentGroupSelectedTab.getId());
                        if (index == TabModel.INVALID_TAB_INDEX) return;
                        mModel.get(index).model.set(TabProperties.TITLE, title);
                        updateDescriptionString(tab, mModel.get(index).model);
                        updateActionButtonDescriptionString(tab, mModel.get(index).model);
                    }

                    @Override
                    protected void deleteTabGroupTitle(int tabRootId) {
                        TabGroupModelFilter filter =
                                (TabGroupModelFilter) mCurrentTabModelFilterSupplier.get();
                        filter.deleteTabGroupTitle(tabRootId);
                    }

                    @Override
                    protected String getTabGroupTitle(int tabRootId) {
                        TabGroupModelFilter filter =
                                (TabGroupModelFilter) mCurrentTabModelFilterSupplier.get();
                        return filter.getTabGroupTitle(tabRootId);
                    }

                    @Override
                    protected void storeTabGroupTitle(int tabRootId, String title) {
                        TabGroupModelFilter filter =
                                (TabGroupModelFilter) mCurrentTabModelFilterSupplier.get();
                        filter.setTabGroupTitle(tabRootId, title);
                    }
                };

        // Right now we need to update layout only if there is a price welcome message card in tab
        // switcher.
        if (mMode == TabListMode.GRID
                && mTabActionState != TabActionState.SELECTABLE
                && PriceTrackingFeatures.isPriceTrackingEnabled(profile)) {
            mListObserver =
                    new ListObserver<Void>() {
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
            mModel.addObserver(mListObserver);
        }
    }

    private void onTabClosedFrom(int tabId, String fromComponent) {
        @TabClosedFrom int from;
        if (fromComponent.equals(TabGroupUiCoordinator.COMPONENT_NAME)) {
            from = TabClosedFrom.TAB_STRIP;
        } else if (fromComponent.equals(TabSwitcherCoordinator.COMPONENT_NAME)) {
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

    void setActionOnAllRelatedTabsForTesting(boolean actionOnAllRelatedTabs) {
        var oldValue = mActionsOnAllRelatedTabs;
        mActionsOnAllRelatedTabs = actionOnAllRelatedTabs;
        ResettersForTesting.register(() -> mActionsOnAllRelatedTabs = oldValue);
    }

    private List<Tab> getRelatedTabsForId(int id) {
        TabModelFilter filter = mCurrentTabModelFilterSupplier.get();
        return filter == null ? new ArrayList<>() : filter.getRelatedTabList(id);
    }

    private List<Integer> getRelatedTabsIds(int id) {
        TabModelFilter filter = mCurrentTabModelFilterSupplier.get();
        return filter == null ? new ArrayList<>() : filter.getRelatedTabIds(id);
    }

    private int getInsertionIndexOfTab(Tab tab, boolean onlyShowRelatedTabs) {
        int index = TabList.INVALID_TAB_INDEX;
        if (tab == null) return index;
        if (onlyShowRelatedTabs) {
            if (mModel.size() == 0) return TabList.INVALID_TAB_INDEX;
            List<Tab> related = getRelatedTabsForId(mModel.get(0).model.get(TabProperties.TAB_ID));
            index = related.indexOf(tab);
            if (index == -1) return TabList.INVALID_TAB_INDEX;
        } else {
            index =
                    mModel.indexOfNthTabCard(
                            TabModelUtils.getTabIndexById(
                                    mCurrentTabModelFilterSupplier.get(), tab.getId()));
            // TODO(wychen): the title (tab count in the group) is wrong when it's not the last
            //  tab added in the group.
        }
        return index;
    }

    private int onTabAdded(Tab tab, boolean onlyShowRelatedTabs) {
        int existingIndex = mModel.indexFromId(tab.getId());
        if (existingIndex != TabModel.INVALID_TAB_INDEX) return existingIndex;

        int newIndex = getInsertionIndexOfTab(tab, onlyShowRelatedTabs);
        if (newIndex == TabList.INVALID_TAB_INDEX) return newIndex;

        Tab currentTab =
                TabModelUtils.getCurrentTab(mCurrentTabModelFilterSupplier.get().getTabModel());
        addTabInfoToModel(tab, newIndex, currentTab == tab);
        return newIndex;
    }

    private boolean isValidMovePosition(int position) {
        return position != TabModel.INVALID_TAB_INDEX && position < mModel.size();
    }

    private boolean areTabsUnchanged(@Nullable List<Tab> tabs) {
        int tabsCount = 0;
        for (int i = 0; i < mModel.size(); i++) {
            if (mModel.get(i).model.get(CARD_TYPE) == TAB) {
                tabsCount += 1;
            }
        }
        if (tabs == null) {
            return tabsCount == 0;
        }
        if (tabs.size() != tabsCount) return false;
        int tabsIndex = 0;
        for (int i = 0; i < mModel.size(); i++) {
            if (mModel.get(i).model.get(CARD_TYPE) == TAB
                    && mModel.get(i).model.get(TabProperties.TAB_ID)
                            != tabs.get(tabsIndex++).getId()) {
                return false;
            }
        }
        return true;
    }

    /**
     * Initialize the component with a list of tabs to show in a grid.
     *
     * @param tabs The list of tabs to be shown.
     * @param quickMode Whether to skip capturing the selected live tab for the thumbnail.
     * @return Whether the {@link TabListRecyclerView} can be shown quickly.
     */
    boolean resetWithListOfTabs(@Nullable List<Tab> tabs, boolean quickMode) {
        mShowingTabs = tabs != null;
        TabModelFilter filter = mCurrentTabModelFilterSupplier.get();
        if (mShowingTabs) {
            addObservers(filter, tabs);
        } else {
            removeObservers(filter);
        }
        if (tabs != null) {
            recordPriceAnnotationsEnabledMetrics();
        }
        if (areTabsUnchanged(tabs)) {
            if (tabs == null) return true;

            int currentTabId = TabModelUtils.getCurrentTabId(filter.getTabModel());
            for (int i = 0; i < tabs.size(); i++) {
                Tab tab = tabs.get(i);
                boolean isSelected = isSelectedTab(tab, currentTabId);
                updateTab(mModel.indexOfNthTabCard(i), tab, isSelected, false, quickMode);
            }
            mLastSelectedTabListModelIndex = TabList.INVALID_TAB_INDEX;
            return true;
        }
        mModel.set(new ArrayList<>());
        mLastSelectedTabListModelIndex = TabList.INVALID_TAB_INDEX;

        if (tabs == null) {
            return true;
        }
        int currentTabId = TabModelUtils.getCurrentTabId(filter.getTabModel());

        for (int i = 0; i < tabs.size(); i++) {
            Tab tab = tabs.get(i);
            addTabInfoToModel(tab, i, isSelectedTab(tab, currentTabId));
        }

        return false;
    }

    /**
     * Add the tab id of a {@Tab} that has been viewed to the sViewedTabIds set.
     * @param tabIndex  The tab index of a {@Tab} the user has viewed.
     */
    private void addViewedTabId(int tabIndex) {
        TabModel tabModel = mCurrentTabModelFilterSupplier.get().getTabModel();
        assert !tabModel.isIncognito();
        int tabId = mModel.get(tabIndex).model.get(TabProperties.TAB_ID);
        assert TabModelUtils.getTabById(tabModel, tabId) != null;
        sViewedTabIds.add(tabId);
    }

    void postHiding() {
        removeObservers(mCurrentTabModelFilterSupplier.get());
        mShowingTabs = false;
        unregisterOnScrolledListener();
        // if tab was marked for add later, add to model and mark as selected.
        if (mTabToAddDelayed != null) {
            int index = onTabAdded(mTabToAddDelayed, !mActionsOnAllRelatedTabs);
            selectTab(mLastSelectedTabListModelIndex, index);
            mTabToAddDelayed = null;
        }
    }

    private boolean isSelectedTab(Tab tab, int tabModelSelectedTabId) {
        SelectionDelegate<Integer> selectionDelegate = getTabSelectionDelegate();
        if (selectionDelegate == null) {
            return tab.getId() == tabModelSelectedTabId;
        } else {
            return selectionDelegate.isItemSelected(tab.getId());
        }
    }

    /**
     * @see TabSwitcherMediator.ResetHandler#softCleanup
     */
    void softCleanup() {
        assert !mShowingTabs;
        for (int i = 0; i < mModel.size(); i++) {
            if (mModel.get(i).model.get(CARD_TYPE) == TAB) {
                mModel.get(i).model.set(TabProperties.THUMBNAIL_FETCHER, null);
                mModel.get(i).model.set(TabProperties.FAVICON_FETCHER, null);
            }
        }
    }

    void hardCleanup() {
        assert !mShowingTabs;
        if (mProfile != null
                && PriceTrackingUtilities.isTrackPricesOnTabsEnabled(mProfile)
                && (PriceTrackingFeatures.isPriceDropIphEnabled(mProfile)
                        || PriceTrackingFeatures.isPriceDropBadgeEnabled(mProfile))) {
            saveSeenPriceDrops();
        }
        sViewedTabIds.clear();
    }

    /**
     * While leaving the tab switcher grid this update whether a tab's current price drop has or has
     * not been seen.
     */
    // TODO(crbug.com/343206772): Move code to TabSwitcherPane.
    private void saveSeenPriceDrops() {
        // The filter determines what's shown in the tab list.
        TabModelFilter filter = mCurrentTabModelFilterSupplier.get();
        // The filter's underlying model should have any tab that was viewed.
        TabModel model = filter.getTabModel();
        for (Integer tabId : sViewedTabIds) {
            Tab tab = TabModelUtils.getTabById(model, tabId);
            if (tab != null && !filter.isTabInTabGroup(tab)) {
                ShoppingPersistedTabData.from(
                        tab,
                        (sptd) -> {
                            if (sptd != null && sptd.getPriceDrop() != null) {
                                sptd.setIsCurrentPriceDropSeen(true);
                            }
                        });
            }
        }
    }

    private void updateTab(
            int index, Tab tab, boolean isSelected, boolean isUpdatingId, boolean quickMode) {
        if (index < 0 || index >= mModel.size()) return;
        if (isUpdatingId) {
            mModel.get(index).model.set(TabProperties.TAB_ID, tab.getId());
        } else {
            assert mModel.get(index).model.get(TabProperties.TAB_ID) == tab.getId();
        }

        boolean isInTabGroup = isTabInTabGroup(tab);
        if (ChromeFeatureList.sTabGroupParityAndroid.isEnabled()) {
            // If the tab to update is in ListMode, update it with the most recent stored color.
            if (mMode == TabListMode.LIST) {
                int tabGroupColorId = TabGroupColorUtils.INVALID_COLOR_ID;
                // Only update the color if the tab is a representation of a tab group, otherwise
                // hide the icon by setting the color to INVALID.
                if (isInTabGroup) {
                    TabGroupModelFilter filter =
                            (TabGroupModelFilter) mCurrentTabModelFilterSupplier.get();
                    tabGroupColorId = filter.getTabGroupColorWithFallback(tab.getRootId());
                }

                PropertyModel model = getModelFromId(tab.getId());
                if (model != null) {
                    model.set(TabProperties.TAB_GROUP_COLOR_ID, tabGroupColorId);
                }
            }
        }

        mModel.get(index)
                .model
                .set(TabProperties.TAB_SELECTED_LISTENER, getTabActionListener(tab, isInTabGroup));
        mModel.get(index).model.set(TabProperties.IS_SELECTED, isSelected);
        mModel.get(index).model.set(TabProperties.SHOULD_SHOW_PRICE_DROP_TOOLTIP, false);
        mModel.get(index)
                .model
                .set(TabProperties.TITLE, getLatestTitleForTab(tab, /* useDefault= */ true));

        mModel.get(index)
                .model
                .set(TabProperties.ON_MENU_ITEM_CLICKED_CALLBACK, mOnMenuItemClickedCallback);
        // A tab is deemed a tab group card representation if it is part of a tab group and
        // based in the tab switcher.
        boolean isTabGroup = isTabInTabGroup(tab) && isParentComponentTabSwitcher();
        // Update the group color icon.
        if (ChromeFeatureList.sTabGroupParityAndroid.isEnabled() && isTabGroup) {
            updateFaviconForTab(tab, null, null);
        }
        // The ordering of TAB_ACTION_BUTTON_LISTENER and IS_TAB_GROUP must be preserved when
        // setting the property keys on the model. Both properties modify the onClickListener
        // so ensure that the default behavior (close on click) is set first, and tab groups
        // under valid circumstances will override the listener with alternate behavior.
        if (!ChromeFeatureList.sTabGroupPaneAndroid.isEnabled() || !isTabGroup) {
            mModel.get(index)
                    .model
                    .set(TabProperties.TAB_ACTION_BUTTON_LISTENER, mTabClosedListener);
        }
        // Only set this for tab group representation cards. An onClickListener will be set in the
        // view as part of the accompanying logic.
        if (ChromeFeatureList.sTabGroupPaneAndroid.isEnabled()) {
            mModel.get(index)
                    .model
                    .set(
                            TabProperties.TAB_GROUP_INFO,
                            new TabGroupInfo(
                                    TabGroupSyncFeatures.isTabGroupSyncEnabled(mProfile),
                                    isTabGroup));
        }

        updateDescriptionString(tab, mModel.get(index).model);
        updateActionButtonDescriptionString(tab, mModel.get(index).model);
        mModel.get(index).model.set(TabProperties.URL_DOMAIN, getDomainForTab(tab));

        setupPersistedTabDataFetcherForTab(tab, index);

        updateFaviconForTab(tab, null, null);
        boolean forceUpdate = isSelected && !quickMode;
        boolean forceUpdateLastSelected =
                mActionsOnAllRelatedTabs && index == mLastSelectedTabListModelIndex && !quickMode;
        // TODO(crbug.com/40273706): Fetching thumbnail for group is expansive, we should consider
        // to improve it.
        if (mThumbnailProvider != null
                && mShowingTabs
                && (mModel.get(index).model.get(TabProperties.THUMBNAIL_FETCHER) == null
                        || forceUpdate
                        || isUpdatingId
                        || forceUpdateLastSelected
                        || isInTabGroup)) {
            boolean isSelectable = mTabActionState == TabActionState.SELECTABLE;
            ThumbnailFetcher callback =
                    new ThumbnailFetcher(
                            mThumbnailProvider,
                            tab.getId(),
                            (forceUpdate || forceUpdateLastSelected) && !isSelectable,
                            forceUpdate
                                    && !TabUiFeatureUtilities.isTabToGtsAnimationEnabled(mContext)
                                    && !isSelectable);
            mModel.get(index).model.set(TabProperties.THUMBNAIL_FETCHER, callback);
        }
    }

    @VisibleForTesting
    public boolean isTabInTabGroup(@NonNull Tab tab) {
        TabModelFilter filter = mCurrentTabModelFilterSupplier.get();
        assert filter.isTabModelRestored();

        return filter.isTabInTabGroup(tab);
    }

    public Set<Integer> getViewedTabIdsForTesting() {
        return sViewedTabIds;
    }

    /**
     * @return The callback that hosts the logic for swipe and drag related actions.
     */
    ItemTouchHelper.SimpleCallback getItemTouchHelperCallback(
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
        mContext.registerComponentCallbacks(mComponentCallbacks);
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
                        int itemType = mModel.get(position).type;

                        if (itemType == TabProperties.UiType.MESSAGE
                                || itemType == TabProperties.UiType.LARGE_MESSAGE
                                || itemType == UiType.DIVIDER
                                || itemType == TabProperties.UiType.CUSTOM_MESSAGE) {
                            return manager.getSpanCount();
                        }
                        return 1;
                    }
                });
        return oldSpanCount != newSpanCount;
    }

    /**
     * Adds an on scroll listener to {@link TabListRecyclerView} that determines whether a tab
     * thumbnail is within view after a scroll is completed.
     * @param recyclerView the {@link TabListRecyclerView} to add the listener too.
     */
    void registerOnScrolledListener(RecyclerView recyclerView) {
        // For InstantStart, this can be called before native is initialized, so ensure the Profile
        // is available before proceeding.
        if (mProfile == null) return;

        if (PriceTrackingUtilities.isTrackPricesOnTabsEnabled(mProfile)
                && (PriceTrackingFeatures.isPriceDropIphEnabled(mProfile)
                        || PriceTrackingFeatures.isPriceDropBadgeEnabled(mProfile))) {
            mRecyclerView = recyclerView;
            mOnScrollListener =
                    new OnScrollListener() {
                        @Override
                        public void onScrolled(@NonNull RecyclerView recyclerView, int dx, int dy) {
                            if (!mCurrentTabModelFilterSupplier.get().getTabModel().isIncognito()) {
                                for (int i = 0; i < mRecyclerView.getChildCount(); i++) {
                                    if (mRecyclerView
                                            .getLayoutManager()
                                            .isViewPartiallyVisible(
                                                    mRecyclerView.getChildAt(i), false, true)) {
                                        addViewedTabId(i);
                                    }
                                }
                            }
                        }
                    };
            mRecyclerView.addOnScrollListener(mOnScrollListener);
        }
    }

    private void unregisterOnScrolledListener() {
        if (mRecyclerView != null && mOnScrollListener != null) {
            mRecyclerView.removeOnScrollListener(mOnScrollListener);
            mOnScrollListener = null;
        }
    }

    /**
     * Span count is computed based on screen width for tablets and orientation for phones.
     * When in multi-window mode on phone, the span count is fixed to 2 to keep tab card size
     * reasonable.
     */
    private int getSpanCount(int screenWidthDp) {
        if (DeviceFormFactor.isNonMultiDisplayContextOnTablet(mContext)) {
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
     * @param helper The {@link TabGridAccessibilityHelper} used to setup accessibility support.
     */
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
                    public boolean performAccessibilityAction(View host, int action, Bundle args) {
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
                        mModel.move(currentPosition, targetPosition);
                        RecordUserAction.record("TabGrid.AccessibilityDelegate.Reordered");
                        return true;
                    }
                };
    }

    /**
     * Exposes a {@link TabGroupTitleEditor} to modify the title of a tab group.
     * @return The {@link TabGroupTitleEditor} used to modify the title of a tab group.
     */
    @Nullable
    TabGroupTitleEditor getTabGroupTitleEditor() {
        return mTabGroupTitleEditor;
    }

    /** Destroy any members that needs clean up. */
    public void destroy() {
        if (mListObserver != null) {
            mModel.removeObserver(mListObserver);
        }
        removeObservers(mCurrentTabModelFilterSupplier.get());
        mCurrentTabModelFilterSupplier.removeObserver(mOnTabModelFilterChanged);

        if (mComponentCallbacks != null) {
            mContext.unregisterComponentCallbacks(mComponentCallbacks);
        }
        unregisterOnScrolledListener();
    }

    void setTabActionState(@TabActionState int tabActionState) {
        if (mTabActionState == tabActionState) return;
        mTabActionState = tabActionState;

        for (int i = 0; i < mModel.size(); i++) {
            ListItem item = mModel.get(i);
            if (item.type != UiType.TAB) continue;
            Tab tab = getTabForIndex(i);
            // Unbind the current TabActionState properties.
            unbindTabActionStateProperties(item.model);

            item.model.set(TabProperties.TAB_ACTION_STATE, mTabActionState);
            bindTabActionStateProperties(tabActionState, tab, item.model);
        }
    }

    private void unbindTabActionStateProperties(PropertyModel model) {
        for (PropertyKey propertyKey : TabProperties.TAB_GRID_SELECTABLE_KEYS) {
            unbindSelectableProperty(model, propertyKey);
        }

        for (PropertyKey propertyKey : TabProperties.TAB_GRID_CLOSABLE_KEYS) {
            unbindClosableProperty(model, propertyKey);
        }
    }

    private void unbindSelectableProperty(
            @NonNull PropertyModel model, @NonNull PropertyKey propertyKey) {
        if (TabProperties.TAB_ACTION_BUTTON_LISTENER == propertyKey) {
            model.set(TabProperties.TAB_ACTION_BUTTON_LISTENER, null);
        } else if (TabProperties.CHECKED_DRAWABLE_STATE_LIST == propertyKey) {
            model.set(TabProperties.CHECKED_DRAWABLE_STATE_LIST, null);
        } else if (TabProperties.SELECTABLE_TAB_ACTION_BUTTON_BACKGROUND == propertyKey) {
            model.set(TabProperties.SELECTABLE_TAB_ACTION_BUTTON_BACKGROUND, null);
        } else if (TabProperties.SELECTABLE_TAB_CLICKED_LISTENER == propertyKey) {
            model.set(TabProperties.SELECTABLE_TAB_CLICKED_LISTENER, null);
        }
    }

    private void unbindClosableProperty(
            @NonNull PropertyModel model, @NonNull PropertyKey propertyKey) {
        if (TabProperties.TAB_ACTION_BUTTON_LISTENER == propertyKey) {
            model.set(TabProperties.TAB_ACTION_BUTTON_LISTENER, null);
        } else if (TabProperties.TAB_SELECTED_LISTENER == propertyKey) {
            model.set(TabProperties.TAB_SELECTED_LISTENER, null);
        } else if (TabProperties.ON_MENU_ITEM_CLICKED_CALLBACK == propertyKey) {
            model.set(TabProperties.ON_MENU_ITEM_CLICKED_CALLBACK, null);
        }
    }

    private void bindTabActionStateProperties(
            @TabActionState int tabActionState, Tab tab, PropertyModel model) {
        model.set(TabProperties.IS_SELECTED, isTabSelected(tabActionState, tab));
        boolean isInTabGroup = isTabInTabGroup(tab);
        if (mTabActionState == TabActionState.SELECTABLE) {
            // Incognito in both light/dark theme is the same as non-incognito mode in dark theme.
            // Non-incognito mode and incognito in both light/dark themes in dark theme all look
            // dark.
            ColorStateList checkedDrawableColorList =
                    ColorStateList.valueOf(
                            tab.isIncognito()
                                    ? mContext.getColor(R.color.default_icon_color_dark)
                                    : SemanticColorUtils.getDefaultIconColorInverse(mContext));
            ColorStateList actionButtonBackgroundColorList =
                    AppCompatResources.getColorStateList(
                            mContext,
                            tab.isIncognito()
                                    ? R.color.default_icon_color_light
                                    : R.color.default_icon_color_tint_list);
            // TODO(crbug.com/41477267): Update color baseline_primary_80 to active_color_dark when
            // the associated bug is landed.
            ColorStateList actionbuttonSelectedBackgroundColorList =
                    ColorStateList.valueOf(
                            tab.isIncognito()
                                    ? mContext.getColor(R.color.baseline_primary_80)
                                    : SemanticColorUtils.getDefaultControlColorActive(mContext));

            model.set(TabProperties.CHECKED_DRAWABLE_STATE_LIST, checkedDrawableColorList);
            model.set(
                    TabProperties.SELECTABLE_TAB_ACTION_BUTTON_BACKGROUND,
                    actionButtonBackgroundColorList);
            model.set(
                    TabProperties.SELECTABLE_TAB_ACTION_BUTTON_SELECTED_BACKGROUND,
                    actionbuttonSelectedBackgroundColorList);
            model.set(TabProperties.SELECTABLE_TAB_CLICKED_LISTENER, mSelectableTabOnClickListener);
        } else {
            model.set(
                    TabProperties.IS_SELECTED,
                    TabModelUtils.getCurrentTabId(
                                    mCurrentTabModelFilterSupplier.get().getTabModel())
                            == tab.getId());
            model.set(TabProperties.TAB_SELECTED_LISTENER, getTabActionListener(tab, isInTabGroup));

            model.set(TabProperties.ON_MENU_ITEM_CLICKED_CALLBACK, mOnMenuItemClickedCallback);
            // A tab is deemed a tab group card representation if it is part of a tab group and
            // based in the tab switcher.
            boolean isTabGroup = isTabInTabGroup(tab) && isParentComponentTabSwitcher();
            // The ordering of TAB_ACTION_BUTTON_LISTENER and IS_TAB_GROUP must be preserved when
            // setting the property keys on the model. Both properties modify the onClickListener
            // so ensure that the default behavior (close on click) is set first, and tab groups
            // under valid circumstances will override the listener with alternate behavior.
            if (!ChromeFeatureList.sTabGroupPaneAndroid.isEnabled() || !isTabGroup) {
                model.set(TabProperties.TAB_ACTION_BUTTON_LISTENER, mTabClosedListener);
            }
            // Only set this for tab group representation cards. An onClickListener will be set in
            // the view as part of the accompanying logic.
            if (ChromeFeatureList.sTabGroupPaneAndroid.isEnabled()) {
                model.set(
                        TabProperties.TAB_GROUP_INFO,
                        new TabGroupInfo(
                                TabGroupSyncFeatures.isTabGroupSyncEnabled(mProfile), isTabGroup));
            }
            // Can this be done separately from SELECTABLE/CLOSABLE?
            updateDescriptionString(tab, model);
            updateActionButtonDescriptionString(tab, model);
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
            return selectionDelegate.isItemSelected(tab.getId());
        } else {
            return TabModelUtils.getCurrentTabId(mCurrentTabModelFilterSupplier.get().getTabModel())
                    == tab.getId();
        }
    }

    private void addTabInfoToModel(Tab tab, int index, boolean isSelected) {
        assert index != TabModel.INVALID_TAB_INDEX;
        boolean showIPH = false;
        boolean isInTabGroup = isTabInTabGroup(tab);
        if (mActionsOnAllRelatedTabs && !mShownIPH) {
            showIPH = isInTabGroup;
        }

        int colorId = TabGroupColorUtils.INVALID_COLOR_ID;
        if (ChromeFeatureList.sTabGroupParityAndroid.isEnabled()) {
            // While groups always have a color, only set it here when it should be shown next to
            // the title. In GRID mode this is not the case, as the color replaces the favicon.
            // Rather it's LIST mode where we do this, and additionally not when we've opened a
            // dialog for a particular group, checked by isParentComponentTabSwitcher().
            if (mMode == TabListMode.LIST && isInTabGroup && isParentComponentTabSwitcher()) {
                TabGroupModelFilter filter =
                        (TabGroupModelFilter) mCurrentTabModelFilterSupplier.get();
                colorId = filter.getTabGroupColorWithFallback(tab.getRootId());
            }
        }

        int selectedTabBackgroundDrawableId =
                tab.isIncognito()
                        ? R.drawable.selected_tab_background_incognito
                        : R.drawable.selected_tab_background;

        int tabstripFaviconBackgroundDrawableId =
                tab.isIncognito()
                        ? R.color.favicon_background_color_incognito
                        : R.color.favicon_background_color;
        PropertyModel tabInfo =
                new PropertyModel.Builder(TabProperties.ALL_KEYS_TAB_GRID)
                        .with(TabProperties.TAB_ACTION_STATE, mTabActionState)
                        .with(TabProperties.TAB_ID, tab.getId())
                        .with(
                                TabProperties.TITLE,
                                getLatestTitleForTab(tab, /* useDefault= */ true))
                        .with(TabProperties.URL_DOMAIN, getDomainForTab(tab))
                        .with(TabProperties.FAVICON_FETCHER, null)
                        .with(TabProperties.FAVICON_FETCHED, false)
                        .with(TabProperties.IS_SELECTED, isSelected)
                        .with(TabProperties.IPH_PROVIDER, showIPH ? mIphProvider : null)
                        .with(CARD_ALPHA, 1f)
                        .with(
                                TabProperties.CARD_ANIMATION_STATUS,
                                TabGridView.AnimationStatus.CARD_RESTORE)
                        .with(TabProperties.TAB_SELECTION_DELEGATE, getTabSelectionDelegate())
                        .with(TabProperties.IS_INCOGNITO, tab.isIncognito())
                        .with(
                                TabProperties.SELECTED_TAB_BACKGROUND_DRAWABLE_ID,
                                selectedTabBackgroundDrawableId)
                        .with(
                                TabProperties.TABSTRIP_FAVICON_BACKGROUND_COLOR_ID,
                                tabstripFaviconBackgroundDrawableId)
                        .with(TabProperties.ACCESSIBILITY_DELEGATE, mAccessibilityDelegate)
                        .with(TabProperties.SHOULD_SHOW_PRICE_DROP_TOOLTIP, false)
                        .with(CARD_TYPE, TAB)
                        .with(
                                TabProperties.QUICK_DELETE_ANIMATION_STATUS,
                                QuickDeleteAnimationStatus.TAB_RESTORE)
                        .with(TabProperties.TAB_GROUP_COLOR_ID, colorId)
                        .build();

        if (!mActionsOnAllRelatedTabs || !isTabInTabGroup(tab)) {
            tabInfo.set(
                    TabProperties.FAVICON_FETCHER,
                    mTabListFaviconProvider.getDefaultFaviconFetcher(tab.isIncognito()));
        }

        bindTabActionStateProperties(mTabActionState, tab, tabInfo);

        @UiType
        int tabUiType =
                mMode == TabListMode.STRIP ? TabProperties.UiType.STRIP : TabProperties.UiType.TAB;
        if (index >= mModel.size()) {
            mModel.add(new SimpleRecyclerViewAdapter.ListItem(tabUiType, tabInfo));
        } else {
            mModel.add(index, new SimpleRecyclerViewAdapter.ListItem(tabUiType, tabInfo));
        }

        setupPersistedTabDataFetcherForTab(tab, index);

        updateFaviconForTab(tab, null, null);

        if (mThumbnailProvider != null && mDefaultGridCardSize != null) {
            if (!mDefaultGridCardSize.equals(tabInfo.get(TabProperties.GRID_CARD_SIZE))) {
                tabInfo.set(
                        TabProperties.GRID_CARD_SIZE,
                        new Size(
                                mDefaultGridCardSize.getWidth(), mDefaultGridCardSize.getHeight()));
            }
        }
        if (mThumbnailProvider != null && mShowingTabs) {
            boolean isSelectable = mTabActionState == TabActionState.SELECTABLE;
            ThumbnailFetcher callback =
                    new ThumbnailFetcher(
                            mThumbnailProvider,
                            tab.getId(),
                            isSelected && !isSelectable,
                            isSelected
                                    && !TabUiFeatureUtilities.isTabToGtsAnimationEnabled(mContext)
                                    && !isSelectable);
            tabInfo.set(TabProperties.THUMBNAIL_FETCHER, callback);
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
        if (isInTabGroup) {
            String title = getLatestTitleForTab(tab, /* useDefault= */ false);
            Resources res = mContext.getResources();
            if (!ChromeFeatureList.sTabGroupParityAndroid.isEnabled()) {
                model.set(
                        TabProperties.CONTENT_DESCRIPTION_STRING,
                        title.isEmpty()
                                ? res.getQuantityString(
                                        R.plurals.accessibility_expand_tab_group,
                                        numOfRelatedTabs,
                                        numOfRelatedTabs)
                                : res.getQuantityString(
                                        R.plurals.accessibility_expand_tab_group_with_group_name,
                                        numOfRelatedTabs,
                                        title,
                                        numOfRelatedTabs));
            } else {
                TabGroupModelFilter filter =
                        (TabGroupModelFilter) mCurrentTabModelFilterSupplier.get();
                @TabGroupColorId int colorId = filter.getTabGroupColorWithFallback(tab.getRootId());
                final @StringRes int colorDescRes =
                        ColorPickerUtils.getTabGroupColorPickerItemColorAccessibilityString(
                                colorId);
                String colorDesc = res.getString(colorDescRes);
                model.set(
                        TabProperties.CONTENT_DESCRIPTION_STRING,
                        title.isEmpty()
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
                                        colorDesc));
            }
        } else {
            model.set(TabProperties.CONTENT_DESCRIPTION_STRING, null);
        }
    }

    private void updateActionButtonDescriptionString(Tab tab, PropertyModel model) {
        if (mActionsOnAllRelatedTabs) {
            boolean isInTabGroup = isTabInTabGroup(tab);
            int numOfRelatedTabs = getRelatedTabsForId(tab.getId()).size();
            if (isInTabGroup) {
                String title = getLatestTitleForTab(tab, /* useDefault= */ false);

                String descriptionString =
                        getActionButtonDescriptionString(numOfRelatedTabs, title, tab.getRootId());
                model.set(TabProperties.ACTION_BUTTON_DESCRIPTION_STRING, descriptionString);
                return;
            }
        }

        model.set(
                ACTION_BUTTON_DESCRIPTION_STRING,
                mContext.getString(R.string.accessibility_tabstrip_btn_close_tab, tab.getTitle()));
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
    private SelectionDelegate<Integer> getTabSelectionDelegate() {
        return mSelectionDelegateProvider == null
                ? null
                : mSelectionDelegateProvider.getSelectionDelegate();
    }

    @VisibleForTesting
    String getLatestTitleForTab(Tab tab, boolean useDefault) {
        String originalTitle = tab.getTitle();
        if (!mActionsOnAllRelatedTabs || mTabGroupTitleEditor == null || !isTabInTabGroup(tab)) {
            return originalTitle;
        }

        String storedTitle = mTabGroupTitleEditor.getTabGroupTitle(tab.getRootId());
        if (TextUtils.isEmpty(storedTitle)) {
            if (useDefault) {
                TabGroupModelFilter filter =
                        (TabGroupModelFilter) mCurrentTabModelFilterSupplier.get();
                return TabGroupTitleEditor.getDefaultTitle(
                        mContext, filter.getRelatedTabCountForRootId(tab.getRootId()));
            } else {
                return "";
            }
        }
        return storedTitle;
    }

    int selectedTabId() {
        if (mNextTabId != Tab.INVALID_TAB_ID) {
            return mNextTabId;
        }

        return TabModelUtils.getCurrentTabId(mCurrentTabModelFilterSupplier.get().getTabModel());
    }

    private void setupPersistedTabDataFetcherForTab(Tab tab, int index) {
        if (mMode == TabListMode.GRID && !tab.isIncognito()) {
            assert mProfile != null;
            if (PriceTrackingUtilities.isTrackPricesOnTabsEnabled(mProfile)
                    && !isTabInTabGroup(tab)) {
                mModel.get(index)
                        .model
                        .set(
                                TabProperties.SHOPPING_PERSISTED_TAB_DATA_FETCHER,
                                new ShoppingPersistedTabDataFetcher(
                                        tab, mPriceWelcomeMessageControllerSupplier));
            } else {
                mModel.get(index)
                        .model
                        .set(TabProperties.SHOPPING_PERSISTED_TAB_DATA_FETCHER, null);
            }
        } else {
            mModel.get(index).model.set(TabProperties.SHOPPING_PERSISTED_TAB_DATA_FETCHER, null);
        }
    }

    @VisibleForTesting
    void updateFaviconForTab(Tab tab, @Nullable Bitmap icon, @Nullable GURL iconUrl) {
        int modelIndex = mModel.indexFromId(tab.getId());
        if (modelIndex == Tab.INVALID_TAB_ID) return;

        if (mActionsOnAllRelatedTabs && isTabInTabGroup(tab)) {
            List<Tab> relatedTabList = getRelatedTabsForId(tab.getId());
            if (mMode != TabListMode.LIST) {
                // For tab group card in grid tab switcher, the favicon is set to be null.
                // With tab group colors, set the the favicon fetcher to a circle of color.
                TabFaviconFetcher faviconFetcher = null;
                if (ChromeFeatureList.sTabGroupParityAndroid.isEnabled()) {
                    TabGroupModelFilter filter =
                            (TabGroupModelFilter) mCurrentTabModelFilterSupplier.get();
                    @TabGroupColorId
                    int colorId = filter.getTabGroupColorWithFallback(tab.getRootId());
                    faviconFetcher =
                            mTabGroupColorFaviconProvider.getFaviconFromTabGroupColorFetcher(
                                    colorId, filter.getTabModel().isIncognito());
                }

                mModel.get(modelIndex).model.set(TabProperties.FAVICON_FETCHER, faviconFetcher);
                return;
            } else if (mMode == TabListMode.LIST && relatedTabList.size() > 1) {
                // The order of the url list matches the multi-thumbnail.
                List<GURL> urls = new ArrayList<>();
                urls.add(tab.getUrl());
                for (int i = 0; urls.size() < 4 && i < relatedTabList.size(); i++) {
                    if (tab.getId() == relatedTabList.get(i).getId()) continue;
                    urls.add(relatedTabList.get(i).getUrl());
                }

                // For tab group card in list tab switcher, the favicon is the composed favicon.
                mModel.get(modelIndex)
                        .model
                        .set(
                                TabProperties.FAVICON_FETCHER,
                                mTabListFaviconProvider.getComposedFaviconImageFetcher(
                                        urls, tab.isIncognito()));
                return;
            }
        }
        if (!mTabListFaviconProvider.isInitialized()) {
            return;
        }

        // If there is an available icon, we fetch favicon synchronously; otherwise asynchronously.
        if (icon != null && iconUrl != null) {
            mModel.get(modelIndex)
                    .model
                    .set(
                            TabProperties.FAVICON_FETCHER,
                            mTabListFaviconProvider.getFaviconFromBitmapFetcher(icon, iconUrl));
            return;
        }

        TabFaviconFetcher fetcher =
                mTabListFaviconProvider.getFaviconForUrlFetcher(tab.getUrl(), tab.isIncognito());
        mModel.get(modelIndex).model.set(TabProperties.FAVICON_FETCHER, fetcher);
    }

    /**
     * Inserts a special {@link org.chromium.ui.modelutil.MVCListAdapter.ListItem} at given index of
     * the current {@link TabListModel}.
     *
     * @param index The index of the {@link org.chromium.ui.modelutil.MVCListAdapter.ListItem} to be
     *              inserted.
     * @param uiType The view type the model will bind to.
     * @param model The model that will be bound to a view.
     */
    void addSpecialItemToModel(int index, @UiType int uiType, PropertyModel model) {
        mModel.add(index, new SimpleRecyclerViewAdapter.ListItem(uiType, model));
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
    void removeSpecialItemFromModel(
            @UiType int uiType, @MessageService.MessageType int itemIdentifier) {
        int index = TabModel.INVALID_TAB_INDEX;
        if (uiType == UiType.MESSAGE || uiType == UiType.LARGE_MESSAGE) {
            if (itemIdentifier == MessageService.MessageType.ALL) {
                while (mModel.lastIndexForMessageItem() != TabModel.INVALID_TAB_INDEX) {
                    index = mModel.lastIndexForMessageItem();
                    mModel.removeAt(index);
                }
                return;
            }
            index = mModel.lastIndexForMessageItemFromType(itemIdentifier);
        }

        if (index == TabModel.INVALID_TAB_INDEX) return;

        assert validateItemAt(index, uiType, itemIdentifier);
        mModel.removeAt(index);
    }

    private boolean validateItemAt(
            int index, @UiType int uiType, @MessageService.MessageType int itemIdentifier) {
        if (uiType == UiType.MESSAGE || uiType == UiType.LARGE_MESSAGE) {
            return mModel.get(index).type == uiType
                    && mModel.get(index).model.get(MESSAGE_TYPE) == itemIdentifier;
        }

        return false;
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
                mModel.indexOfNthTabCard(mCurrentTabModelFilterSupplier.get().index());
        int indexBelowSelectedTab = (selectedTabIndex / spanCount + 1) * spanCount;
        int indexAfterLastTab = mModel.getTabIndexBefore(mModel.size()) + 1;
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
        if (mProfile == null
                || !PriceTrackingUtilities.isPriceWelcomeMessageCardEnabled(mProfile)) {
            return;
        }
        assert mGridLayoutManager != null;
        int spanCount = mGridLayoutManager.getSpanCount();
        GridLayoutManager.SpanSizeLookup spanSizeLookup = mGridLayoutManager.getSpanSizeLookup();
        int spanSizeSumForCurrentRow = 0;
        int index = 0;
        for (; index < mModel.size(); index++) {
            spanSizeSumForCurrentRow += spanSizeLookup.getSpanSize(index);
            if (spanSizeSumForCurrentRow == spanCount) {
                // This row is compact, we clear and recount the spanSize for next row.
                spanSizeSumForCurrentRow = 0;
            } else if (spanSizeSumForCurrentRow > spanCount) {
                // Find a blank grid and break.
                if (mModel.get(index).type == TabProperties.UiType.LARGE_MESSAGE) break;
                spanSizeSumForCurrentRow = 0;
            }
        }
        if (spanSizeSumForCurrentRow <= spanCount) return;
        int blankSize = spanCount - (spanSizeSumForCurrentRow - spanSizeLookup.getSpanSize(index));
        for (int i = index + 1; i < mModel.size(); i++) {
            if (spanSizeLookup.getSpanSize(i) > blankSize) continue;
            mModel.move(i, index);
            // We should return after one move because once item moved, updateLayout() will be
            // called again.
            return;
        }
    }

    View.AccessibilityDelegate getAccessibilityDelegateForTesting() {
        return mAccessibilityDelegate;
    }

    @VisibleForTesting
    void recordPriceAnnotationsEnabledMetrics() {
        if (mMode != TabListMode.GRID
                || !mActionsOnAllRelatedTabs
                || mProfile == null
                || !PriceTrackingFeatures.isPriceTrackingEligible(mProfile)) {
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
                    PriceTrackingUtilities.isTrackPricesOnTabsEnabled(mProfile));
            preferencesManager.writeLong(
                    ChromePreferenceKeys.PRICE_TRACKING_ANNOTATIONS_ENABLED_METRICS_TIMESTAMP,
                    System.currentTimeMillis());
        }
    }

    /**
     * @param tab the {@link Tab} to find the group index of.
     * @return the index for the tab group within {@link mModel}
     */
    int getIndexForTabWithRelatedTabs(Tab tab) {
        return getIndexForTabIdWithRelatedTabs(tab.getId());
    }

    /**
     * @param tab the {@link Tab} to find the group index of.
     * @return the index for the tab group within {@link mModel}
     */
    int getIndexForTabIdWithRelatedTabs(int tabId) {
        List<Integer> relatedTabIds = getRelatedTabsIds(tabId);
        if (!relatedTabIds.isEmpty()) {
            for (int i = 0; i < mModel.size(); i++) {
                PropertyModel model = mModel.get(i).model;
                if (model.get(CARD_TYPE) != TAB) continue;

                int modelTabId = model.get(TAB_ID);
                if (relatedTabIds.contains(modelTabId)) {
                    return i;
                }
            }
        }
        return TabModel.INVALID_TAB_INDEX;
    }

    /**
     * Returns the index in {@link mModel} of the group with {@code rootId} and the {@link Tab}
     * representing the group. Will be null if the entry is not present, the tab cannot be found, or
     * the tab is not part of a tab group.
     */
    private @Nullable Pair<Integer, Tab> getIndexAndTabForRootId(int rootId) {
        int index = getIndexForTabIdWithRelatedTabs(rootId);
        if (index == TabModel.INVALID_TAB_INDEX) return null;

        Tab tab = getTabForIndex(index);
        if (tab == null || !mCurrentTabModelFilterSupplier.get().isTabInTabGroup(tab)) {
            return null;
        }
        return Pair.create(index, tab);
    }

    private @Nullable Tab getTabForIndex(int index) {
        return TabModelUtils.getTabById(
                mCurrentTabModelFilterSupplier.get().getTabModel(),
                mModel.get(index).model.get(TabProperties.TAB_ID));
    }

    Tab getTabToAddDelayedForTesting() {
        return mTabToAddDelayed;
    }

    void setComponentNameForTesting(String name) {
        var oldValue = mComponentName;
        mComponentName = name;
        ResettersForTesting.register(() -> mComponentName = oldValue);
    }

    private void onTabModelFilterChanged(
            @Nullable TabModelFilter newFilter, @Nullable TabModelFilter oldFilter) {
        removeObservers(oldFilter);

        // The observers will be bound to the newFilter's when the model is reset for with tabs for
        // that filter for the first time. Doing this on the first reset after changing models
        // makes sense as otherwise we will be observing updates when the mModel contains tabs for
        // the oldFilter which can result in invalid updates.
    }

    private void addObservers(TabModelFilter filter, @NonNull List<Tab> tabs) {
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
        if (filter instanceof TabGroupModelFilter groupFilter) {
            groupFilter.addTabGroupObserver(mTabGroupObserver);
        }
    }

    private void removeObservers(@Nullable TabModelFilter filter) {
        if (filter == null) return;

        TabModel tabModel = filter.getTabModel();
        if (tabModel != null) {
            // Observers are added when tabs are shown via addTabInfoToModel(). When switching
            // filters the TabObservers should be removed from all the tabs in the previous model.
            // If no observer was added this will no-op. Previously this was only done in
            // destroy(), but that left observers behind on the inactive model.
            for (int i = 0; i < tabModel.getCount(); i++) {
                tabModel.getTabAt(i).removeObserver(mTabObserver);
            }
        }
        filter.removeObserver(mTabModelObserver);
        if (filter instanceof TabGroupModelFilter groupFilter) {
            groupFilter.removeTabGroupObserver(mTabGroupObserver);
        }
    }

    /**
     * @param itemIdentifier The itemIdentifier to match.
     * @return whether a special {@link org.chromium.ui.modelutil.MVCListAdapter.ListItem} with the
     *     given {@code itemIdentifier} for its {@link PropertyModel} exists in the current {@link
     *     TabListModel}.
     */
    boolean specialItemExistsInModel(@MessageService.MessageType int itemIdentifier) {
        if (itemIdentifier == MessageService.MessageType.ALL) {
            return mModel.lastIndexForMessageItem() != TabModel.INVALID_TAB_INDEX;
        }
        return mModel.lastIndexForMessageItemFromType(itemIdentifier) != TabModel.INVALID_TAB_INDEX;
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
            @NonNull Runnable onAnimationEnd,
            @NonNull List<Tab> tabs,
            @NonNull TabListRecyclerView recyclerView) {
        recyclerView.setBlockTouchInput(true);
        Drawable originalForeground = recyclerView.getForeground();

        // Prepare the tabs that will be hidden by the animation.
        TreeMap<Integer, List<Integer>> bottomValuesToTabIndexes = new TreeMap<>();
        getOrderOfTabsForQuickDeleteAnimation(recyclerView, tabs, bottomValuesToTabIndexes);

        setQuickDeleteAnimationStatusForTabIndexes(
                bottomValuesToTabIndexes.values().stream()
                        .flatMap(Collection::stream)
                        .collect(Collectors.toList()),
                QuickDeleteAnimationStatus.TAB_PREPARE);

        // Create the gradient drawable and prepare the animator.
        int tabGridHeight = recyclerView.getHeight();
        int intersectionHeight =
                QuickDeleteAnimationGradientDrawable.getAnimationsIntersectionHeight(tabGridHeight);
        QuickDeleteAnimationGradientDrawable gradientDrawable =
                QuickDeleteAnimationGradientDrawable.createQuickDeleteWipeAnimationDrawable(
                        mContext, tabGridHeight);

        ObjectAnimator wipeAnimation = gradientDrawable.createWipeAnimator(tabGridHeight);

        wipeAnimation.addUpdateListener(
                valueAnimator -> {
                    if (bottomValuesToTabIndexes.isEmpty()) return;

                    float value = (float) valueAnimator.getAnimatedValue();
                    int bottomVal = bottomValuesToTabIndexes.lastKey();
                    if (bottomVal >= Math.round(value) + intersectionHeight) {
                        setQuickDeleteAnimationStatusForTabIndexes(
                                bottomValuesToTabIndexes.get(bottomVal),
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
            int index = mModel.indexFromId(id);
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
        TabModelFilter filter = mCurrentTabModelFilterSupplier.get();
        assert filter != null;

        Set<Tab> unfilteredTabs = new HashSet<>(tabs);
        Set<Tab> filteredTabs = new HashSet<>();
        Set<Integer> checkedRootIds = new HashSet<>();

        for (Tab tab : unfilteredTabs) {
            if (!filter.isTabInTabGroup(tab)) {
                filteredTabs.add(tab);
                continue;
            }

            if (checkedRootIds.contains(tab.getRootId())) continue;
            checkedRootIds.add(tab.getRootId());

            List<Tab> relatedTabs = filter.getRelatedTabList(tab.getId());
            if (unfilteredTabs.containsAll(relatedTabs)) {
                int groupIndex = filter.indexOf(tab);
                Tab groupTab = filter.getTabAt(groupIndex);
                filteredTabs.add(groupTab);
            }
        }

        return filteredTabs;
    }

    private void setQuickDeleteAnimationStatusForTabIndexes(
            List<Integer> indexes, @QuickDeleteAnimationStatus int animationStatus) {
        for (int index : indexes) {
            mModel.get(index)
                    .model
                    .set(TabProperties.QUICK_DELETE_ANIMATION_STATUS, animationStatus);
        }
    }

    private void onMenuItemClicked(@IdRes int menuId, int tabId) {
        if (menuId == R.id.close_tab) {
            RecordUserAction.record("TabGroupItemMenu.Close");
            closeTabGroup(tabId, /* hideTabGroups= */ true);
        } else if (menuId == R.id.edit_group_name) {
            RecordUserAction.record("TabGroupItemMenu.Rename");
            renameTabGroup(tabId);
        } else if (menuId == R.id.ungroup_tab) {
            RecordUserAction.record("TabGroupItemMenu.Ungroup");
            ungroupTabGroup(tabId);
        } else if (menuId == R.id.delete_tab) {
            RecordUserAction.record("TabGroupItemMenu.Delete");
            closeTabGroup(tabId, /* hideTabGroups= */ false);
        }
    }

    private void closeTabGroup(int tabId, boolean hideTabGroups) {
        TabGroupModelFilter filter = (TabGroupModelFilter) mCurrentTabModelFilterSupplier.get();
        TabModel tabModel = filter.getTabModel();
        int rootId = TabModelUtils.getTabById(tabModel, tabId).getRootId();
        List<Tab> tabs = filter.getRelatedTabListForRootId(rootId);
        boolean isIncognito = filter.getTabModel().isIncognito();

        if (hideTabGroups || isIncognito) {
            filter.closeMultipleTabs(tabs, /* canUndo= */ true, hideTabGroups);
        } else {
            List<Integer> tabIds = tabs.stream().map(Tab::getId).collect(Collectors.toList());

            // Present a confirmation dialog to the user before closing the tab group.
            Callback<Integer> onResult =
                    (@ConfirmationResult Integer result) -> {
                        if (result != ConfirmationResult.CONFIRMATION_NEGATIVE) {
                            boolean canUndo = result == ConfirmationResult.IMMEDIATE_CONTINUE;
                            List<Tab> tabsToClose =
                                    tabIds.stream()
                                            .map(filter.getTabModel()::getTabById)
                                            .filter(Objects::nonNull)
                                            .filter(tab -> !tab.isClosing())
                                            .collect(Collectors.toList());
                            filter.closeMultipleTabs(tabsToClose, canUndo, hideTabGroups);
                        }
                    };
            mActionConfirmationManager.processDeleteGroupAttempt(onResult);
        }
    }

    private void renameTabGroup(int tabId) {
        TabModel tabModel = mCurrentTabModelFilterSupplier.get().getTabModel();
        int rootId = TabModelUtils.getTabById(tabModel, tabId).getRootId();
        TabGroupModelFilter filter = (TabGroupModelFilter) mCurrentTabModelFilterSupplier.get();

        var tabGroupVisualDataDialogManager =
                new TabGroupVisualDataDialogManager(
                        mContext,
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

                        mModalDialogManager.dismissDialog(model, cause);
                    }

                    @Override
                    public void onDismiss(PropertyModel model, int dismissalCause) {
                        if (dismissalCause == DialogDismissalCause.POSITIVE_BUTTON_CLICKED) {
                            @TabGroupColorId
                            int oldColorId = filter.getTabGroupColorWithFallback(rootId);
                            @TabGroupColorId
                            int currentColorId =
                                    tabGroupVisualDataDialogManager.getCurrentColorId();
                            boolean didChangeColor = oldColorId != currentColorId;
                            if (didChangeColor) {
                                filter.setTabGroupColor(rootId, currentColorId);
                                RecordUserAction.record("TabGroup.RenameDialog.ColorChanged");
                            }

                            String defaultGroupTitle =
                                    tabGroupVisualDataDialogManager.getDefaultGroupTitle();
                            String inputGroupTitle =
                                    tabGroupVisualDataDialogManager.getCurrentGroupTitle();
                            boolean didChangeTitle =
                                    !Objects.equals(defaultGroupTitle, inputGroupTitle);
                            // This check must be included in case the user has a null title
                            // which is displayed as a tab count and chooses not to change it.
                            if (didChangeTitle) {
                                filter.setTabGroupTitle(rootId, inputGroupTitle);
                                RecordUserAction.record("TabGroup.RenameDialog.TitleChanged");
                            }
                        }

                        tabGroupVisualDataDialogManager.hideDialog();
                    }
                };

        tabGroupVisualDataDialogManager.showDialog(rootId, filter, dialogController);
    }

    private void ungroupTabGroup(int tabId) {
        TabModel tabModel = mCurrentTabModelFilterSupplier.get().getTabModel();
        int rootId = TabModelUtils.getTabById(tabModel, tabId).getRootId();
        TabGroupModelFilter filter = (TabGroupModelFilter) mCurrentTabModelFilterSupplier.get();
        List<Tab> tabs = filter.getRelatedTabListForRootId(rootId);
        boolean isIncognito = filter.getTabModel().isIncognito();

        if (isIncognito) {
            for (Tab tab : tabs) {
                filter.moveTabOutOfGroup(tab.getId());
            }
        } else {
            // Present a confirmation dialog to the user before ungrouping the tab group.
            Callback<Integer> onResult =
                    (@ConfirmationResult Integer result) -> {
                        if (result != ConfirmationResult.CONFIRMATION_NEGATIVE) {
                            for (Tab tab : tabs) {
                                filter.moveTabOutOfGroup(tab.getId());
                            }
                        }
                    };

            mActionConfirmationManager.processUngroupAttempt(onResult);
        }
    }

    private String getActionButtonDescriptionString(
            int numOfRelatedTabs, String title, int rootId) {
        Resources res = mContext.getResources();
        if (!ChromeFeatureList.sTabGroupParityAndroid.isEnabled()) {
            if (title.isEmpty()) {
                return res.getQuantityString(
                        R.plurals.accessibility_close_tab_group_button,
                        numOfRelatedTabs,
                        numOfRelatedTabs);
            } else {
                return res.getQuantityString(
                        R.plurals.accessibility_close_tab_group_button_with_group_name,
                        numOfRelatedTabs,
                        title,
                        numOfRelatedTabs);
            }
        } else {
            if (ChromeFeatureList.sTabGroupPaneAndroid.isEnabled()) {
                String descriptionTitle = title;
                if (descriptionTitle.isEmpty()) {
                    descriptionTitle =
                            TabGroupTitleEditor.getDefaultTitle(mContext, numOfRelatedTabs);
                }
                return res.getString(
                        R.string.accessibility_open_tab_group_overflow_menu_with_group_name,
                        descriptionTitle);
            } else {
                TabGroupModelFilter filter =
                        (TabGroupModelFilter) mCurrentTabModelFilterSupplier.get();
                @TabGroupColorId int colorId = filter.getTabGroupColorWithFallback(rootId);
                final @StringRes int colorDescRes =
                        ColorPickerUtils.getTabGroupColorPickerItemColorAccessibilityString(
                                colorId);
                String colorDesc = res.getString(colorDescRes);
                if (title.isEmpty()) {
                    return res.getQuantityString(
                            R.plurals.accessibility_close_tab_group_button_with_color,
                            numOfRelatedTabs,
                            numOfRelatedTabs,
                            colorDesc);
                } else {
                    return res.getQuantityString(
                            R.plurals
                                    .accessibility_close_tab_group_button_with_group_name_with_color,
                            numOfRelatedTabs,
                            title,
                            numOfRelatedTabs,
                            colorDesc);
                }
            }
        }
    }

    private PropertyModel getModelFromId(int tabId) {
        int modelIndex = mModel.indexFromId(tabId);
        if (modelIndex == TabModel.INVALID_TAB_INDEX) return null;
        return mModel.get(modelIndex).model;
    }

    private boolean isParentComponentTabSwitcher() {
        return TabSwitcherCoordinator.COMPONENT_NAME.equals(mComponentName);
    }

    @TabListMode
    int getTabListModeForTesting() {
        return mMode;
    }
}

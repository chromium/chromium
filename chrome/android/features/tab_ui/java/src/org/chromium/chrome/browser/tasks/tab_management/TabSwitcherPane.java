// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;
import android.content.SharedPreferences;
import android.content.SharedPreferences.OnSharedPreferenceChangeListener;
import android.os.Build;
import android.view.View;
import android.view.View.OnClickListener;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.PluralsRes;
import androidx.core.util.Pair;

import org.chromium.base.Callback;
import org.chromium.base.Token;
import org.chromium.base.ValueChangedCallback;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.base.supplier.Supplier;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.chrome.browser.compositor.CompositorViewHolder;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.hub.DelegateButtonData;
import org.chromium.chrome.browser.hub.HubColorScheme;
import org.chromium.chrome.browser.hub.Pane;
import org.chromium.chrome.browser.hub.PaneHubController;
import org.chromium.chrome.browser.hub.PaneId;
import org.chromium.chrome.browser.hub.ResourceButtonData;
import org.chromium.chrome.browser.hub.TabSwitcherDrawableButtonData;
import org.chromium.chrome.browser.price_tracking.PriceTrackingFeatures;
import org.chromium.chrome.browser.price_tracking.PriceTrackingUtilities;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileProvider;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tab.state.ShoppingPersistedTabData;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncServiceFactory;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilterObserver;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilterObserver.DidRemoveTabGroupReason;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.browser.tasks.tab_management.TabListCoordinator.TabListMode;
import org.chromium.chrome.browser.tasks.tab_management.archived_tabs_auto_delete_promo.ArchivedTabsAutoDeletePromoManager;
import org.chromium.chrome.browser.toolbar.TabSwitcherDrawable;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeController;
import org.chromium.chrome.browser.user_education.IphCommand;
import org.chromium.chrome.browser.user_education.IphCommandBuilder;
import org.chromium.chrome.browser.user_education.UserEducationHelper;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.sensitive_content.SensitiveContentFeatures;
import org.chromium.components.tab_group_sync.TabGroupSyncService;

import java.util.List;
import java.util.function.DoubleConsumer;

/** A {@link Pane} representing the regular tab switcher. */
public class TabSwitcherPane extends TabSwitcherPaneBase implements TabSwitcherDrawable.Observer {
    private static final int ON_CREATION_IPH_DELAY = 100;

    private final TabModelObserver mTabModelObserver =
            new TabModelObserver() {
                @Override
                public void didSelectTab(Tab tab, @TabSelectionType int type, int lastId) {
                    onTabSelected(tab);
                }
            };

    private final TabGroupModelFilterObserver mFilterObserver =
            new TabGroupModelFilterObserver() {
                @Override
                public void didRemoveTabGroup(
                        int oldRootId,
                        @Nullable Token oldTabGroupId,
                        @DidRemoveTabGroupReason int removalReason) {
                    onDidRemoveTabGroup(oldTabGroupId, removalReason);
                }

                @Override
                public void didCreateNewGroup(Tab destinationTab, TabGroupModelFilter filter) {
                    // Unfortunately it's difficult to wait for a recycler view to finish  binding
                    // and fully showing views. So just wait with a short delay.
                    PostTask.postDelayedTask(
                            TaskTraits.UI_DEFAULT,
                            () -> tryToTriggerRemoteGroupIph(),
                            ON_CREATION_IPH_DELAY);
                }
            };

    private final Callback<Boolean> mScrollingObserver = this::onScrollingChanged;
    private final Callback<Boolean> mVisibilityObserver = this::onVisibilityChanged;
    private final @NonNull SharedPreferences mSharedPreferences;
    private final @NonNull Supplier<TabGroupModelFilter> mTabGroupModelFilterSupplier;
    private final @NonNull TabSwitcherPaneDrawableCoordinator mTabSwitcherPaneDrawableCoordinator;
    private final ObservableSupplierImpl<Boolean> mHubSearchEnabledStateSupplier =
            new ObservableSupplierImpl<>();
    private @Nullable OnSharedPreferenceChangeListener mPriceAnnotationsPrefListener;
    private @Nullable TabGroupSyncService mTabGroupSyncService;
    private final TabSwitcherDrawable mTabSwitcherDrawable;
    private final @Nullable ArchivedTabsAutoDeletePromoManager mArchivedTabsAutoDeletePromoManager;

    /**
     * @param context The activity context.
     * @param sharedPreferences The app shared preferences.
     * @param profileProviderSupplier The profile provider supplier.
     * @param factory The factory used to construct {@link TabSwitcherPaneCoordinator}s.
     * @param tabGroupModelFilterSupplier The supplier of the regular {@link TabGroupModelFilter}.
     * @param newTabButtonClickListener The {@link OnClickListener} for the new tab button.
     * @param tabSwitcherDrawableCoordinator The drawable to represent the pane.
     * @param onToolbarAlphaChange Observer to notify when alpha changes during animations.
     * @param userEducationHelper Used for showing IPHs.
     * @param edgeToEdgeSupplier Supplier to the {@link EdgeToEdgeController} instance.
     * @param compositorViewHolderSupplier Supplier to the {@link CompositorViewHolder} instance.
     * @param tabGroupCreationUiDelegate Orchestrates the tab group creation UI flow.
     * @param archivedTabsAutoDeletePromoManager Manager for Archived Tabs Auto Delete Promo.
     */
    TabSwitcherPane(
            @NonNull Context context,
            @NonNull SharedPreferences sharedPreferences,
            @NonNull OneshotSupplier<ProfileProvider> profileProviderSupplier,
            @NonNull TabSwitcherPaneCoordinatorFactory factory,
            @NonNull Supplier<TabGroupModelFilter> tabGroupModelFilterSupplier,
            @NonNull OnClickListener newTabButtonClickListener,
            @NonNull TabSwitcherPaneDrawableCoordinator tabSwitcherDrawableCoordinator,
            @NonNull DoubleConsumer onToolbarAlphaChange,
            @NonNull UserEducationHelper userEducationHelper,
            @NonNull ObservableSupplier<EdgeToEdgeController> edgeToEdgeSupplier,
            @NonNull ObservableSupplier<CompositorViewHolder> compositorViewHolderSupplier,
            @NonNull TabGroupCreationUiDelegate tabGroupCreationUiDelegate,
            @Nullable ArchivedTabsAutoDeletePromoManager archivedTabsAutoDeletePromoManager) {
        super(
                context,
                factory,
                /* isIncognito= */ false,
                onToolbarAlphaChange,
                userEducationHelper,
                edgeToEdgeSupplier,
                compositorViewHolderSupplier,
                tabGroupCreationUiDelegate);
        mSharedPreferences = sharedPreferences;
        mTabGroupModelFilterSupplier = tabGroupModelFilterSupplier;
        mTabSwitcherPaneDrawableCoordinator = tabSwitcherDrawableCoordinator;

        mTabSwitcherDrawable = tabSwitcherDrawableCoordinator.getTabSwitcherDrawable();
        mTabSwitcherDrawable.addTabSwitcherDrawableObserver(this);
        mArchivedTabsAutoDeletePromoManager = archivedTabsAutoDeletePromoManager;
        // Set the TabSwitcherDrawable state on an initial run through.
        onDrawableStateChanged();

        mNewTabButtonDataSupplier.set(
                new DelegateButtonData(
                        new ResourceButtonData(
                                R.string.button_new_tab,
                                R.string.button_new_tab,
                                R.drawable.new_tab_icon),
                        () -> {
                            newTabButtonClickListener.onClick(null);
                        }));

        profileProviderSupplier.onAvailable(this::onProfileProviderAvailable);
        getIsVisibleSupplier().addObserver(mVisibilityObserver);
        getTabSwitcherPaneCoordinatorSupplier()
                .addObserver(new ValueChangedCallback<>(this::onTabSwitcherPaneCoordinatorChanged));
    }

    @Override
    public @PaneId int getPaneId() {
        return PaneId.TAB_SWITCHER;
    }

    @Override
    public @HubColorScheme int getColorScheme() {
        return HubColorScheme.DEFAULT;
    }

    @Override
    public void destroy() {
        // Do this before super.destroy() since the visibility supplier is owned by the base class.
        getIsVisibleSupplier().removeObserver(mVisibilityObserver);
        super.destroy();
        mTabSwitcherPaneDrawableCoordinator.destroy();
        if (mPriceAnnotationsPrefListener != null) {
            mSharedPreferences.unregisterOnSharedPreferenceChangeListener(
                    mPriceAnnotationsPrefListener);
        }
        removeObservers();
        mTabSwitcherDrawable.removeTabSwitcherDrawableObserver(this);
    }

    @Override
    public void showAllTabs() {
        resetWithListOfTabs(mTabGroupModelFilterSupplier.get().getRepresentativeTabList());
    }

    @Override
    public @Nullable Tab getCurrentTab() {
        return TabModelUtils.getCurrentTab(mTabGroupModelFilterSupplier.get().getTabModel());
    }

    @Override
    public boolean shouldEagerlyCreateCoordinator() {
        return true;
    }

    @Override
    public void resetWithListOfTabs(@Nullable List<Tab> tabs) {
        @Nullable TabSwitcherPaneCoordinator coordinator = getTabSwitcherPaneCoordinator();
        if (coordinator == null) {
            return;
        }

        @Nullable TabGroupModelFilter filter = mTabGroupModelFilterSupplier.get();
        if (filter == null || !filter.isTabModelRestored()) {
            // The tab list is trying to show without the filter being ready. This happens when
            // first trying to show a the pane. If this happens an attempt to show will be made
            // when the filter's restoreCompleted() method is invoked in TabSwitcherPaneMediator.
            // Start a timer to measure how long it takes for tab state to be initialized and for
            // this UI to show i.e. isTabModelRestored becomes true. This timer will emit a
            // histogram when we successfully show. This timer is cancelled if: 1) the pane becomes
            // invisible in TabSwitcherPaneBase#notifyLoadHint, or 2) the filter becomes ready and
            // nothing gets shown.
            startWaitForTabStateInitializedTimer();
            return;
        }

        boolean isNotVisibleOrSelected =
                !getIsVisibleSupplier().get() || !filter.getTabModel().isActiveModel();

        if (isNotVisibleOrSelected) {
            cancelWaitForTabStateInitializedTimer();
            coordinator.resetWithListOfTabs(null);
        } else {
            // TODO(crbug.com/373850469): Add unit tests when robolectric supports Android V.
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.VANILLA_ICE_CREAM
                    && ChromeFeatureList.isEnabled(SensitiveContentFeatures.SENSITIVE_CONTENT)
                    && ChromeFeatureList.isEnabled(
                            SensitiveContentFeatures.SENSITIVE_CONTENT_WHILE_SWITCHING_TABS)) {
                TabUiUtils.updateViewContentSensitivityForTabs(
                        filter.getTabModel(),
                        coordinator::setTabSwitcherContentSensitivity,
                        "SensitiveContent.TabSwitching.RegularTabSwitcherPane.Sensitivity");
            }

            finishWaitForTabStateInitializedTimer();
            coordinator.resetWithListOfTabs(tabs);
        }
    }

    @Override
    protected Runnable getOnTabGroupCreationRunnable() {
        return this::tryToTriggerTabGroupSurfaceIph;
    }

    @Override
    protected void tryToTriggerOnShownIphs() {
        // The IPH system will ensure we don't show all three.
        tryToTriggerTabGroupSurfaceIph();
        tryToTriggerRemoteGroupIph();
        // Attempts to show the Auto Delete Decision Promo
        if (mArchivedTabsAutoDeletePromoManager != null) {
            mArchivedTabsAutoDeletePromoManager.tryToShowArchivedTabsAutoDeleteDecisionPromo();
        }
    }

    private void onTabSwitcherPaneCoordinatorChanged(
            @Nullable TabSwitcherPaneCoordinator newValue,
            @Nullable TabSwitcherPaneCoordinator oldValue) {
        if (oldValue != null) {
            OneshotSupplier<ObservableSupplier<Boolean>> wrappedSupplier =
                    oldValue.getIsScrollingSupplier();
            if (wrappedSupplier.hasValue()) {
                wrappedSupplier.get().removeObserver(mScrollingObserver);
            }
        }
        if (newValue != null) {
            OneshotSupplier<ObservableSupplier<Boolean>> wrappedSupplier =
                    newValue.getIsScrollingSupplier();
            wrappedSupplier.onAvailable(
                    supplier -> {
                        supplier.addObserver(mScrollingObserver);
                    });
        }
    }

    private void onProfileProviderAvailable(@NonNull ProfileProvider profileProvider) {
        Profile profile = profileProvider.getOriginalProfile();
        mTabGroupSyncService = TabGroupSyncServiceFactory.getForProfile(profile);

        mTracker = TrackerFactory.getTrackerForProfile(profileProvider.getOriginalProfile());

        if (!PriceTrackingFeatures.isPriceAnnotationsEnabled(profileProvider.getOriginalProfile())
                && getTabListMode() == TabListMode.GRID) {
            return;
        }
        mPriceAnnotationsPrefListener =
                (sharedPrefs, key) -> {
                    if (!PriceTrackingUtilities.TRACK_PRICES_ON_TABS.equals(key)
                            || !getIsVisibleSupplier().get()) {
                        return;
                    }
                    TabGroupModelFilter filter = mTabGroupModelFilterSupplier.get();
                    @Nullable
                    TabSwitcherPaneCoordinator coordinator = getTabSwitcherPaneCoordinator();
                    if (filter.getTabModel().isActiveModel()
                            && filter.isTabModelRestored()
                            && coordinator != null) {
                        coordinator.resetWithListOfTabs(filter.getRepresentativeTabList());
                    }
                };
        mSharedPreferences.registerOnSharedPreferenceChangeListener(mPriceAnnotationsPrefListener);
    }

    private void onVisibilityChanged(boolean visible) {
        TabGroupModelFilter filter = mTabGroupModelFilterSupplier.get();
        if (filter == null) return;

        if (visible) {
            filter.getTabModel().addObserver(mTabModelObserver);
            filter.addTabGroupObserver(mFilterObserver);
        } else {
            removeObservers();
        }
    }

    private void tryToTriggerTabGroupSurfaceIph() {
        // There are lot more reasons to bail out here than reasons this method may be invoked. In
        // general because of the posted delay, most dependencies should be satisfied, and these
        // checks should just add robustness. Once crbug.com/346356139 is fixed, we can revisit
        // this approach.
        @Nullable PaneHubController paneHubController = getPaneHubController();
        if (paneHubController == null) return;
        @Nullable View anchorView = paneHubController.getPaneButton(PaneId.TAB_GROUPS);
        if (anchorView == null) return;

        if (mTabGroupSyncService == null) return;
        if (mTabGroupSyncService.getAllGroupIds().length == 0) return;

        if (getIsAnimatingSupplier().get()) return;

        IphCommand command =
                new IphCommandBuilder(
                                getRootView().getResources(),
                                FeatureConstants.TAB_GROUPS_SURFACE,
                                R.string.tab_group_surface_iph_with_sync,
                                R.string.tab_group_surface_iph_with_sync)
                        .setAnchorView(anchorView)
                        .build();
        mUserEducationHelper.requestShowIph(command);
    }

    private void onScrollingChanged(boolean isScrolling) {
        // When we stop scrolling is a good time to check if the remote group iph should trigger, as
        // new tab groups cards may be fully visible one the screen where they were not previously.
        if (!isScrolling) {
            tryToTriggerRemoteGroupIph();
        }
    }

    private void tryToTriggerRemoteGroupIph() {
        if (mTabGroupSyncService == null) return;

        @Nullable PaneHubController paneHubController = getPaneHubController();
        if (paneHubController == null) return;

        TabSwitcherPaneCoordinator coordinator = getTabSwitcherPaneCoordinator();
        // Skip showing IPH if the tab grid dialog is open as it will appear atop the dialog which
        // looks incorrect.
        if (coordinator == null
                || Boolean.TRUE.equals(coordinator.getTabGridDialogVisibilitySupplier().get())) {
            return;
        }

        TabGroupModelFilter filter = mTabGroupModelFilterSupplier.get();
        @Nullable Pair<Integer, Integer> range = coordinator.getVisibleRange();
        if (range == null) return;
        // Iterate in reverse because when multiple viable groups are on screen, we want to trigger
        // on the most recently added, which should be ordered later.
        for (int viewIndex = range.second; viewIndex >= range.first; --viewIndex) {
            int filterIndex = coordinator.countOfTabCardsOrInvalid(viewIndex);
            @Nullable Tab tab = filter.getRepresentativeTabAt(filterIndex);
            if (tab == null || !filter.isTabInTabGroup(tab)) continue;

            @Nullable Token tabGroupId = tab.getTabGroupId();
            if (!TabUiUtils.shouldShowIphForSync(mTabGroupSyncService, tabGroupId)) continue;

            @Nullable View anchorView = coordinator.getViewByIndex(viewIndex);
            if (anchorView == null) continue;

            IphCommand command =
                    new IphCommandBuilder(
                                    getRootView().getResources(),
                                    FeatureConstants.TAB_GROUPS_REMOTE_GROUP,
                                    R.string.newly_synced_tab_group_iph,
                                    R.string.newly_synced_tab_group_iph)
                            .setAnchorView(anchorView)
                            .build();
            mUserEducationHelper.requestShowIph(command);
            return;
        }
    }

    private void removeObservers() {
        TabGroupModelFilter filter = mTabGroupModelFilterSupplier.get();
        if (filter != null) {
            filter.getTabModel().removeObserver(mTabModelObserver);
            filter.removeTabGroupObserver(mFilterObserver);
        }
    }

    private void onTabSelected(@Nullable Tab tab) {
        if (tab == null) return;

        Profile profile = tab.getProfile();
        if (getTabListMode() == TabListCoordinator.TabListMode.GRID
                && !profile.isOffTheRecord()
                && PriceTrackingUtilities.isTrackPricesOnTabsEnabled(profile)) {
            RecordUserAction.record(
                    "Commerce.TabGridSwitched."
                            + (ShoppingPersistedTabData.hasPriceDrop(tab)
                                    ? "HasPriceDrop"
                                    : "NoPriceDrop"));
        }
    }

    private void onDidRemoveTabGroup(
            @Nullable Token oldTabGroupId, @DidRemoveTabGroupReason int removalReason) {
        if (removalReason != DidRemoveTabGroupReason.CLOSE) return;

        TabGroupModelFilter filter = mTabGroupModelFilterSupplier.get();
        if (!filter.isTabGroupHiding(oldTabGroupId)) return;

        @Nullable PaneHubController paneHubController = getPaneHubController();
        if (paneHubController == null) return;

        @Nullable View anchorView = paneHubController.getPaneButton(PaneId.TAB_GROUPS);
        if (anchorView == null) return;

        IphCommand command =
                new IphCommandBuilder(
                                getRootView().getResources(),
                                FeatureConstants.TAB_GROUPS_SURFACE_ON_HIDE,
                                R.string.find_hidden_tab_group_iph,
                                R.string.find_hidden_tab_group_iph)
                        .setAnchorView(anchorView)
                        .build();
        mUserEducationHelper.requestShowIph(command);
    }

    // TabSwitcherDrawable.Observer implementation.

    @Override
    public void onDrawableStateChanged() {
        @PluralsRes
        int tabSwitcherButtonDescRes = getTabSwitcherDrawableDescription(mTabSwitcherDrawable);
        mReferenceButtonDataSupplier.set(
                new TabSwitcherDrawableButtonData(
                        R.string.tab_switcher_standard_stack_text,
                        tabSwitcherButtonDescRes,
                        mTabSwitcherDrawable,
                        mTabGroupModelFilterSupplier.get().getTabModel().getCount()));
    }

    private @PluralsRes int getTabSwitcherDrawableDescription(TabSwitcherDrawable drawable) {
        @PluralsRes int drawableDescRes = R.plurals.accessibility_tab_switcher_standard_stack;
        if (TabUiUtils.isDataSharingFunctionalityEnabled()
                && drawable.getShowIconNotificationStatus()) {
            drawableDescRes = R.plurals.accessibility_tab_switcher_standard_stack_with_notification;
        }
        return drawableDescRes;
    }

    @Override
    public @NonNull ObservableSupplier<Boolean> getHubSearchEnabledStateSupplier() {
        return mHubSearchEnabledStateSupplier;
    }
}

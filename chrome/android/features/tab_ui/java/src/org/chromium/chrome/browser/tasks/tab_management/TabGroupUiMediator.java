// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.chrome.browser.tasks.tab_management.TabGroupUiProperties.BACKGROUND_COLOR;
import static org.chromium.chrome.browser.tasks.tab_management.TabGroupUiProperties.IMAGE_TILES_CONTAINER_VISIBLE;
import static org.chromium.chrome.browser.tasks.tab_management.TabGroupUiProperties.INITIAL_SCROLL_INDEX;
import static org.chromium.chrome.browser.tasks.tab_management.TabGroupUiProperties.NEW_TAB_BUTTON_ON_CLICK_LISTENER;
import static org.chromium.chrome.browser.tasks.tab_management.TabGroupUiProperties.SHOW_GROUP_DIALOG_BUTTON_VISIBLE;
import static org.chromium.chrome.browser.tasks.tab_management.TabGroupUiProperties.SHOW_GROUP_DIALOG_ON_CLICK_LISTENER;
import static org.chromium.chrome.browser.tasks.tab_management.TabGroupUiProperties.TINT;

import android.content.res.ColorStateList;
import android.os.Handler;
import android.view.View;

import androidx.annotation.ColorInt;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.base.CallbackController;
import org.chromium.base.Token;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.LazyOneshotSupplier;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.collaboration.CollaborationServiceFactory;
import org.chromium.chrome.browser.data_sharing.DataSharingServiceFactory;
import org.chromium.chrome.browser.data_sharing.ui.shared_image_tiles.SharedImageTilesConfig;
import org.chromium.chrome.browser.data_sharing.ui.shared_image_tiles.SharedImageTilesCoordinator;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.layouts.LayoutStateProvider.LayoutStateObserver;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncFeatures;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncServiceFactory;
import org.chromium.chrome.browser.tab_ui.TabContentManager;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilterObserver;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorTabObserver;
import org.chromium.chrome.browser.tasks.tab_management.TabGridDialogMediator.DialogController;
import org.chromium.chrome.browser.theme.ThemeColorProvider;
import org.chromium.chrome.browser.theme.ThemeColorProvider.ThemeColorObserver;
import org.chromium.chrome.browser.theme.ThemeColorProvider.TintObserver;
import org.chromium.chrome.browser.toolbar.bottom.BottomControlsCoordinator;
import org.chromium.chrome.browser.toolbar.bottom.BottomControlsCoordinator.BottomControlsVisibilityController;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler;
import org.chromium.components.collaboration.CollaborationService;
import org.chromium.components.collaboration.ServiceStatus;
import org.chromium.components.data_sharing.DataSharingService;
import org.chromium.components.data_sharing.GroupMember;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;

import java.util.List;
import java.util.Objects;

/** A mediator for the TabGroupUi. Responsible for managing the internal state of the component. */
public class TabGroupUiMediator implements BackPressHandler {

    /** Defines an interface for a {@link TabGroupUiMediator} reset event handler. */
    interface ResetHandler {
        /**
         * Handles a reset event originated from {@link TabGroupUiMediator} when the bottom sheet is
         * collapsed or the dialog is hidden.
         *
         * @param tabs List of Tabs to reset.
         */
        void resetStripWithListOfTabs(List<Tab> tabs);

        /**
         * Handles a reset event originated from {@link TabGroupUiMediator}
         * when the bottom sheet is expanded or the dialog is shown.
         *
         * @param tabs List of Tabs to reset.
         */
        void resetGridWithListOfTabs(List<Tab> tabs);
    }

    /** Wraps a child component's token with information from this component. */
    private static class NestedSnapshot {
        private final Object mChildSnapshot;
        private final @ColorInt int mBackgroundColor;
        private final int mWidthPx;

        /* package */ NestedSnapshot(
                Object childSnapshot, @ColorInt int backgroundColor, int widthPx) {
            mChildSnapshot = childSnapshot;
            mBackgroundColor = backgroundColor;
            mWidthPx = widthPx;
        }

        @Override
        public boolean equals(@Nullable Object obj) {
            return (obj instanceof NestedSnapshot other)
                    && Objects.equals(mChildSnapshot, other.mChildSnapshot)
                    && mBackgroundColor == other.mBackgroundColor
                    && this.mWidthPx == other.mWidthPx;
        }

        @Override
        public int hashCode() {
            return Objects.hash(mChildSnapshot, mBackgroundColor, mWidthPx);
        }
    }

    private final Callback<Integer> mOnGroupSharedStateChanged = this::onGroupSharedStateChanged;
    private final Callback<List<GroupMember>> mOnGroupMembersChanged = this::onGroupMembersChanged;
    private final Callback mOnTokenComponentChange = this::onTokenComponentChange;
    private final ObservableSupplierImpl<Integer> mWidthPxSupplier =
            new ObservableSupplierImpl<>(0);
    private final ThemeColorObserver mThemeColorObserver = this::onThemeColorChanged;
    private final TintObserver mTintObserver = this::onTintChanged;
    private final PropertyModel mModel;
    private final TabModelObserver mTabModelObserver;
    private final ResetHandler mResetHandler;
    private final TabModelSelector mTabModelSelector;
    private final TabContentManager mTabContentManager;
    private final TabCreatorManager mTabCreatorManager;
    private final BottomControlsCoordinator.BottomControlsVisibilityController
            mVisibilityController;
    private final LazyOneshotSupplier<DialogController> mTabGridDialogControllerSupplier;
    private final Callback<TabModel> mCurrentTabModelObserver;
    private final ObservableSupplier<Boolean> mOmniboxFocusStateSupplier;
    private final ObservableSupplierImpl<Boolean> mHandleBackPressChangedSupplier;
    private final ThemeColorProvider mThemeColorProvider;
    private final Callback<Object> mOnSnapshotTokenChange;
    private final ObservableSupplier<Object> mChildTokenSupplier;

    // These should only be used when regular (non-incognito) tabs are set in the model.
    private final @Nullable SharedImageTilesCoordinator mSharedImageTilesCoordinator;
    private final @Nullable SharedImageTilesConfig.Builder mSharedImageTilesConfigBuilder;
    private final @Nullable TransitiveSharedGroupObserver mTransitiveSharedGroupObserver;

    private final LayoutStateObserver mLayoutStateObserver;
    private final TabGroupModelFilterObserver mTabGroupModelFilterObserver;
    private final Callback<Boolean> mOmniboxFocusObserver;

    private CallbackController mCallbackController = new CallbackController();
    private LayoutStateProvider mLayoutStateProvider;
    private TabModelSelectorTabObserver mTabModelSelectorTabObserver;
    private @Nullable Token mCurrentTabGroupId;
    private boolean mIsShowingHub;

    TabGroupUiMediator(
            BottomControlsVisibilityController visibilityController,
            ObservableSupplierImpl<Boolean> handleBackPressChangedSupplier,
            ResetHandler resetHandler,
            PropertyModel model,
            TabModelSelector tabModelSelector,
            TabContentManager tabContentManager,
            TabCreatorManager tabCreatorManager,
            OneshotSupplier<LayoutStateProvider> layoutStateProviderSupplier,
            @Nullable
                    LazyOneshotSupplier<TabGridDialogMediator.DialogController>
                            dialogControllerSupplier,
            ObservableSupplier<Boolean> omniboxFocusStateSupplier,
            @Nullable SharedImageTilesCoordinator sharedImageTilesCoordinator,
            @Nullable SharedImageTilesConfig.Builder sharedImageTilesConfigBuilder,
            ThemeColorProvider themeColorProvider,
            Callback<Object> onSnapshotTokenChange,
            ObservableSupplierImpl<Object> childTokenSupplier) {
        mResetHandler = resetHandler;
        mModel = model;
        mTabModelSelector = tabModelSelector;
        mTabContentManager = tabContentManager;
        mTabCreatorManager = tabCreatorManager;
        mVisibilityController = visibilityController;
        mTabGridDialogControllerSupplier = dialogControllerSupplier;
        mOmniboxFocusStateSupplier = omniboxFocusStateSupplier;
        mSharedImageTilesCoordinator = sharedImageTilesCoordinator;
        mSharedImageTilesConfigBuilder = sharedImageTilesConfigBuilder;
        mThemeColorProvider = themeColorProvider;

        mThemeColorProvider.addThemeColorObserver(mThemeColorObserver);
        mThemeColorProvider.addTintObserver(mTintObserver);
        mOnSnapshotTokenChange = onSnapshotTokenChange;
        mChildTokenSupplier = childTokenSupplier;
        mChildTokenSupplier.addObserver(mOnTokenComponentChange);
        mWidthPxSupplier.addObserver(mOnTokenComponentChange);

        onThemeColorChanged(mThemeColorProvider.getThemeColor(), false);
        onTintChanged(
                mThemeColorProvider.getTint(),
                mThemeColorProvider.getTint(),
                BrandedColorScheme.APP_DEFAULT);
        Profile originalProfile = mTabModelSelector.getModel(/* incognito= */ false).getProfile();
        CollaborationService collaborationService =
                CollaborationServiceFactory.getForProfile(originalProfile);
        @NonNull ServiceStatus serviceStatus = collaborationService.getServiceStatus();
        if (TabGroupSyncFeatures.isTabGroupSyncEnabled(originalProfile)
                && serviceStatus.isAllowedToJoin()) {
            TabGroupSyncService tabGroupSyncService =
                    TabGroupSyncServiceFactory.getForProfile(originalProfile);
            DataSharingService dataSharingService =
                    DataSharingServiceFactory.getForProfile(originalProfile);
            mTransitiveSharedGroupObserver =
                    new TransitiveSharedGroupObserver(
                            tabGroupSyncService, dataSharingService, collaborationService);
            mTransitiveSharedGroupObserver
                    .getGroupSharedStateSupplier()
                    .addObserver(mOnGroupSharedStateChanged);
            mTransitiveSharedGroupObserver
                    .getGroupMembersSupplier()
                    .addObserver(mOnGroupMembersChanged);
        } else {
            mTransitiveSharedGroupObserver = null;
        }

        var layoutStateProvider = layoutStateProviderSupplier.get();
        if (layoutStateProvider != null
                && layoutStateProvider.isLayoutVisible(LayoutType.TAB_SWITCHER)) {
            mIsShowingHub = true;
        }

        // Register for tab model.
        mTabModelObserver =
                new TabModelObserver() {
                    @Override
                    public void didSelectTab(Tab tab, @TabSelectionType int type, int lastId) {
                        resetTabStrip();
                    }

                    @Override
                    public void didAddTab(
                            Tab tab,
                            int type,
                            @TabCreationState int creationState,
                            boolean markedForSelection) {
                        resetTabStrip();
                        if (mCurrentTabGroupId != null
                                && Objects.equals(tab.getTabGroupId(), mCurrentTabGroupId)
                                && type == TabLaunchType.FROM_TAB_GROUP_UI) {
                            postUpdateInitialScrollIndex(
                                    () -> Math.max(0, getTabsToShowForId(tab.getId()).size() - 1));
                        }
                    }

                    @Override
                    public void restoreCompleted() {
                        resetTabStrip();
                    }

                    @Override
                    public void tabClosureUndone(Tab tab) {
                        resetTabStrip();
                    }
                };
        mLayoutStateObserver =
                new LayoutStateProvider.LayoutStateObserver() {
                    @Override
                    public void onStartedShowing(@LayoutType int layoutType) {
                        if (layoutType == LayoutType.TAB_SWITCHER) {
                            mIsShowingHub = true;
                            hideTabGridDialog();
                            resetTabStrip();
                        }
                    }

                    @Override
                    public void onFinishedHiding(@LayoutType int layoutType) {
                        if (layoutType == LayoutType.TAB_SWITCHER) {
                            mIsShowingHub = false;
                            resetTabStrip();
                        }
                    }
                };

        mTabModelSelectorTabObserver =
                new TabModelSelectorTabObserver(mTabModelSelector) {
                    @Override
                    public void onPageLoadStarted(Tab tab, GURL url) {
                        // TODO(crbug.com/40695094) This is a band-aid fix for M84. The root cause
                        // is probably a leaked observer. Remove this when the TabObservers are
                        // removed during tab reparenting.
                        if (mTabModelSelector.getTabById(tab.getId()) == null) return;

                        int numTabs = 0;
                        TabGroupModelFilter filter = getCurrentTabGroupModelFilter();
                        if (mCurrentTabGroupId != null && filter.isTabInTabGroup(tab)) {
                            numTabs = filter.getTabCountForGroup(tab.getTabGroupId());
                        }

                        RecordHistogram.recordCount1MHistogram(
                                "TabStrip.TabCountOnPageLoad", numTabs);
                    }

                    @Override
                    public void onActivityAttachmentChanged(Tab tab, WindowAndroid window) {
                        // Remove this when tab is detached since the TabModelSelectorTabObserver is
                        // not properly destroyed when there is a normal/night mode switch.
                        if (window == null) {
                            this.destroy();
                            mTabModelSelectorTabObserver = null;
                        }
                    }
                };

        mCurrentTabModelObserver = (tabModel) -> resetTabStrip();

        mTabGroupModelFilterObserver =
                new TabGroupModelFilterObserver() {
                    @Override
                    public void didMoveTabOutOfGroup(Tab movedTab, int prevFilterIndex) {
                        resetTabStrip();
                    }

                    @Override
                    public void didMergeTabToGroup(Tab movedTab) {
                        resetTabStrip();
                    }
                };

        var filterProvider = mTabModelSelector.getTabGroupModelFilterProvider();
        filterProvider
                .getTabGroupModelFilter(false)
                .addTabGroupObserver(mTabGroupModelFilterObserver);
        filterProvider
                .getTabGroupModelFilter(true)
                .addTabGroupObserver(mTabGroupModelFilterObserver);

        mOmniboxFocusObserver = isFocus -> resetTabStrip();
        mOmniboxFocusStateSupplier.addObserver(mOmniboxFocusObserver);

        filterProvider.addTabGroupModelFilterObserver(mTabModelObserver);
        mTabModelSelector.getCurrentTabModelSupplier().addObserver(mCurrentTabModelObserver);

        if (layoutStateProvider != null) {
            setLayoutStateProvider(layoutStateProvider);
        } else {
            layoutStateProviderSupplier.onAvailable(
                    mCallbackController.makeCancelable(this::setLayoutStateProvider));
        }

        setupToolbarButtons();
        mModel.set(TabGroupUiProperties.SHOW_GROUP_DIALOG_BUTTON_VISIBLE, true);
        mModel.set(TabGroupUiProperties.IS_MAIN_CONTENT_VISIBLE, true);
        mModel.set(TabGroupUiProperties.WIDTH_PX_CALLBACK, mWidthPxSupplier::set);
        resetTabStrip();

        mHandleBackPressChangedSupplier = handleBackPressChangedSupplier;
        if (mTabGridDialogControllerSupplier != null) {
            mTabGridDialogControllerSupplier.onAvailable(
                    controller -> {
                        controller
                                .getHandleBackPressChangedSupplier()
                                .addObserver(mHandleBackPressChangedSupplier::set);
                    });
        }
    }

    private void setLayoutStateProvider(LayoutStateProvider layoutStateProvider) {
        mLayoutStateProvider = layoutStateProvider;
        layoutStateProvider.addObserver(mLayoutStateObserver);
    }

    private void onThemeColorChanged(@ColorInt int color, boolean shouldAnimate) {
        mModel.set(BACKGROUND_COLOR, color);
        if (mSharedImageTilesCoordinator != null && mSharedImageTilesConfigBuilder != null) {
            mSharedImageTilesConfigBuilder.setBorderColor(color);
            mSharedImageTilesCoordinator.updateConfig(mSharedImageTilesConfigBuilder.build());
        }
        publishSnapshotToken();
    }

    private void onTintChanged(
            ColorStateList tint, ColorStateList activityFocusTint, int brandedColorScheme) {
        mModel.set(TINT, mThemeColorProvider.getTint());
    }

    private void setupToolbarButtons() {
        View.OnClickListener showGroupDialogOnClickListener =
                view -> {
                    // Don't handle taps until fully visible and done animating.
                    @Nullable DialogController controller = getTabGridDialogControllerIfExists();
                    if (controller != null && controller.getShowingOrAnimationSupplier().get()) {
                        return;
                    }

                    Tab currentTab = mTabModelSelector.getCurrentTab();
                    if (currentTab == null) return;

                    // Ensure the current tab has a thumbnail.
                    mTabContentManager.cacheTabThumbnail(currentTab);

                    mResetHandler.resetGridWithListOfTabs(getTabsToShowForId(currentTab.getId()));
                    RecordUserAction.record("TabGroup.ExpandedFromStrip.TabGridDialog");
                };
        mModel.set(SHOW_GROUP_DIALOG_ON_CLICK_LISTENER, showGroupDialogOnClickListener);

        View.OnClickListener newTabButtonOnClickListener =
                view -> {
                    Tab currentTab = mTabModelSelector.getCurrentTab();
                    List<Tab> relatedTabs = getTabsToShowForId(currentTab.getId());

                    assert relatedTabs.size() > 0;

                    Tab parentTabToAttach = relatedTabs.get(relatedTabs.size() - 1);
                    mTabCreatorManager
                            .getTabCreator(currentTab.isIncognito())
                            .createNewTab(
                                    new LoadUrlParams(UrlConstants.NTP_URL),
                                    TabLaunchType.FROM_TAB_GROUP_UI,
                                    parentTabToAttach);
                    RecordUserAction.record(
                            "MobileNewTabOpened." + TabGroupUiCoordinator.COMPONENT_NAME);
                };
        mModel.set(NEW_TAB_BUTTON_ON_CLICK_LISTENER, newTabButtonOnClickListener);
    }

    private void hideTabStrip() {
        if (mCurrentTabGroupId == null) return;

        hideTabGridDialog();
        updateTabGroupIdForShareByTab(null);
        mResetHandler.resetStripWithListOfTabs(null);
        mCurrentTabGroupId = null;
        mVisibilityController.setBottomControlsVisible(false);
    }

    private void showTabStrip(Tab tab) {
        if (Objects.equals(mCurrentTabGroupId, tab.getTabGroupId())) return;

        hideTabGridDialog();
        updateTabGroupIdForShareByTab(tab);
        assert tab.getTabGroupId() != null;
        List<Tab> listOfTabs = getTabsToShowForId(tab.getId());
        mResetHandler.resetStripWithListOfTabs(listOfTabs);
        mCurrentTabGroupId = tab.getTabGroupId();

        postUpdateInitialScrollIndex(
                () -> {
                    @Nullable Tab currentTab = mTabModelSelector.getCurrentTab();
                    if (currentTab == null) return 0;

                    return getTabsToShowForId(currentTab.getId()).indexOf(currentTab);
                });
        mVisibilityController.setBottomControlsVisible(true);
    }

    private void postUpdateInitialScrollIndex(Supplier<Integer> indexSupplier) {
        // Post to make sure that the recyclerView already knows how many visible items it has.
        // This is to make sure that we can scroll to a state where the selected tab is in the
        // middle of the strip.
        Handler handler = new Handler();
        handler.post(() -> mModel.set(INITIAL_SCROLL_INDEX, indexSupplier.get()));
    }

    private boolean isOmniboxFocused() {
        @Nullable Boolean focused = mOmniboxFocusStateSupplier.get();
        return Boolean.TRUE.equals(focused);
    }

    private void resetTabStrip() {
        if (!mTabModelSelector.isTabStateInitialized()) return;

        if (mIsShowingHub || isOmniboxFocused()) {
            hideTabStrip();
            return;
        }

        Tab tab = mTabModelSelector.getCurrentTab();
        if (tab == null || !getCurrentTabGroupModelFilter().isTabInTabGroup(tab)) {
            hideTabStrip();
        } else {
            showTabStrip(tab);
        }
    }

    private void updateTabGroupIdForShareByTab(@Nullable Tab tab) {
        if (mTransitiveSharedGroupObserver == null) return;

        if (tab == null || tab.isIncognitoBranded()) {
            mTransitiveSharedGroupObserver.setTabGroupId(/* tabGroupId= */ null);
            return;
        }

        mTransitiveSharedGroupObserver.setTabGroupId(tab.getTabGroupId());
    }

    private void onGroupMembersChanged(@Nullable List<GroupMember> members) {
        if (mSharedImageTilesCoordinator == null) return;

        @Nullable
        String collaborationId = mTransitiveSharedGroupObserver.getCollaborationIdSupplier().get();
        if (members != null && TabShareUtils.isCollaborationIdValid(collaborationId)) {
            mSharedImageTilesCoordinator.onGroupMembersChanged(collaborationId, members);
        } else {
            mSharedImageTilesCoordinator.onGroupMembersChanged(
                    /* collaborationId= */ null, /* members= */ null);
        }
    }

    private void onGroupSharedStateChanged(@Nullable @GroupSharedState Integer groupSharedState) {
        if (groupSharedState == null
                || groupSharedState == GroupSharedState.NOT_SHARED
                || groupSharedState == GroupSharedState.COLLABORATION_ONLY) {
            mModel.set(SHOW_GROUP_DIALOG_BUTTON_VISIBLE, true);
            mModel.set(IMAGE_TILES_CONTAINER_VISIBLE, false);
        } else {
            mModel.set(SHOW_GROUP_DIALOG_BUTTON_VISIBLE, false);
            mModel.set(IMAGE_TILES_CONTAINER_VISIBLE, true);
        }
    }

    /**
     * Get a list of tabs to show based on a tab ID. When tab group is enabled, it will return all
     * tabs that are in the same group with target tab.
     *
     * @param id The ID of the tab that will be used to decide the list of tabs to show.
     */
    private List<Tab> getTabsToShowForId(int id) {
        return getCurrentTabGroupModelFilter().getRelatedTabList(id);
    }

    private TabGroupModelFilter getCurrentTabGroupModelFilter() {
        return mTabModelSelector.getTabGroupModelFilterProvider().getCurrentTabGroupModelFilter();
    }

    private void onTokenComponentChange(Object ignored) {
        publishSnapshotToken();
    }

    private void publishSnapshotToken() {
        Object token =
                new NestedSnapshot(
                        mChildTokenSupplier.get(),
                        mThemeColorProvider.getThemeColor(),
                        mWidthPxSupplier.get());
        mOnSnapshotTokenChange.onResult(token);
    }

    public boolean onBackPressed() {
        // TODO(crbug.com/40099884): add a regression test to make sure that the back button closes
        // the dialog when the dialog is showing.
        @Nullable DialogController controller = getTabGridDialogControllerIfExists();
        return controller != null ? controller.handleBackPressed() : false;
    }

    @Override
    public @BackPressResult int handleBackPress() {
        @Nullable DialogController controller = getTabGridDialogControllerIfExists();
        if (controller != null) {
            return controller.handleBackPress();
        }
        return BackPressResult.FAILURE;
    }

    @Override
    public ObservableSupplier<Boolean> getHandleBackPressChangedSupplier() {
        return mHandleBackPressChangedSupplier;
    }

    public void destroy() {
        if (mTabModelSelector != null) {
            var filterProvider = mTabModelSelector.getTabGroupModelFilterProvider();

            filterProvider.removeTabGroupModelFilterObserver(mTabModelObserver);
            mTabModelSelector.getCurrentTabModelSupplier().removeObserver(mCurrentTabModelObserver);
            if (mTabGroupModelFilterObserver != null) {
                filterProvider
                        .getTabGroupModelFilter(false)
                        .removeTabGroupObserver(mTabGroupModelFilterObserver);
                filterProvider
                        .getTabGroupModelFilter(true)
                        .removeTabGroupObserver(mTabGroupModelFilterObserver);
            }
        }

        if (mTabModelSelectorTabObserver != null) {
            mTabModelSelectorTabObserver.destroy();
        }
        if (mLayoutStateProvider != null) {
            mLayoutStateProvider.removeObserver(mLayoutStateObserver);
        }
        if (mCallbackController != null) {
            mCallbackController.destroy();
            mCallbackController = null;
        }
        if (mOmniboxFocusObserver != null) {
            mOmniboxFocusStateSupplier.removeObserver(mOmniboxFocusObserver);
        }
        if (mTransitiveSharedGroupObserver != null) {
            mTransitiveSharedGroupObserver
                    .getGroupSharedStateSupplier()
                    .removeObserver(mOnGroupSharedStateChanged);
            mTransitiveSharedGroupObserver
                    .getGroupMembersSupplier()
                    .removeObserver(mOnGroupMembersChanged);
            mTransitiveSharedGroupObserver.destroy();
        }
        mChildTokenSupplier.removeObserver(mOnTokenComponentChange);
        mWidthPxSupplier.removeObserver(mOnTokenComponentChange);
        mThemeColorProvider.removeThemeColorObserver(mThemeColorObserver);
        mThemeColorProvider.removeTintObserver(mTintObserver);
    }

    private @Nullable DialogController getTabGridDialogControllerIfExists() {
        if (mTabGridDialogControllerSupplier == null) return null;
        if (!mTabGridDialogControllerSupplier.hasValue()) return null;
        return mTabGridDialogControllerSupplier.get();
    }

    private void hideTabGridDialog() {
        DialogController controller = getTabGridDialogControllerIfExists();
        if (controller != null) controller.hideDialog(/* showAnimation= */ false);
    }
}

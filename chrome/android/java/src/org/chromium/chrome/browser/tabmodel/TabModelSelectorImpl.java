// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.app.Activity;
import android.content.Context;
import android.os.Handler;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.ContextUtils;
import org.chromium.base.Token;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.build.annotations.EnsuresNonNull;
import org.chromium.build.annotations.MonotonicNonNull;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.app.tabwindow.TabWindowManagerSingleton;
import org.chromium.chrome.browser.flags.ActivityType;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager;
import org.chromium.chrome.browser.ntp.RecentlyClosedBridge;
import org.chromium.chrome.browser.profiles.ProfileProvider;
import org.chromium.chrome.browser.tab.SadTab;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabHidingType;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabLoadIfNeededCaller;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncServiceFactory;
import org.chromium.chrome.browser.tab_ui.TabContentManager;
import org.chromium.chrome.browser.tabmodel.NextTabPolicy.NextTabPolicySupplier;
import org.chromium.chrome.browser.tabmodel.TabCreator.NeedsTabModel;
import org.chromium.chrome.browser.tabmodel.TabCreator.NeedsTabModelOrderController;
import org.chromium.chrome.browser.tasks.tab_management.TabShareUtils;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.url.GURL;

import java.util.Collections;
import java.util.function.Supplier;

/**
 * This class manages all the ContentViews in the app. As it manipulates views, it must be
 * instantiated and used in the UI Thread. It acts as a TabModel which delegates all TabModel
 * methods to the active model that it contains.
 */
@NullMarked
public class TabModelSelectorImpl extends TabModelSelectorBase implements TabModelDelegate {
    public static final int CUSTOM_TABS_SELECTOR_INDEX = -1;

    // Type of the Activity for this tab model. Used by sync to determine how to handle restore
    // on cold start.
    private final @ActivityType int mActivityType;
    private final TabModelOrderController mOrderController;
    private final AsyncTabParamsManager mAsyncTabParamsManager;
    private final OneshotSupplier<ProfileProvider> mProfileProviderSupplier;
    private final Context mContext;
    private final @Nullable ModalDialogManager mModalDialogManager;
    private final boolean mIsUndoSupported;
    private final NextTabPolicySupplier mNextTabPolicySupplier;
    private final @Nullable MultiInstanceManager mMultiInstanceManager;

    private @MonotonicNonNull TabContentManager mTabContentManager;
    private @MonotonicNonNull RecentlyClosedBridge mRecentlyClosedBridge;
    private @MonotonicNonNull TabModelSelectorTabObserver mTabModelSelectorTabObserver;
    private @Nullable Tab mVisibleTab;

    /**
     * Builds a {@link TabModelSelectorImpl} instance.
     *
     * @param context The activity context.
     * @param modalDialogManager A {@link ModalDialogManager}.
     * @param profileProviderSupplier Provides the Profiles used in this selector.
     * @param tabCreatorManager A {@link TabCreatorManager} instance.
     * @param nextTabPolicySupplier Supplier of a policy to decide which tab to select next.
     * @param asyncTabParamsManager The params manager to use for async tab creation.
     * @param supportUndo Whether a tab closure can be undone.
     * @param activityType Type of the activity for the tab model selector.
     * @param startIncognito Whether to start in incognito mode.
     */
    public TabModelSelectorImpl(
            Context context,
            @Nullable ModalDialogManager modalDialogManager,
            OneshotSupplier<ProfileProvider> profileProviderSupplier,
            TabCreatorManager tabCreatorManager,
            NextTabPolicySupplier nextTabPolicySupplier,
            @Nullable MultiInstanceManager multiInstanceManager,
            AsyncTabParamsManager asyncTabParamsManager,
            boolean supportUndo,
            @ActivityType int activityType,
            boolean startIncognito) {
        super(tabCreatorManager, startIncognito);
        mContext = context;
        mModalDialogManager = modalDialogManager;
        mProfileProviderSupplier = profileProviderSupplier;
        mIsUndoSupported = supportUndo;
        mOrderController = new TabModelOrderControllerImpl(this);
        mNextTabPolicySupplier = nextTabPolicySupplier;
        mMultiInstanceManager = multiInstanceManager;
        mAsyncTabParamsManager = asyncTabParamsManager;
        mActivityType = activityType;
    }

    @Override
    public void markTabStateInitialized() {
        if (isTabStateInitialized()) return;
        super.markTabStateInitialized();

        TabModelJniBridge model = (TabModelJniBridge) getModel(false);
        if (model != null) {
            model.completeInitialization();
            if (!ChromeFeatureList.isEnabled(ChromeFeatureList.HEADLESS_TAB_MODEL)) {
                model.broadcastSessionRestoreComplete();
            }
        } else {
            assert false : "Normal tab model is null after tab state loaded.";
        }
    }

    /**
     * Should be called once the native library is loaded so that the actual internals of this class
     * can be initialized.
     *
     * @param tabContentProvider A {@link TabContentManager} instance.
     */
    @Override
    public void onNativeLibraryReady(
            TabContentManager tabContentProvider, boolean wasTabCollectionsActive) {
        assert mTabContentManager == null : "onNativeLibraryReady called twice!";

        ProfileProvider profileProvider = mProfileProviderSupplier.get();
        assert profileProvider != null;

        TabCreator regularTabCreator = getTabCreatorManager().getTabCreator(false);
        TabCreator incognitoTabCreator = getTabCreatorManager().getTabCreator(true);
        mRecentlyClosedBridge =
                new RecentlyClosedBridge(profileProvider.getOriginalProfile(), this);
        Supplier<TabGroupModelFilter> regularTabGroupModelFilterSupplier =
                () ->
                        assumeNonNull(
                                getTabGroupModelFilterProvider()
                                        .getTabGroupModelFilter(/* isIncognito= */ false));
        TabRemover regularTabRemover =
                mModalDialogManager != null
                        ? new TabRemoverImpl(
                                mContext, mModalDialogManager, regularTabGroupModelFilterSupplier)
                        : new PassthroughTabRemover(regularTabGroupModelFilterSupplier);
        TabUngrouperFactory tabUngrouperFactory =
                (isIncognitoBranded, tabGroupModelFilterSupplier) -> {
                    return (isIncognitoBranded || mModalDialogManager == null)
                            ? new PassthroughTabUngrouper(tabGroupModelFilterSupplier)
                            : new TabUngrouperImpl(
                                    mContext, mModalDialogManager, tabGroupModelFilterSupplier);
                };
        TabModelHolder normalModelHolder =
                TabModelHolderFactory.createTabModelHolder(
                        profileProvider.getOriginalProfile(),
                        mActivityType,
                        regularTabCreator,
                        incognitoTabCreator,
                        mOrderController,
                        tabContentProvider,
                        mNextTabPolicySupplier,
                        mAsyncTabParamsManager,
                        this,
                        regularTabRemover,
                        mIsUndoSupported,
                        /* isArchivedTabModel= */ false,
                        tabUngrouperFactory,
                        wasTabCollectionsActive);
        if (regularTabCreator instanceof NeedsTabModel needsTabModel) {
            needsTabModel.setTabModel(normalModelHolder.tabModel);
        }
        if (regularTabCreator
                instanceof NeedsTabModelOrderController needsTabModelOrderController) {
            needsTabModelOrderController.setTabModelOrderController(mOrderController);
        }

        TabRemover incognitoTabRemover =
                new PassthroughTabRemover(
                        () ->
                                assumeNonNull(
                                        getTabGroupModelFilterProvider()
                                                .getTabGroupModelFilter(/* isIncognito= */ true)));
        IncognitoTabModelHolder incognitoModelHolder =
                TabModelHolderFactory.createIncognitoTabModelHolder(
                        profileProvider,
                        regularTabCreator,
                        incognitoTabCreator,
                        mOrderController,
                        tabContentProvider,
                        mNextTabPolicySupplier,
                        mAsyncTabParamsManager,
                        mActivityType,
                        this,
                        incognitoTabRemover,
                        tabUngrouperFactory,
                        wasTabCollectionsActive);
        if (incognitoTabCreator instanceof NeedsTabModel needsTabModel) {
            needsTabModel.setTabModel(incognitoModelHolder.tabModel);
        }
        if (incognitoTabCreator
                instanceof NeedsTabModelOrderController needsTabModelOrderController) {
            needsTabModelOrderController.setTabModelOrderController(mOrderController);
        }
        onNativeLibraryReadyInternal(tabContentProvider, normalModelHolder, incognitoModelHolder);
    }

    @EnsuresNonNull("mTabContentManager")
    @VisibleForTesting
    void onNativeLibraryReadyInternal(
            TabContentManager tabContentProvider,
            TabModelHolder normalModelHolder,
            IncognitoTabModelHolder incognitoModelHolder) {
        mTabContentManager = tabContentProvider;
        initialize(normalModelHolder, incognitoModelHolder);

        addObserver(
                new TabModelSelectorObserver() {
                    @Override
                    public void onNewTabCreated(Tab tab, @TabCreationState int creationState) {
                        // Only invalidate if the tab exists in the currently selected model.
                        if (getCurrentModel().getTabById(tab.getId()) != null) {
                            mTabContentManager.invalidateIfChanged(tab.getId(), tab.getUrl());
                        }
                    }
                });

        assert mTabModelSelectorTabObserver == null;
        mTabModelSelectorTabObserver =
                new TabModelSelectorTabObserver(this) {
                    @Override
                    public void onUrlUpdated(Tab tab) {
                        TabModel model = getModelForTabId(tab.getId());
                        if (model == getCurrentModel()) {
                            mTabContentManager.invalidateIfChanged(tab.getId(), tab.getUrl());
                        }
                    }

                    @Override
                    public void onPageLoadStarted(Tab tab, GURL url) {
                        mTabContentManager.invalidateIfChanged(tab.getId(), tab.getUrl());
                    }

                    @Override
                    public void onCrash(Tab tab) {
                        if (SadTab.isShowing(tab)) {
                            mTabContentManager.removeTabThumbnail(tab.getId());
                        }
                    }

                    @Override
                    public void onActivityAttachmentChanged(
                            Tab tab, @Nullable WindowAndroid window) {
                        if (window == null && !isReparentingInProgress()) {
                            TabModel tabModel = getModel(tab.isIncognito());

                            // Do not currently support moving grouped tabs.
                            TabGroupModelFilter filter =
                                    getTabGroupModelFilterProvider()
                                            .getTabGroupModelFilter(tab.isIncognito());
                            assumeNonNull(filter);
                            if (filter.isTabInTabGroup(tab)) {
                                filter.getTabUngrouper()
                                        .ungroupTabs(
                                                Collections.singletonList(tab),
                                                /* trailing= */ true,
                                                /* allowDialog= */ false);
                            }

                            tabModel.getTabRemover().removeTab(tab, /* allowDialog= */ false);
                        }
                    }

                    @Override
                    public void onCloseContents(Tab tab) {
                        tryCloseTab(
                                TabClosureParams.closeTab(tab).allowUndo(false).build(),
                                /* allowDialog= */ false);
                    }
                };
    }

    @Override
    public void openMostRecentlyClosedEntry(TabModel tabModel) {
        assert tabModel == getModel(false)
                : "Trying to restore a tab from an off-the-record tab model.";
        assumeNonNull(mRecentlyClosedBridge);
        mRecentlyClosedBridge.openMostRecentlyClosedEntry(tabModel);
    }

    @Override
    public void destroy() {
        super.destroy();
        if (mRecentlyClosedBridge != null) mRecentlyClosedBridge.destroy();
        if (mTabModelSelectorTabObserver != null) mTabModelSelectorTabObserver.destroy();
    }

    /**
     * Exposed to allow tests to initialize the selector with different tab models.
     *
     * @param normalModelHolder The normal tab model.
     * @param incognitoModelHolder The incognito tab model.
     */
    public void initializeForTesting(
            TabModelHolder normalModelHolder, IncognitoTabModelHolder incognitoModelHolder) {
        initialize(normalModelHolder, incognitoModelHolder);
    }

    @Override
    public void selectModel(boolean incognito) {
        TabModel oldModel = getCurrentModel();
        super.selectModel(incognito);
        TabModel newModel = getCurrentModel();
        if (oldModel != newModel) {
            TabModelUtils.setIndex(newModel, newModel.index());

            // Make the call to notifyDataSetChanged() after any delayed events
            // have had a chance to fire. Otherwise, this may result in some
            // drawing to occur before animations have a chance to work.
            new Handler()
                    .post(
                            new Runnable() {
                                @Override
                                public void run() {
                                    notifyChanged();
                                    // The tab model has changed to regular and all the visual
                                    // elements wrt regular mode is in-place. We can now signal
                                    // the re-auth to hide the dialog.
                                    if (mIncognitoReauthDialogDelegate != null
                                            && !newModel.isIncognito()) {
                                        mIncognitoReauthDialogDelegate
                                                .onAfterRegularTabModelChanged();
                                    }
                                }
                            });
        }
    }

    @Override
    public void moveTabToWindow(Tab tab, Activity activity, int newIndex) {
        if (getModel(tab.isIncognito()).getTabById(tab.getId()) != tab) return;

        assert mMultiInstanceManager != null;
        mMultiInstanceManager.moveTabsToWindow(activity, Collections.singletonList(tab), newIndex);
    }

    @Override
    public void moveTabGroupToWindow(
            Token tabGroupId, Activity activity, int newIndex, boolean isIncognito) {
        TabGroupModelFilter tabGroupModelFilter =
                getTabGroupModelFilterProvider().getTabGroupModelFilter(isIncognito);
        assumeNonNull(tabGroupModelFilter);
        if (!tabGroupModelFilter.tabGroupExists(tabGroupId)) return;

        TabModel tabModel = tabGroupModelFilter.getTabModel();
        Tab currentTab = tabModel.getCurrentTabSupplier().get();
        assert currentTab != null;
        Activity currentActivity = ContextUtils.activityFromContext(currentTab.getContext());
        if (currentActivity == null) return;

        String collaborationId = null;
        if (!isIncognito) {
            TabGroupSyncService tabGroupSyncService =
                    TabGroupSyncServiceFactory.getForProfile(assumeNonNull(tabModel.getProfile()));
            collaborationId =
                    TabShareUtils.getCollaborationIdOrNull(tabGroupId, tabGroupSyncService);
        }
        TabGroupMetadata tabGroupMetadata =
                TabGroupMetadataExtractor.extractTabGroupMetadata(
                        tabGroupModelFilter,
                        tabGroupModelFilter.getTabsInGroup(tabGroupId),
                        TabWindowManagerSingleton.getInstance().getIdForWindow(currentActivity),
                        currentTab.getId(),
                        TabShareUtils.isCollaborationIdValid(collaborationId));
        if (tabGroupMetadata == null) return;

        assert mMultiInstanceManager != null;
        mMultiInstanceManager.moveTabGroupToWindow(activity, tabGroupMetadata, newIndex);
    }

    /**
     * Commits all pending tab closures for all {@link TabModel}s in this {@link TabModelSelector}.
     */
    @Override
    public void commitAllTabClosures() {
        for (int i = 0; i < getModels().size(); i++) {
            getModels().get(i).commitAllTabClosures();
        }
    }

    @Override
    public void requestToShowTab(@Nullable Tab tab, @TabSelectionType int type) {
        boolean isFromExternalApp =
                tab != null && tab.getLaunchType() == TabLaunchType.FROM_EXTERNAL_APP;
        if (mVisibleTab != null && mVisibleTab != tab && !mVisibleTab.needsReload()) {
            boolean attached =
                    mVisibleTab.getWebContents() != null
                            && mVisibleTab.getWebContents().getTopLevelNativeWindow() != null;
            if (mVisibleTab.isInitialized() && attached) {
                // TODO(dtrainor): Once we figure out why we can't grab a snapshot from the current
                // tab when we have other tabs loading from external apps remove the checks for
                // FROM_EXTERNAL_APP/FROM_NEW.
                if (!mVisibleTab.isClosing()
                        && (!isFromExternalApp || type != TabSelectionType.FROM_NEW)) {
                    cacheTabBitmap(mVisibleTab);
                }
                mVisibleTab.hide(TabHidingType.CHANGED_TABS);
                notifyTabHidden(mVisibleTab);
            }
            mVisibleTab = null;
        }

        if (tab == null) {
            notifyChanged();
            return;
        }

        // We hit this case when the user enters tab switcher and comes back to the current tab
        // without actual tab switch.
        if (mVisibleTab == tab && !mVisibleTab.isHidden()) {
            // The current tab might have been killed by the os while in tab switcher.
            tab.loadIfNeeded(TabLoadIfNeededCaller.REQUEST_TO_SHOW_TAB);
            // |tabToDropImportance| must be null, so no need to drop importance.
            return;
        }
        mVisibleTab = tab;

        // Don't execute the tab display part if Chrome has just been sent to background. This
        // avoids unecessary work (tab restore) and prevents pollution of tab display metrics - see
        // http://crbug.com/316166.
        if (type != TabSelectionType.FROM_EXIT) {
            tab.show(type, TabLoadIfNeededCaller.REQUEST_TO_SHOW_TAB_THEN_SHOW);
        }
    }

    private void cacheTabBitmap(Tab tabToCache) {
        // Trigger a capture of this tab.
        if (tabToCache == null) return;
        assumeNonNull(mTabContentManager);
        mTabContentManager.cacheTabThumbnail(tabToCache);
    }

    @Override
    public boolean isTabModelRestored() {
        return isTabStateInitialized();
    }
}

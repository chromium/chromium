// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_group_sync;

import androidx.annotation.Nullable;

import org.chromium.base.CallbackController;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.tab_group_sync.LocalTabGroupId;
import org.chromium.components.tab_group_sync.OpeningSource;
import org.chromium.components.tab_group_sync.SavedTabGroup;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.components.tab_group_sync.TabGroupUiActionHandler;
import org.chromium.url.GURL;

/**
 * Central class responsible for making things happen. i.e. apply remote changes to local and local
 * changes to remote. This is a per-activity object and hence responsible for handling updates for
 * current window only.
 */
public final class TabGroupSyncController implements TabGroupUiActionHandler {
    /**
     * A delegate in helping out with creating and navigating tabs in response to remote updates
     * from sync. The tab will be created in a background state and will not be navigated
     * immediately. The navigation will happen only when the tab becomes active such as user
     * switches to the tab.
     */
    // TODO(shaktisahu): Should this be called TabNavigationDelegate or NavigationDelegate?
    public interface TabCreationDelegate {
        /**
         * Creates a tab in background in the local tab model. The tab will be created at the given
         * position and will be loaded with the given URL. The tab is created in a frozen state and
         * will not be loaded until when user switches back to it.
         *
         * @param url The URL to load.
         * @param title The title of the tab to be shown.
         * @param parent The parent of the tab.
         * @param position The position of the tab in the tab model.
         * @return The tab created.
         */
        Tab createBackgroundTab(GURL url, String title, Tab parent, int position);

        /**
         * Called to navigate a tab to a given URL and set its title. If the tab is in foreground,
         * the navigation will happen right away.
         *
         * @param tab The tab on which the URL will be loaded.
         * @param url The URL to load.
         * @param title The title to be shown.
         * @param isForegroundTab Whether the tab is a foreground tab.
         */
        void navigateToUrl(Tab tab, GURL url, String title, boolean isForegroundTab);
    }

    private final TabModelSelector mTabModelSelector;
    private final TabGroupSyncService mTabGroupSyncService;
    private final PrefService mPrefService;
    private final Supplier<Boolean> mIsActiveWindowSupplier;
    private final TabGroupModelFilter mTabGroupModelFilter;
    private final NavigationTracker mNavigationTracker;
    private final TabCreatorManager mTabCreatorManager;
    private final TabCreationDelegate mTabCreationDelegate;
    private final LocalTabGroupMutationHelper mLocalMutationHelper;
    private final RemoteTabGroupMutationHelper mRemoteMutationHelper;
    private TabGroupSyncLocalObserver mLocalObserver;
    private TabGroupSyncRemoteObserver mRemoteObserver;
    private StartupHelper mStartupHelper;
    private boolean mSyncBackendInitialized;
    private CallbackController mCallbackController = new CallbackController();

    private final TabGroupSyncService.Observer mSyncInitObserver =
            new TabGroupSyncService.Observer() {
                @Override
                public void onInitialized() {
                    mTabGroupSyncService.removeObserver(mSyncInitObserver);
                    mSyncBackendInitialized = true;
                    assert mTabModelSelector.isTabStateInitialized();
                    initializeTabGroupSyncComponents();
                }

                @Override
                public void onTabGroupAdded(SavedTabGroup group, int source) {}

                @Override
                public void onTabGroupUpdated(SavedTabGroup group, int source) {}

                @Override
                public void onTabGroupRemoved(LocalTabGroupId localTabGroupId, int source) {}

                @Override
                public void onTabGroupRemoved(String syncTabGroupId, int source) {}

                @Override
                public void onTabGroupLocalIdChanged(
                        String syncTabGroupId, @Nullable LocalTabGroupId localTabGroupId) {}
            };

    /** Constructor. */
    public TabGroupSyncController(
            TabModelSelector tabModelSelector,
            TabCreatorManager tabCreatorManager,
            TabGroupSyncService tabGroupSyncService,
            PrefService prefService,
            Supplier<Boolean> isActiveWindowSupplier) {
        mTabModelSelector = tabModelSelector;
        mTabCreatorManager = tabCreatorManager;
        mTabGroupSyncService = tabGroupSyncService;
        mPrefService = prefService;
        mIsActiveWindowSupplier = isActiveWindowSupplier;

        mNavigationTracker = new NavigationTracker();
        mTabCreationDelegate =
                new TabCreationDelegateImpl(
                        mTabCreatorManager.getTabCreator(/* incognito= */ false),
                        mNavigationTracker);
        mTabGroupModelFilter =
                ((TabGroupModelFilter)
                        tabModelSelector.getTabModelFilterProvider().getTabModelFilter(false));

        mLocalMutationHelper =
                new LocalTabGroupMutationHelper(
                        mTabGroupModelFilter, mTabGroupSyncService, mTabCreationDelegate);
        mRemoteMutationHelper =
                new RemoteTabGroupMutationHelper(mTabGroupModelFilter, mTabGroupSyncService);

        TabModelUtils.runOnTabStateInitialized(
                tabModelSelector,
                mCallbackController.makeCancelable(selector -> onTabStateInitialized()));
    }

    /** Called when the activity is getting destroyed. */
    public void destroy() {
        mCallbackController.destroy();
        if (mLocalObserver != null) mLocalObserver.destroy();
        if (mRemoteObserver != null) mRemoteObserver.destroy();
    }

    @Override
    public void openTabGroup(String syncId) {
        assert mSyncBackendInitialized;
        if (!mSyncBackendInitialized) return;

        // Skip groups that are open in another window, or have been deleted.
        SavedTabGroup savedTabGroup = mTabGroupSyncService.getGroup(syncId);
        if (savedTabGroup == null || savedTabGroup.localId != null) return;

        mLocalObserver.enableObservers(false);
        mLocalMutationHelper.createNewTabGroup(savedTabGroup, OpeningSource.OPENED_FROM_REVISIT_UI);
        mLocalObserver.enableObservers(true);
    }

    private void onTabStateInitialized() {
        mTabGroupSyncService.addObserver(mSyncInitObserver);
    }

    /**
     * Construction and initialization of this glue layer between sync and tab model. Sets up
     * observers for both directions and starts syncing. Invoked only after sync and local tab model
     * have been initialized.
     */
    private void initializeTabGroupSyncComponents() {
        mStartupHelper =
                new StartupHelper(
                        mTabGroupModelFilter,
                        mTabGroupSyncService,
                        mLocalMutationHelper,
                        mRemoteMutationHelper);
        mLocalObserver =
                new TabGroupSyncLocalObserver(
                        mTabModelSelector,
                        mTabGroupModelFilter,
                        mTabGroupSyncService,
                        mRemoteMutationHelper,
                        mNavigationTracker);
        mRemoteObserver =
                new TabGroupSyncRemoteObserver(
                        mTabGroupModelFilter,
                        mTabGroupSyncService,
                        mLocalMutationHelper,
                        enable -> mLocalObserver.enableObservers(enable),
                        mPrefService,
                        mIsActiveWindowSupplier);

        mStartupHelper.initializeTabGroupSync();
        mLocalObserver.enableObservers(true);
    }
}

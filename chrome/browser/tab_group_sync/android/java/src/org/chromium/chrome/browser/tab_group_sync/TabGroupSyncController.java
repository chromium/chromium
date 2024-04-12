// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_group_sync;

import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.url.GURL;

/**
 * Central class responsible for making things happen. i.e. apply remote changes to local and local
 * changes to remote.
 */
public final class TabGroupSyncController {
    /**
     * A delegate in helping out with creating and navigating tabs in response to remote updates
     * from sync. The tab will be created in a background state and will not be navigated
     * immediately. The navigation will happen only when the tab becomes active such as user
     * switches to the tab.
     */
    public interface TabCreationDelegate {
        /**
         * Creates a tab in background in the local tab model. The tab will be created at the given
         * position and will be loaded with the given URL. The URL will not be loaded right away.
         *
         * @param url The URL to load.
         * @param parent The parent of the tab.
         * @param position The position of the tab in the tab model.
         * @return The tab created.
         */
        Tab createBackgroundTab(GURL url, Tab parent, int position);
    }

    private final TabModelSelector mTabModelSelector;
    private final TabGroupSyncService mTabGroupSyncService;
    private final TabGroupModelFilter mTabGroupModelFilter;
    private final NavigationTracker mNavigationTracker;
    private final TabCreatorManager mTabCreatorManager;
    private final TabCreationDelegate mTabCreationDelegate;
    private final TabGroupSyncLocalObserver mLocalObserver;
    private final TabGroupSyncRemoteObserver mRemoteObserver;
    private final LocalTabGroupMutationHelper mLocalMutationHelper;
    private final RemoteTabGroupMutationHelper mRemoteMutationHelper;
    private final StartupHelper mStartupHelper;
    private boolean mSyncBackendInitialized;

    /** Constructor. */
    public TabGroupSyncController(
            TabModelSelector tabModelSelector,
            TabCreatorManager tabCreatorManager,
            TabGroupSyncService tabGroupSyncService) {
        mTabModelSelector = tabModelSelector;
        mTabCreatorManager = tabCreatorManager;
        mTabGroupSyncService = tabGroupSyncService;

        mNavigationTracker = new NavigationTracker();
        mTabCreationDelegate = new TabCreationDelegateImpl(mTabCreatorManager, mNavigationTracker);
        mTabGroupModelFilter =
                ((TabGroupModelFilter)
                        tabModelSelector.getTabModelFilterProvider().getTabModelFilter(false));

        mLocalMutationHelper =
                new LocalTabGroupMutationHelper(
                        mTabGroupModelFilter,
                        mTabGroupSyncService,
                        mTabCreationDelegate,
                        mNavigationTracker);
        mRemoteMutationHelper =
                new RemoteTabGroupMutationHelper(mTabGroupModelFilter, mTabGroupSyncService);
        mStartupHelper =
                new StartupHelper(
                        mTabGroupModelFilter, mTabGroupSyncService, mRemoteMutationHelper);

        mLocalObserver =
                new TabGroupSyncLocalObserver(
                        tabModelSelector,
                        mTabGroupModelFilter,
                        mTabGroupSyncService,
                        mRemoteMutationHelper,
                        mNavigationTracker);
        mRemoteObserver =
                new TabGroupSyncRemoteObserver(
                        mTabGroupModelFilter,
                        mTabGroupSyncService,
                        mLocalMutationHelper,
                        mTabCreationDelegate,
                        mNavigationTracker,
                        enable -> mLocalObserver.enableObservers(enable),
                        this::onSyncBackendInitialized);
        TabModelUtils.runOnTabStateInitialized(
                tabModelSelector, selector -> maybeCompleteInitialization());
    }

    /** Called when the activity is getting destroyed. */
    public void destroy() {
        mLocalObserver.destroy();
        mRemoteObserver.destroy();
    }

    private void onSyncBackendInitialized() {
        mSyncBackendInitialized = true;
        maybeCompleteInitialization();
    }

    private void maybeCompleteInitialization() {
        if (!mSyncBackendInitialized || !mTabModelSelector.isTabStateInitialized()) return;

        mStartupHelper.initializeTabGroupSync();
        mLocalObserver.enableObservers(true);
    }

    private static class TabCreationDelegateImpl implements TabCreationDelegate {
        private final TabCreatorManager mTabCreatorManager;
        private final NavigationTracker mNavigationTracker;

        /** Constructor. */
        public TabCreationDelegateImpl(
                TabCreatorManager tabCreatorManager, NavigationTracker navigationTracker) {
            mTabCreatorManager = tabCreatorManager;
            mNavigationTracker = navigationTracker;
        }

        @Override
        public Tab createBackgroundTab(GURL url, Tab parent, int position) {
            LoadUrlParams loadUrlParams = new LoadUrlParams(url);
            mNavigationTracker.setNavigationWasFromSync(
                    loadUrlParams.getNavigationHandleUserData());
            return createLiveTab(loadUrlParams, parent, position);
        }

        private Tab createLiveTab(LoadUrlParams loadUrlParams, Tab parent, int position) {
            // TODO(b/330890093): Introduce a new launch type and replace this code with frozen tab.
            return mTabCreatorManager
                    .getTabCreator(/* incognito= */ false)
                    .createNewTab(loadUrlParams, TabLaunchType.FROM_RESTORE, parent, position);
        }
    }
}

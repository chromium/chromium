// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browseractions;

import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.base.task.AsyncTask;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.browseractions.BrowserActionsTabCreatorManager.BrowserActionsTabCreator;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.ChromeTabCreator;
import org.chromium.chrome.browser.tabmodel.EmptyTabModel;
import org.chromium.chrome.browser.tabmodel.EmptyTabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModel.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabModel.TabSelectionType;
import org.chromium.chrome.browser.tabmodel.TabModelDelegate;
import org.chromium.chrome.browser.tabmodel.TabModelImpl;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelOrderController;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorBase;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.browser.tabmodel.TabPersistentStore;
import org.chromium.chrome.browser.tabmodel.TabPersistentStore.TabPersistentStoreObserver;
import org.chromium.content_public.browser.LoadUrlParams;

import java.util.ArrayList;
import java.util.List;

/**
 * The tab model selector creates Tabs for Browser Actions. Tabs created by it are not shown in the
 * foreground and don't need {@link TabContentMananger}.
 */
public class BrowserActionsTabModelSelector
        extends TabModelSelectorBase implements TabModelDelegate {
    /** The singleton reference. */
    private static BrowserActionsTabModelSelector sInstance;
    private static final Object sInstanceLock = new Object();

    private final TabCreatorManager mTabCreatorManager;

    private final TabPersistentStore mTabSaver;

    private final TabModelOrderController mOrderController;

    private final List<LoadUrlParams> mPendingUrls = new ArrayList<>();

    private Runnable mTabCreationRunnable;

    /**
     * Builds a {@link BrowserActionsTabModelSelector} instance.
     */
    private BrowserActionsTabModelSelector() {
        super();
        mTabCreatorManager = new BrowserActionsTabCreatorManager();
        final TabPersistentStoreObserver persistentStoreObserver =
                new TabPersistentStoreObserver() {
                    @Override
                    public void onStateLoaded() {
                        markTabStateInitialized();
                    }
                };
        BrowserActionsTabPersistencePolicy persistencePolicy =
                new BrowserActionsTabPersistencePolicy();
        mTabSaver = new TabPersistentStore(
                persistencePolicy, this, mTabCreatorManager, persistentStoreObserver);
        mOrderController = new TabModelOrderController(this);
    }

    /**
     * @return The singleton instance of {@link BrowserActionsTabModelSelector}.
     */
    public static BrowserActionsTabModelSelector getInstance() {
        synchronized (sInstanceLock) {
            ThreadUtils.assertOnUiThread();
            if (sInstance == null) {
                sInstance = new BrowserActionsTabModelSelector();
                sInstance.initializeSelector();
            }
            return sInstance;
        }
    }

    /**
     * @return Whether {@link BrowserActionsTabModelSelector} has been initialized.
     */
    public static boolean isInitialized() {
        return sInstance != null;
    }

    /**
     * Initializes the selectors for the {@link BrowserActionsTabModelSelector}.
     */
    public void initializeSelector() {
        BrowserActionsTabCreator regularTabCreator =
                (BrowserActionsTabCreator) mTabCreatorManager.getTabCreator(false);
        TabModelImpl normalModel = new TabModelImpl(false, false, regularTabCreator, null, null,
                mOrderController, null, mTabSaver, this, false);
        TabModel incognitoModel = EmptyTabModel.getInstance();
        initialize(false, normalModel, incognitoModel);
        regularTabCreator.setTabModel(getCurrentModel());
    }

    @Override
    public void markTabStateInitialized() {
        super.markTabStateInitialized();
        // Add observer when tab model is restored to prevent restored tabs updating tab model.
        TabModelObserver tabModelObserver = new EmptyTabModelObserver() {
            @Override
            public void didAddTab(Tab tab, @TabLaunchType int type) {
                if (tab != null) {
                    mTabSaver.addTabToSaveQueue(tab);
                }
            }
        };
        getModel(false).addObserver(tabModelObserver);
        if (mTabCreationRunnable != null) {
            ThreadUtils.runOnUiThread(mTabCreationRunnable);
        }
        mTabCreationRunnable = null;
    }

    /**
     * Creates a new Tab with given url in Browser Actions tab model.
     * @param loadUrlParams The url params to be opened.
     * @param tabCreatedCallback The {@link Callback} to run when tab is created.
     */
    public void openNewTab(LoadUrlParams loadUrlParams, Callback<Integer> tabCreatedCallback) {
        // If tab model is restored, directly create a new tab.
        if (isTabStateInitialized()) {
            createNewTab(loadUrlParams, tabCreatedCallback);
            return;
        }
        if (mTabCreationRunnable == null) {
            mTabCreationRunnable = new Runnable() {
                @Override
                public void run() {
                    for (int i = 0; i < mPendingUrls.size(); i++) {
                        createNewTab(mPendingUrls.get(i), tabCreatedCallback);
                    }
                    mPendingUrls.clear();
                }
            };
            new AsyncTask<Void>() {
                @Override
                protected Void doInBackground() {
                    mTabSaver.loadState(true);
                    mTabSaver.restoreTabs(false);
                    return null;
                }
            }
                    .executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
        }
        mPendingUrls.add(loadUrlParams);
    }

    private void createNewTab(LoadUrlParams params, Callback<Integer> tabCreatedCallback) {
        Tab tab = openNewTab(params, TabLaunchType.FROM_BROWSER_ACTIONS, null, false);
        tabCreatedCallback.onResult(tab.getId());
    }

    @Override
    public void requestToShowTab(Tab tab, @TabSelectionType int type) {}

    @Override
    public boolean closeAllTabsRequest(boolean incognito) {
        return false;
    }

    @Override
    public boolean isCurrentModel(TabModel model) {
        return false;
    }

    @Override
    public boolean isInOverviewMode() {
        return false;
    }

    @Override
    public boolean isSessionRestoreInProgress() {
        return false;
    }

    /**
     * @return Return the current tab model. For Browser Actions, it always returns the normal tab
     * model.
     */
    public TabModelImpl getModel() {
        return (TabModelImpl) getModel(false);
    }

    /**
     * Save the whole tab list to the disk.
     */
    public void saveState() {
        commitAllTabClosures();
        mTabSaver.saveState();
    }

    @Override
    public Tab openNewTab(
            LoadUrlParams loadUrlParams, @TabLaunchType int type, Tab parent, boolean incognito) {
        if (!isTabStateInitialized()) {
            assert type == TabLaunchType.FROM_RESTORE;
        }
        return mTabCreatorManager.getTabCreator(incognito).createNewTab(
                loadUrlParams, type, parent);
    }

    /**
     * Merge tabs from {@link BrowserActionsTabModelSelector} to the selector in a {@link
     * ChromeActivity}.
     * @param activity The ChromeActivity all tabs will be merged to.
     * @param shouldSelectTab Whether the last tab should be set as the active tab.
     */
    public void mergeBrowserActionsTabModel(ChromeActivity activity, boolean shouldSelectTab) {
        assert isInitialized() : "Browser Actions tab model should be initialized at this point";
        TabModel chromeNormalTabModel = activity.getTabModelSelector().getModel(false);
        TabModel browserActionsNormlTabModel = getModel(false);
        while (browserActionsNormlTabModel.getCount() > 0) {
            Tab tab = browserActionsNormlTabModel.getTabAt(0);
            browserActionsNormlTabModel.removeTab(tab);
            tab.attach(activity,
                    ((ChromeTabCreator) activity.getTabCreator(false))
                            .createDefaultTabDelegateFactory());
            chromeNormalTabModel.addTab(tab, -1, TabLaunchType.FROM_BROWSER_ACTIONS);
        }
        saveState();
        if (shouldSelectTab) {
            TabModelUtils.setIndex(chromeNormalTabModel, chromeNormalTabModel.getCount() - 1);
        }
    }

    @Override
    public void destroy() {
        super.destroy();
        mTabSaver.destroy();
        sInstance = null;
    }

    /**
     * Add a {@link TabPersistentStoreObserver} to {@link TabPersistentStore}.
     * @param observer The observer to add.
     */
    public void addTabPersistentStoreObserver(TabPersistentStoreObserver observer) {
        mTabSaver.addObserver(observer);
    }

    /**
     * Remove a {@link TabPersistentStoreObserver} from {@link TabPersistentStore}.
     * @param observer The observer to remove.
     */
    public void removeTabPersistentStoreObserver(TabPersistentStoreObserver observer) {
        mTabSaver.removeObserver(observer);
    }
}

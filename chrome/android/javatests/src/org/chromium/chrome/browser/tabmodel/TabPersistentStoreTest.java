// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import android.app.Activity;
import android.content.SharedPreferences;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.SmallTest;
import android.util.Pair;
import android.util.SparseArray;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ActivityState;
import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.task.AsyncTask;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.AdvancedMockContext;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.TabState;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutHelper;
import org.chromium.chrome.browser.fullscreen.ChromeFullscreenManager;
import org.chromium.chrome.browser.snackbar.undo.UndoBarController;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager.TabCreator;
import org.chromium.chrome.browser.tabmodel.TabModel.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabModel.TabSelectionType;
import org.chromium.chrome.browser.tabmodel.TabPersistentStore.TabModelSelectorMetadata;
import org.chromium.chrome.browser.tabmodel.TabPersistentStore.TabPersistentStoreObserver;
import org.chromium.chrome.browser.tabmodel.TestTabModelDirectory.TabModelMetaDataInfo;
import org.chromium.chrome.browser.test.ChromeBrowserTestRule;
import org.chromium.chrome.browser.widget.OverviewListLayout;
import org.chromium.chrome.test.util.browser.tabmodel.MockTabModelSelector;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.Callable;

/** Tests for the TabPersistentStore. */
@RunWith(BaseJUnit4ClassRunner.class)
public class TabPersistentStoreTest {
    @Rule
    public final ChromeBrowserTestRule mBrowserTestRule = new ChromeBrowserTestRule();

    private ChromeActivity mChromeActivity;

    private static final int SELECTOR_INDEX = 0;

    private static class TabRestoredDetails {
        public int index;
        public int id;
        public String url;
        public boolean isStandardActiveIndex;
        public boolean isIncognitoActiveIndex;

        /** Store information about a Tab that's been restored. */
        TabRestoredDetails(int index, int id, String url,
                boolean isStandardActiveIndex, boolean isIncognitoActiveIndex) {
            this.index = index;
            this.id = id;
            this.url = url;
            this.isStandardActiveIndex = isStandardActiveIndex;
            this.isIncognitoActiveIndex = isIncognitoActiveIndex;
        }
    }

    private static class MockTabCreator extends TabCreator {
        public final SparseArray<TabState> created;
        public final CallbackHelper callback;

        private final boolean mIsIncognito;
        private final TabModelSelector mSelector;

        public int idOfFirstCreatedTab = Tab.INVALID_TAB_ID;

        public MockTabCreator(boolean incognito, TabModelSelector selector) {
            created = new SparseArray<>();
            callback = new CallbackHelper();
            mIsIncognito = incognito;
            mSelector = selector;
        }

        @Override
        public boolean createsTabsAsynchronously() {
            return false;
        }

        @Override
        public Tab createNewTab(
                LoadUrlParams loadUrlParams, @TabModel.TabLaunchType int type, Tab parent) {
            Tab tab = Tab.createTabForLazyLoad(
                    mIsIncognito, null, TabLaunchType.FROM_LINK, Tab.INVALID_TAB_ID, loadUrlParams);
            mSelector.getModel(mIsIncognito).addTab(tab, TabModel.INVALID_TAB_INDEX, type);
            storeTabInfo(null, tab.getId());
            return tab;
        }

        @Override
        public Tab createFrozenTab(TabState state, int id, int index) {
            Tab tab = Tab.createFrozenTabFromState(
                    id, state.isIncognito(), null, state.parentId, state);
            mSelector.getModel(mIsIncognito).addTab(tab, index, TabLaunchType.FROM_RESTORE);
            storeTabInfo(state, id);
            return tab;
        }

        @Override
        public boolean createTabWithWebContents(Tab parent, WebContents webContents, int parentId,
                @TabLaunchType int type, String url) {
            return false;
        }

        @Override
        public Tab launchUrl(String url, @TabModel.TabLaunchType int type) {
            return null;
        }

        private void storeTabInfo(TabState state, int id) {
            if (created.size() == 0) idOfFirstCreatedTab = id;
            created.put(id, state);
            callback.notifyCalled();
        }
    }

    private static class MockTabCreatorManager implements TabCreatorManager {
        private MockTabCreator mRegularCreator;
        private MockTabCreator mIncognitoCreator;

        public MockTabCreatorManager(TabModelSelector selector) {
            mRegularCreator = new MockTabCreator(false, selector);
            mIncognitoCreator = new MockTabCreator(true, selector);
        }

        @Override
        public MockTabCreator getTabCreator(boolean incognito) {
            return incognito ? mIncognitoCreator : mRegularCreator;
        }
    }

    /**
     * Used when testing interactions of TabPersistentStore with real {@link TabModelImpl}s.
     */
    static class TestTabModelSelector extends TabModelSelectorBase
            implements TabModelDelegate {
        final TabPersistentStore mTabPersistentStore;
        final MockTabPersistentStoreObserver mTabPersistentStoreObserver;
        private final MockTabCreatorManager mTabCreatorManager;
        private final TabModelOrderController mTabModelOrderController;

        public TestTabModelSelector() throws Exception {
            mTabCreatorManager = new MockTabCreatorManager(this);
            mTabPersistentStoreObserver = new MockTabPersistentStoreObserver();
            mTabPersistentStore = ThreadUtils.runOnUiThreadBlocking(
                    new Callable<TabPersistentStore>() {
                        @Override
                        public TabPersistentStore call() {
                            TabPersistencePolicy persistencePolicy =
                                    new TabbedModeTabPersistencePolicy(0, true);
                            return new TabPersistentStore(
                                    persistencePolicy, TestTabModelSelector.this,
                                    mTabCreatorManager, mTabPersistentStoreObserver);
                        }
                    });
            mTabModelOrderController = new TabModelOrderController(this);

            Callable<TabModelImpl> callable = new Callable<TabModelImpl>() {
                @Override
                public TabModelImpl call() {
                    return new TabModelImpl(false, false, mTabCreatorManager.getTabCreator(false),
                            mTabCreatorManager.getTabCreator(true), null, mTabModelOrderController,
                            null, mTabPersistentStore, TestTabModelSelector.this, true);
                }
            };
            TabModelImpl regularTabModel = ThreadUtils.runOnUiThreadBlocking(callable);
            TabModel incognitoTabModel = new IncognitoTabModel(
                    new IncognitoTabModelImplCreator(mTabCreatorManager.getTabCreator(false),
                            mTabCreatorManager.getTabCreator(true),
                            null, mTabModelOrderController, null, mTabPersistentStore, this));
            initialize(false, regularTabModel, incognitoTabModel);
        }

        @Override
        public Tab openNewTab(LoadUrlParams loadUrlParams, @TabLaunchType int type, Tab parent,
                boolean incognito) {
            return mTabCreatorManager.getTabCreator(incognito).createNewTab(
                    loadUrlParams, type, parent);
        }

        @Override
        public void requestToShowTab(Tab tab, @TabSelectionType int type) {}

        @Override
        public boolean closeAllTabsRequest(boolean incognito) {
            TabModel model = getModel(incognito);
            while (model.getCount() > 0) {
                Tab tabToClose = model.getTabAt(0);
                model.closeTab(tabToClose, false, false, true);
            }
            return true;
        }

        @Override
        public boolean isInOverviewMode() {
            return false;
        }

        @Override
        public boolean isSessionRestoreInProgress() {
            return false;
        }

        @Override
        public boolean isCurrentModel(TabModel model) {
            return false;
        }
    }

    static class MockTabPersistentStoreObserver extends TabPersistentStoreObserver {
        public final CallbackHelper initializedCallback = new CallbackHelper();
        public final CallbackHelper detailsReadCallback = new CallbackHelper();
        public final CallbackHelper stateLoadedCallback = new CallbackHelper();
        public final CallbackHelper stateMergedCallback = new CallbackHelper();
        public final CallbackHelper listWrittenCallback = new CallbackHelper();
        public final ArrayList<TabRestoredDetails> details = new ArrayList<>();

        public int mTabCountAtStartup = -1;

        @Override
        public void onInitialized(int tabCountAtStartup) {
            mTabCountAtStartup = tabCountAtStartup;
            initializedCallback.notifyCalled();
        }

        @Override
        public void onDetailsRead(int index, int id, String url,
                boolean isStandardActiveIndex, boolean isIncognitoActiveIndex) {
            details.add(new TabRestoredDetails(
                    index, id, url, isStandardActiveIndex, isIncognitoActiveIndex));
            detailsReadCallback.notifyCalled();
        }

        @Override
        public void onStateLoaded() {
            stateLoadedCallback.notifyCalled();
        }

        @Override
        public void onStateMerged() {
            stateMergedCallback.notifyCalled();
        }

        @Override
        public void onMetadataSavedAsynchronously(TabModelSelectorMetadata metadata) {
            listWrittenCallback.notifyCalled();
        }
    }

    private final TabWindowManager.TabModelSelectorFactory mMockTabModelSelectorFactory =
            new TabWindowManager.TabModelSelectorFactory() {
                @Override
                public TabModelSelector buildSelector(Activity activity,
                        TabCreatorManager tabCreatorManager, int selectorIndex) {
                    try {
                        return new TestTabModelSelector();
                    } catch (Exception e) {
                        throw new RuntimeException(e);
                    }
                }
            };

    /** Class for mocking out the directory containing all of the TabState files. */
    private TestTabModelDirectory mMockDirectory;
    private AdvancedMockContext mAppContext;
    private SharedPreferences mPreferences;

    @Before
    public void setUp() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mChromeActivity = new ChromeActivity() {
                    @Override
                    protected boolean handleBackPressed() {
                        return false;
                    }

                    @Override
                    protected Pair<? extends TabCreator, ? extends TabCreator> createTabCreators() {
                        return null;
                    }

                    @Override
                    protected TabModelSelector createTabModelSelector() {
                        return null;
                    }

                    @Override
                    protected ChromeFullscreenManager createFullscreenManager() {
                        return null;
                    }
                };
            }
        });

        // Using an AdvancedMockContext allows us to use a fresh in-memory SharedPreference.
        mAppContext = new AdvancedMockContext(InstrumentationRegistry.getInstrumentation()
                                                      .getTargetContext()
                                                      .getApplicationContext());
        ContextUtils.initApplicationContextForTests(mAppContext);
        mPreferences = ContextUtils.getAppSharedPreferences();
        mMockDirectory = new TestTabModelDirectory(
                mAppContext, "TabPersistentStoreTest", Integer.toString(SELECTOR_INDEX));
        TabPersistentStore.setBaseStateDirectoryForTests(mMockDirectory.getBaseDirectory());
    }

    @After
    public void tearDown() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                TabWindowManager.getInstance().onActivityStateChange(
                        mChromeActivity, ActivityState.DESTROYED);
            }
        });
        mMockDirectory.tearDown();
    }

    private TabPersistentStore buildTabPersistentStore(final TabPersistencePolicy persistencePolicy,
            final TabModelSelector modelSelector, final TabCreatorManager creatorManager,
            final TabPersistentStoreObserver observer) {
        return ThreadUtils.runOnUiThreadBlockingNoException(new Callable<TabPersistentStore>() {
            @Override
            public TabPersistentStore call() throws Exception {
                return new TabPersistentStore(persistencePolicy, modelSelector, creatorManager,
                        observer);
            }
        });
    }

    @Test
    @SmallTest
    @Feature("TabPersistentStore")
    public void testBasic() throws Exception {
        TabModelMetaDataInfo info = TestTabModelDirectory.TAB_MODEL_METADATA_V4;
        int numExpectedTabs = info.contents.length;

        mMockDirectory.writeTabModelFiles(info, true);

        // Set up the TabPersistentStore.
        MockTabModelSelector mockSelector = new MockTabModelSelector(0, 0, null);
        MockTabCreatorManager mockManager = new MockTabCreatorManager(mockSelector);
        MockTabCreator regularCreator = mockManager.getTabCreator(false);
        MockTabPersistentStoreObserver mockObserver = new MockTabPersistentStoreObserver();
        TabPersistencePolicy persistencePolicy = new TabbedModeTabPersistencePolicy(0, false);
        final TabPersistentStore store =
                buildTabPersistentStore(persistencePolicy, mockSelector, mockManager, mockObserver);

        // Should not prefetch with no prior active tab preference stored.
        Assert.assertNull(store.mPrefetchActiveTabTask);

        // Make sure the metadata file loads properly and in order.
        store.loadState(false /* ignoreIncognitoFiles */);
        mockObserver.initializedCallback.waitForCallback(0, 1);
        Assert.assertEquals(numExpectedTabs, mockObserver.mTabCountAtStartup);

        mockObserver.detailsReadCallback.waitForCallback(0, numExpectedTabs);
        Assert.assertEquals(numExpectedTabs, mockObserver.details.size());
        for (int i = 0; i < numExpectedTabs; i++) {
            TabRestoredDetails details = mockObserver.details.get(i);
            Assert.assertEquals(i, details.index);
            Assert.assertEquals(info.contents[i].tabId, details.id);
            Assert.assertEquals(info.contents[i].url, details.url);
            Assert.assertEquals(details.id == info.selectedTabId, details.isStandardActiveIndex);
            Assert.assertEquals(false, details.isIncognitoActiveIndex);
        }

        // Restore the TabStates.  The first Tab added should be the most recently selected tab.
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                store.restoreTabs(true);
            }
        });
        regularCreator.callback.waitForCallback(0, 1);
        Assert.assertEquals(info.selectedTabId, regularCreator.idOfFirstCreatedTab);

        // Confirm that all the TabStates were read from storage (i.e. non-null).
        mockObserver.stateLoadedCallback.waitForCallback(0, 1);
        for (int i = 0; i < info.contents.length; i++) {
            int tabId = info.contents[i].tabId;
            Assert.assertNotNull(regularCreator.created.get(tabId));
        }
    }

    @Test
    @SmallTest
    @Feature({"TabPersistentStore"})
    public void testInterruptedButStillRestoresAllTabs() throws Exception {
        TabModelMetaDataInfo info = TestTabModelDirectory.TAB_MODEL_METADATA_V4;
        int numExpectedTabs = info.contents.length;

        mMockDirectory.writeTabModelFiles(info, true);

        // Load up one TabPersistentStore, but don't load up the TabState files.  This prevents the
        // Tabs from being added to the TabModel.
        MockTabModelSelector firstSelector = new MockTabModelSelector(0, 0, null);
        MockTabCreatorManager firstManager = new MockTabCreatorManager(firstSelector);
        MockTabPersistentStoreObserver firstObserver = new MockTabPersistentStoreObserver();
        TabPersistencePolicy firstPersistencePolicy = new TabbedModeTabPersistencePolicy(0, false);
        final TabPersistentStore firstStore = buildTabPersistentStore(
                firstPersistencePolicy, firstSelector, firstManager, firstObserver);
        firstStore.loadState(false /* ignoreIncognitoFiles */);
        firstObserver.initializedCallback.waitForCallback(0, 1);
        Assert.assertEquals(numExpectedTabs, firstObserver.mTabCountAtStartup);
        firstObserver.detailsReadCallback.waitForCallback(0, numExpectedTabs);

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                firstStore.saveState();
            }
        });

        // Prepare a second TabPersistentStore.
        MockTabModelSelector secondSelector = new MockTabModelSelector(0, 0, null);
        MockTabCreatorManager secondManager = new MockTabCreatorManager(secondSelector);
        MockTabCreator secondCreator = secondManager.getTabCreator(false);
        MockTabPersistentStoreObserver secondObserver = new MockTabPersistentStoreObserver();
        TabPersistencePolicy secondPersistencePolicy = new TabbedModeTabPersistencePolicy(0, false);

        final TabPersistentStore secondStore = buildTabPersistentStore(
                secondPersistencePolicy, secondSelector, secondManager, secondObserver);

        // The second TabPersistentStore reads the file written by the first TabPersistentStore.
        // Make sure that all of the Tabs appear in the new one -- even though the new file was
        // written before the first TabPersistentStore loaded any TabState files and added them to
        // the TabModels.
        secondStore.loadState(false /* ignoreIncognitoFiles */);
        secondObserver.initializedCallback.waitForCallback(0, 1);
        Assert.assertEquals(numExpectedTabs, secondObserver.mTabCountAtStartup);

        secondObserver.detailsReadCallback.waitForCallback(0, numExpectedTabs);
        Assert.assertEquals(numExpectedTabs, secondObserver.details.size());
        for (int i = 0; i < numExpectedTabs; i++) {
            TabRestoredDetails details = secondObserver.details.get(i);

            // Find the details for the current Tab ID.
            // TODO(dfalcantara): Revisit this bit when tab ordering is correctly preserved.
            TestTabModelDirectory.TabStateInfo currentInfo = null;
            for (int j = 0; j < numExpectedTabs && currentInfo == null; j++) {
                if (TestTabModelDirectory.TAB_MODEL_METADATA_V4.contents[j].tabId == details.id) {
                    currentInfo = TestTabModelDirectory.TAB_MODEL_METADATA_V4.contents[j];
                }
            }

            // TODO(dfalcantara): This won't be properly set until we have tab ordering preserved.
            // Assert.assertEquals(details.id ==
            // TestTabModelDirectory.TAB_MODEL_METADATA_V4_SELECTED_ID,
            //        details.isStandardActiveIndex);

            Assert.assertEquals(currentInfo.url, details.url);
            Assert.assertEquals(false, details.isIncognitoActiveIndex);
        }

        // Restore all of the TabStates.  Confirm that all the TabStates were read (i.e. non-null).
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                secondStore.restoreTabs(true);
            }
        });

        secondObserver.stateLoadedCallback.waitForCallback(0, 1);
        for (int i = 0; i < numExpectedTabs; i++) {
            int tabId = TestTabModelDirectory.TAB_MODEL_METADATA_V4.contents[i].tabId;
            Assert.assertNotNull(secondCreator.created.get(tabId));
        }
    }

    @Test
    @SmallTest
    @Feature({"TabPersistentStore"})
    public void testMissingTabStateButStillRestoresTab() throws Exception {
        TabModelMetaDataInfo info = TestTabModelDirectory.TAB_MODEL_METADATA_V5;
        int numExpectedTabs = info.contents.length;

        // Write out info for all but the third tab (arbitrarily chosen).
        mMockDirectory.writeTabModelFiles(info, false);
        for (int i = 0; i < info.contents.length; i++) {
            if (i != 2) mMockDirectory.writeTabStateFile(info.contents[i]);
        }

        // Initialize the classes.
        MockTabModelSelector mockSelector = new MockTabModelSelector(0, 0, null);
        MockTabCreatorManager mockManager = new MockTabCreatorManager(mockSelector);
        MockTabPersistentStoreObserver mockObserver = new MockTabPersistentStoreObserver();
        TabPersistencePolicy persistencePolicy = new TabbedModeTabPersistencePolicy(0, false);
        final TabPersistentStore store =
                buildTabPersistentStore(persistencePolicy, mockSelector, mockManager, mockObserver);

        // Make sure the metadata file loads properly and in order.
        store.loadState(false /* ignoreIncognitoFiles */);
        mockObserver.initializedCallback.waitForCallback(0, 1);
        Assert.assertEquals(numExpectedTabs, mockObserver.mTabCountAtStartup);

        mockObserver.detailsReadCallback.waitForCallback(0, numExpectedTabs);
        Assert.assertEquals(numExpectedTabs, mockObserver.details.size());
        for (int i = 0; i < numExpectedTabs; i++) {
            TabRestoredDetails details = mockObserver.details.get(i);
            Assert.assertEquals(i, details.index);
            Assert.assertEquals(info.contents[i].tabId, details.id);
            Assert.assertEquals(info.contents[i].url, details.url);
            Assert.assertEquals(details.id == info.selectedTabId, details.isStandardActiveIndex);
            Assert.assertEquals(false, details.isIncognitoActiveIndex);
        }

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                // Restore the TabStates, and confirm that the correct number of tabs is created
                // even with one missing.
                store.restoreTabs(true);
            }
        });
        mockObserver.stateLoadedCallback.waitForCallback(0, 1);
        Assert.assertEquals(numExpectedTabs, mockSelector.getModel(false).getCount());
        Assert.assertEquals(0, mockSelector.getModel(true).getCount());
    }

    @Test
    @SmallTest
    @Feature({"TabPersistentStore"})
    public void testRestoresTabWithMissingTabStateWhileIgnoringIncognitoTab() throws Exception {
        TabModelMetaDataInfo info = TestTabModelDirectory.TAB_MODEL_METADATA_V5_WITH_INCOGNITO;
        int numExpectedTabs = info.contents.length;

        // Write out info for all but the third tab (arbitrarily chosen).
        mMockDirectory.writeTabModelFiles(info, false);
        for (int i = 0; i < info.contents.length; i++) {
            if (i != 2) mMockDirectory.writeTabStateFile(info.contents[i]);
        }

        // Initialize the classes.
        MockTabModelSelector mockSelector = new MockTabModelSelector(0, 0, null);
        MockTabCreatorManager mockManager = new MockTabCreatorManager(mockSelector);
        MockTabPersistentStoreObserver mockObserver = new MockTabPersistentStoreObserver();
        TabPersistencePolicy persistencePolicy = new TabbedModeTabPersistencePolicy(0, false);
        final TabPersistentStore store =
                buildTabPersistentStore(persistencePolicy, mockSelector, mockManager, mockObserver);

        // Load the TabModel metadata.
        store.loadState(false /* ignoreIncognitoFiles */);
        mockObserver.initializedCallback.waitForCallback(0, 1);
        Assert.assertEquals(numExpectedTabs, mockObserver.mTabCountAtStartup);
        mockObserver.detailsReadCallback.waitForCallback(0, numExpectedTabs);
        Assert.assertEquals(numExpectedTabs, mockObserver.details.size());

        // TODO(dfalcantara): Expand MockTabModel* to support Incognito Tab decryption.

        // Restore the TabStates, and confirm that the correct number of tabs is created even with
        // one missing.  No Incognito tabs should be created because the TabState is missing.
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                store.restoreTabs(true);
            }
        });
        mockObserver.stateLoadedCallback.waitForCallback(0, 1);
        Assert.assertEquals(info.numRegularTabs, mockSelector.getModel(false).getCount());
        Assert.assertEquals(0, mockSelector.getModel(true).getCount());
    }

    @Test
    @SmallTest
    @Feature({"TabPersistentStore"})
    public void testPrefetchActiveTab() throws Exception {
        final TabModelMetaDataInfo info = TestTabModelDirectory.TAB_MODEL_METADATA_V5_NO_M18;
        mMockDirectory.writeTabModelFiles(info, true);

        // Set to pre-fetch
        mPreferences.edit().putInt(
                TabPersistentStore.PREF_ACTIVE_TAB_ID, info.selectedTabId).apply();

        // Initialize the classes.
        MockTabModelSelector mockSelector = new MockTabModelSelector(0, 0, null);
        MockTabCreatorManager mockManager = new MockTabCreatorManager(mockSelector);
        MockTabPersistentStoreObserver mockObserver = new MockTabPersistentStoreObserver();
        TabPersistencePolicy persistencePolicy = new TabbedModeTabPersistencePolicy(0, false);
        final TabPersistentStore store = buildTabPersistentStore(
                persistencePolicy, mockSelector, mockManager, mockObserver);
        store.waitForMigrationToFinish();

        Assert.assertNotNull(store.mPrefetchActiveTabTask);

        store.loadState(false /* ignoreIncognitoFiles */);
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                store.restoreTabs(true);
            }
        });

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                // Confirm that the pre-fetched active tab state was used, must be done here on the
                // UI thread as the message to finish the task is posted here.
                Assert.assertEquals(
                        AsyncTask.Status.FINISHED, store.mPrefetchActiveTabTask.getStatus());

                // Confirm that the correct active tab ID is updated when saving state.
                mPreferences.edit().putInt(TabPersistentStore.PREF_ACTIVE_TAB_ID, -1).apply();

                store.saveState();
            }
        });

        Assert.assertEquals(
                info.selectedTabId, mPreferences.getInt(TabPersistentStore.PREF_ACTIVE_TAB_ID, -1));
    }

    /**
     * Tests that a real {@link TabModelImpl} will use the {@link TabPersistentStore} to write out
     * an updated metadata file when a closure is undone.
     */
    @Test
    @SmallTest
    @Feature({"TabPersistentStore"})
    public void testUndoSingleTabClosureWritesTabListFile() throws Exception {
        TabModelMetaDataInfo info = TestTabModelDirectory.TAB_MODEL_METADATA_V5_NO_M18;
        mMockDirectory.writeTabModelFiles(info, true);

        // Start closing one tab, then undo it.  Make sure the tab list metadata is saved out.
        TestTabModelSelector selector = createAndRestoreRealTabModelImpls(info);
        MockTabPersistentStoreObserver mockObserver = selector.mTabPersistentStoreObserver;
        final TabModel regularModel = selector.getModel(false);
        int currentWrittenCallbackCount = mockObserver.listWrittenCallback.getCallCount();
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                Tab tabToClose = regularModel.getTabAt(2);
                regularModel.closeTab(tabToClose, false, false, true);
                regularModel.cancelTabClosure(tabToClose.getId());
            }
        });
        mockObserver.listWrittenCallback.waitForCallback(currentWrittenCallbackCount, 1);
    }

    /**
     * Tests that a real {@link TabModelImpl} will use the {@link TabPersistentStore} to write out
     * valid a valid metadata file and the TabModel's associated TabStates after closing and
     * canceling the closure of all the tabs simultaneously.
     */
    @Test
    @SmallTest
    @Feature({"TabPersistentStore"})
    @RetryOnFailure
    public void testUndoCloseAllTabsWritesTabListFile() throws Exception {
        final TabModelMetaDataInfo info = TestTabModelDirectory.TAB_MODEL_METADATA_V5_NO_M18;
        mMockDirectory.writeTabModelFiles(info, true);

        for (int i = 0; i < 2; i++) {
            final TestTabModelSelector selector = createAndRestoreRealTabModelImpls(info);

            // Undoing tab closures one-by-one results in the first tab always being selected after
            // the initial restoration.
            if (i == 0) {
                Assert.assertEquals(info.selectedTabId, selector.getCurrentTab().getId());
            } else {
                Assert.assertEquals(info.contents[0].tabId, selector.getCurrentTab().getId());
            }

            ThreadUtils.runOnUiThreadBlocking(new Runnable() {
                @Override
                public void run() {
                    closeAllTabsThenUndo(selector, info);

                    // Synchronously save the data out to simulate minimizing Chrome.
                    selector.mTabPersistentStore.saveState();
                }
            });

            // Load up each TabState and confirm that values are still correct.
            for (int j = 0; j < info.numRegularTabs; j++) {
                TabState currentState = TabState.restoreTabState(
                        mMockDirectory.getDataDirectory(), info.contents[j].tabId);
                Assert.assertEquals(
                        info.contents[j].title, currentState.getDisplayTitleFromState());
                Assert.assertEquals(info.contents[j].url, currentState.getVirtualUrlFromState());
            }
        }
    }

    @Test
    @SmallTest
    @Feature({"TabPersistentStore", "MultiWindow"})
    @MinAndroidSdkLevel(24)
    public void testDuplicateTabIdsOnColdStart() throws Exception {
        final TabModelMetaDataInfo info = TestTabModelDirectory.TAB_MODEL_METADATA_V5_NO_M18;

        // Write the same data to tab_state0 and tab_state1.
        mMockDirectory.writeTabModelFiles(info, true, 0);
        mMockDirectory.writeTabModelFiles(info, true, 1);

        // This method will check that the correct number of tabs are created.
        createAndRestoreRealTabModelImpls(info);
    }

    private TestTabModelSelector createAndRestoreRealTabModelImpls(TabModelMetaDataInfo info)
            throws Exception {
        TestTabModelSelector selector =
                ThreadUtils.runOnUiThreadBlocking(new Callable<TestTabModelSelector>() {
                    @Override
                    public TestTabModelSelector call() {
                        TabWindowManager tabWindowManager = TabWindowManager.getInstance();
                        tabWindowManager.setTabModelSelectorFactory(mMockTabModelSelectorFactory);
                        // Clear any existing TestTabModelSelector (required when
                        // createAndRestoreRealTabModelImpls is called multiple times in one test).
                        tabWindowManager.onActivityStateChange(
                                mChromeActivity, ActivityState.DESTROYED);
                        return (TestTabModelSelector) tabWindowManager.requestSelector(
                                mChromeActivity, mChromeActivity, 0);
                    }
                });

        final TabPersistentStore store = selector.mTabPersistentStore;
        MockTabPersistentStoreObserver mockObserver = selector.mTabPersistentStoreObserver;

        // Load up the TabModel metadata.
        int numExpectedTabs = info.numRegularTabs + info.numIncognitoTabs;
        store.loadState(false /* ignoreIncognitoFiles */);
        mockObserver.initializedCallback.waitForCallback(0, 1);
        Assert.assertEquals(numExpectedTabs, mockObserver.mTabCountAtStartup);
        mockObserver.detailsReadCallback.waitForCallback(0, info.contents.length);
        Assert.assertEquals(numExpectedTabs, mockObserver.details.size());

        // Restore the TabStates, check that things were restored correctly, in the right tab order.
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                store.restoreTabs(true);
            }
        });
        mockObserver.stateLoadedCallback.waitForCallback(0, 1);
        Assert.assertEquals(info.numRegularTabs, selector.getModel(false).getCount());
        Assert.assertEquals(info.numIncognitoTabs, selector.getModel(true).getCount());
        for (int i = 0; i < numExpectedTabs; i++) {
            Assert.assertEquals(
                    info.contents[i].tabId, selector.getModel(false).getTabAt(i).getId());
        }

        return selector;
    }

    /**
     * Close all Tabs in the regular TabModel, then undo the operation to restore the Tabs.
     * This simulates how {@link StripLayoutHelper} and {@link UndoBarController} would close
     * all of a {@link TabModel}'s tabs on tablets, which is different from how the
     * {@link OverviewListLayout} would do it on phones.
     */
    private void closeAllTabsThenUndo(TabModelSelector selector, TabModelMetaDataInfo info) {
        // Close all the tabs, using an Observer to determine what is actually being closed.
        TabModel regularModel = selector.getModel(false);
        final List<Integer> closedTabIds = new ArrayList<>();
        TabModelObserver closeObserver = new EmptyTabModelObserver() {
            @Override
            public void allTabsPendingClosure(List<Tab> tabs) {
                for (Tab tab : tabs) closedTabIds.add(tab.getId());
            }
        };
        regularModel.addObserver(closeObserver);
        regularModel.closeAllTabs(false, false);
        Assert.assertEquals(info.numRegularTabs, closedTabIds.size());

        // Cancel closing each tab.
        for (Integer id : closedTabIds) regularModel.cancelTabClosure(id);
        Assert.assertEquals(info.numRegularTabs, regularModel.getCount());
    }
}

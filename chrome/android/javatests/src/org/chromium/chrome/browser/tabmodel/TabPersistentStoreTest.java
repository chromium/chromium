// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import android.content.Context;
import android.os.Looper;
import android.util.Pair;

import androidx.test.filters.SmallTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.After;
import org.junit.AfterClass;
import org.junit.Assert;
import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ApplicationStatus.ActivityStateListener;
import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.base.task.AsyncTask;
import org.chromium.base.test.util.AdvancedMockContext;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.Matchers;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.app.metrics.LaunchCauseMetrics;
import org.chromium.chrome.browser.app.tabmodel.AsyncTabParamsManagerSingleton;
import org.chromium.chrome.browser.app.tabmodel.TabModelOrchestrator;
import org.chromium.chrome.browser.app.tabmodel.TabWindowManagerSingleton;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutHelper;
import org.chromium.chrome.browser.crypto.CipherFactory;
import org.chromium.chrome.browser.flags.ActivityType;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.init.ActivityProfileProvider;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.profiles.ProfileProvider;
import org.chromium.chrome.browser.tab.MockTab;
import org.chromium.chrome.browser.tab.MockTabAttributes;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tab.TabState;
import org.chromium.chrome.browser.tab.TabStateAttributes;
import org.chromium.chrome.browser.tab.TabStateExtractor;
import org.chromium.chrome.browser.tab.WebContentsState;
import org.chromium.chrome.browser.tab.state.ShoppingPersistedTabData;
import org.chromium.chrome.browser.tab_ui.TabContentManager;
import org.chromium.chrome.browser.tabmodel.NextTabPolicy.NextTabPolicySupplier;
import org.chromium.chrome.browser.tabmodel.TabPersistentStore.TabModelSelectorMetadata;
import org.chromium.chrome.browser.tabmodel.TabPersistentStore.TabPersistentStoreObserver;
import org.chromium.chrome.browser.tabmodel.TabPersistentStore.TabRestoreDetails;
import org.chromium.chrome.browser.tabmodel.TabPersistentStore.TabRestoreMethod;
import org.chromium.chrome.browser.tabmodel.TestTabModelDirectory.TabModelMetaDataInfo;
import org.chromium.chrome.browser.tabmodel.TestTabModelDirectory.TabStateInfo;
import org.chromium.chrome.browser.tabpersistence.TabStateDirectory;
import org.chromium.chrome.browser.tabpersistence.TabStateFileManager;
import org.chromium.chrome.browser.ui.RootUiCoordinator;
import org.chromium.chrome.test.ChromeBrowserTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.browser.tabmodel.MockTabCreator;
import org.chromium.chrome.test.util.browser.tabmodel.MockTabCreatorManager;
import org.chromium.chrome.test.util.browser.tabmodel.MockTabModelSelector;

import java.io.File;
import java.nio.ByteBuffer;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.concurrent.Callable;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeoutException;
import java.util.stream.Collectors;

/** Tests for the TabPersistentStore. */

// TODO(crbug.com/40167624) reintroduce batching - batching was removed because introducing
// parameterized tests caused cross-talk between tests.

@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({
    ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
    "force-fieldtrials=Study/Group"
})
@DisableFeatures(ChromeFeatureList.ANDROID_TAB_DECLUTTER_RESCUE_KILLSWITCH)
public class TabPersistentStoreTest {
    // Test activity type that does not restore tab on cold restart.
    // Any type other than ActivityType.TABBED works.
    private static final @ActivityType int NO_RESTORE_TYPE = ActivityType.CUSTOM_TAB;

    @Rule public final ChromeBrowserTestRule mBrowserTestRule = new ChromeBrowserTestRule();

    private ChromeActivity mChromeActivity;

    private static final int SELECTOR_INDEX = 0;

    private static final int PREV_ROOT_ID = 32;
    private static final int NEW_ROOT_ID = 42;

    private static class TabRestoredDetails {
        public int index;
        public int id;
        public String url;
        public boolean isStandardActiveIndex;
        public boolean isIncognitoActiveIndex;
        public Boolean isIncognito;

        /** Store information about a Tab that's been restored. */
        TabRestoredDetails(
                int index,
                int id,
                String url,
                boolean isStandardActiveIndex,
                boolean isIncognitoActiveIndex,
                Boolean isIncognito) {
            this.index = index;
            this.id = id;
            this.url = url;
            this.isStandardActiveIndex = isStandardActiveIndex;
            this.isIncognitoActiveIndex = isIncognitoActiveIndex;
            this.isIncognito = isIncognito;
        }
    }

    /** Used when testing interactions of TabPersistentStore with real {@link TabModelImpl}s. */
    static class TestTabModelSelector extends TabModelSelectorBase implements TabModelDelegate {
        final TabPersistentStore mTabPersistentStore;
        final MockTabPersistentStoreObserver mTabPersistentStoreObserver;
        private final TabModelOrderController mTabModelOrderController;
        // Required to ensure TabContentManager is not null.
        @Mock // Annotation required to disable R8 optimization of the class.
        private final TabContentManager mMockTabContentManager;

        public TestTabModelSelector(Context context) throws Exception {
            super(new MockTabCreatorManager(), false);
            ((MockTabCreatorManager) getTabCreatorManager()).initialize(this);
            mTabPersistentStoreObserver = new MockTabPersistentStoreObserver();
            // Use of a mockito object here is ok as this object is not important to the test. A
            // real object is not available from {@link ChromeActivity} due to the test structure.
            // {@link TabModelImpl} requires a non-null {@link TabContentManager} to initialize.
            mMockTabContentManager = Mockito.mock(TabContentManager.class);
            mTabPersistentStore =
                    ThreadUtils.runOnUiThreadBlocking(
                            new Callable<TabPersistentStore>() {
                                @Override
                                public TabPersistentStore call() {
                                    TabPersistencePolicy persistencePolicy =
                                            createTabPersistencePolicy(0, true, true);
                                    persistencePolicy.setTabContentManager(mMockTabContentManager);
                                    TabPersistentStore tabPersistentStore =
                                            new TabPersistentStore(
                                                    TabPersistentStore.CLIENT_TAG_REGULAR,
                                                    persistencePolicy,
                                                    TestTabModelSelector.this,
                                                    getTabCreatorManager(),
                                                    TabWindowManagerSingleton.getInstance(),
                                                    sCipherFactory);
                                    tabPersistentStore.addObserver(mTabPersistentStoreObserver);
                                    return tabPersistentStore;
                                }
                            });
            mTabModelOrderController = new TabModelOrderControllerImpl(this);
            NextTabPolicySupplier nextTabPolicySupplier = () -> NextTabPolicy.HIERARCHICAL;

            Callable<TabModelImpl> callable =
                    new Callable<TabModelImpl>() {
                        @Override
                        public TabModelImpl call() {
                            return new TabModelImpl(
                                    ProfileManager.getLastUsedRegularProfile(),
                                    NO_RESTORE_TYPE,
                                    getTabCreatorManager().getTabCreator(false),
                                    getTabCreatorManager().getTabCreator(true),
                                    mTabModelOrderController,
                                    mMockTabContentManager,
                                    nextTabPolicySupplier,
                                    AsyncTabParamsManagerSingleton.getInstance(),
                                    TestTabModelSelector.this,
                                    /* supportUndo= */ true,
                                    /* trackInNativeModelList= */ true);
                        }
                    };
            TabModelImpl regularTabModel = ThreadUtils.runOnUiThreadBlocking(callable);
            IncognitoTabModelImpl incognitoTabModel =
                    new IncognitoTabModelImpl(
                            new IncognitoTabModelImplCreator(
                                    null,
                                    getTabCreatorManager().getTabCreator(false),
                                    getTabCreatorManager().getTabCreator(true),
                                    mTabModelOrderController,
                                    null,
                                    nextTabPolicySupplier,
                                    AsyncTabParamsManagerSingleton.getInstance(),
                                    NO_RESTORE_TYPE,
                                    this));
            initialize(regularTabModel, incognitoTabModel);
        }

        @Override
        public void requestToShowTab(Tab tab, @TabSelectionType int type) {}

        @Override
        public boolean isSessionRestoreInProgress() {
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
        public void onDetailsRead(
                int index,
                int id,
                String url,
                boolean isStandardActiveIndex,
                boolean isIncognitoActiveIndex,
                Boolean isIncognito,
                boolean fromMerge) {
            details.add(
                    new TabRestoredDetails(
                            index,
                            id,
                            url,
                            isStandardActiveIndex,
                            isIncognitoActiveIndex,
                            isIncognito));
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

    private static final TabModelSelectorFactory sMockTabModelSelectorFactory =
            new TabModelSelectorFactory() {
                @Override
                public TabModelSelector buildSelector(
                        Context context,
                        OneshotSupplier<ProfileProvider> profileProviderSupplier,
                        TabCreatorManager tabCreatorManager,
                        NextTabPolicySupplier nextTabPolicySupplier) {
                    try {
                        return new TestTabModelSelector(context);
                    } catch (Exception e) {
                        throw new RuntimeException(e);
                    }
                }
            };
    private static TabWindowManagerImpl sTabWindowManager;
    private static CipherFactory sCipherFactory;

    /** Class for mocking out the directory containing all of the TabState files. */
    private TestTabModelDirectory mMockDirectory;

    private AdvancedMockContext mAppContext;
    private SharedPreferencesManager mPreferences;

    // This is used to pretend we've started the activity, so we can attach a base context to the
    // activity.
    private final ActivityStateListener mActivityStateListener =
            (activity, state) -> {
                if (state == ActivityState.STARTED) {
                    mChromeActivity.onStart();
                }
            };

    @BeforeClass
    public static void beforeClassSetUp() {
        // Required for parameterized tests - otherwise we will fail
        // assert sInstance == null in setTabModelSelectorFactoryForTesting
        TabWindowManagerSingleton.resetTabModelSelectorFactoryForTesting();
        TabWindowManagerSingleton.setTabModelSelectorFactoryForTesting(
                sMockTabModelSelectorFactory);

        sCipherFactory = new CipherFactory();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    sTabWindowManager =
                            (TabWindowManagerImpl) TabWindowManagerSingleton.getInstance();
                });
    }

    @AfterClass
    public static void afterClassTearDown() {
        TabWindowManagerSingleton.resetTabModelSelectorFactoryForTesting();
    }

    @Before
    public void setUp() {
        TabPersistentStore.resetDeferredStartupCompleteForTesting();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mChromeActivity =
                            new ChromeActivity() {
                                @Override
                                protected boolean handleBackPressed() {
                                    return false;
                                }

                                @Override
                                protected Pair<? extends TabCreator, ? extends TabCreator>
                                        createTabCreators() {
                                    return null;
                                }

                                @Override
                                protected TabModelOrchestrator createTabModelOrchestrator() {
                                    return null;
                                }

                                @Override
                                protected void createTabModels() {}

                                @Override
                                protected void destroyTabModels() {}

                                @Override
                                protected LaunchCauseMetrics createLaunchCauseMetrics() {
                                    return null;
                                }

                                @Override
                                public @ActivityType int getActivityType() {
                                    return ActivityType.TABBED;
                                }

                                // This is intended to pretend we've started the activity, so we can
                                // attach a base context to the activity.
                                @Override
                                public void onStart() {
                                    if (getBaseContext() == null) {
                                        attachBaseContext(mAppContext);
                                    }
                                }

                                @Override
                                protected OneshotSupplier<ProfileProvider> createProfileProvider() {
                                    throw new IllegalStateException();
                                }

                                @Override
                                protected RootUiCoordinator createRootUiCoordinator() {
                                    return null;
                                }
                            };
                    ApplicationStatus.onStateChangeForTesting(
                            mChromeActivity, ActivityState.CREATED);
                });

        // Using an AdvancedMockContext allows us to use a fresh in-memory SharedPreference.
        mAppContext =
                new AdvancedMockContext(
                        InstrumentationRegistry.getInstrumentation()
                                .getTargetContext()
                                .getApplicationContext());
        ContextUtils.initApplicationContextForTests(mAppContext);
        mPreferences = ChromeSharedPreferences.getInstance();
        mMockDirectory =
                new TestTabModelDirectory(
                        mAppContext, "TabPersistentStoreTest", Integer.toString(SELECTOR_INDEX));
        TabStateDirectory.setBaseStateDirectoryForTests(mMockDirectory.getBaseDirectory());

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ApplicationStatus.registerStateListenerForActivity(
                            mActivityStateListener, mChromeActivity);
                });
    }

    @After
    public void tearDown() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    sTabWindowManager.onActivityStateChange(
                            mChromeActivity, ActivityState.DESTROYED);
                    ApplicationStatus.onStateChangeForTesting(
                            mChromeActivity, ActivityState.DESTROYED);
                    ApplicationStatus.unregisterActivityStateListener(mActivityStateListener);
                });
        mMockDirectory.tearDown();
    }

    private TabPersistentStore buildTabPersistentStore(
            final TabPersistencePolicy persistencePolicy,
            final TabModelSelector modelSelector,
            final TabCreatorManager creatorManager) {
        return ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    return new TabPersistentStore(
                            TabPersistentStore.CLIENT_TAG_REGULAR,
                            persistencePolicy,
                            modelSelector,
                            creatorManager,
                            TabWindowManagerSingleton.getInstance(),
                            sCipherFactory);
                });
    }

    private static TabbedModeTabPersistencePolicy createTabPersistencePolicy(
            int selectorIndex, boolean mergeTabs, boolean tabMergingEnabled) {
        return ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    return new TabbedModeTabPersistencePolicy(
                            selectorIndex, mergeTabs, tabMergingEnabled);
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
        MockTabModelSelector mockSelector =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            Profile profile = ProfileManager.getLastUsedRegularProfile();
                            return new MockTabModelSelector(
                                    profile, profile.getPrimaryOTRProfile(true), 0, 0, null);
                        });

        MockTabCreatorManager mockManager = new MockTabCreatorManager(mockSelector);
        MockTabCreator regularCreator = mockManager.getTabCreator(false);
        MockTabPersistentStoreObserver mockObserver = new MockTabPersistentStoreObserver();
        TabPersistencePolicy persistencePolicy = createTabPersistencePolicy(0, false, true);
        final TabPersistentStore store =
                buildTabPersistentStore(persistencePolicy, mockSelector, mockManager);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    store.addObserver(mockObserver);

                    // Should not prefetch with no prior active tab preference stored.
                    Assert.assertNull(store.getPrefetchTabStateActiveTabTaskForTesting());

                    // Make sure the metadata file loads properly and in order.
                    store.loadState(/* ignoreIncognitoFiles= */ false);
                });

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
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    store.restoreTabs(true);
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
    @Feature("TabPersistentStore")
    @EnableFeatures({ChromeFeatureList.TAB_STATE_FLAT_BUFFER + "<Study"})
    @CommandLineFlags.Add({
        "force-fieldtrials=Study/Group",
        "force-fieldtrial-params=Study.Group:migrate_stale_tabs/true"
    })
    public void testFlatBufferMigration() throws Exception {
        Pair<TabPersistentStore, Tab[]> storeAndRestoredTabs = createStoreAndRestoreTabs();
        TabPersistentStore store = storeAndRestoredTabs.first;
        Tab[] tabs = storeAndRestoredTabs.second;
        waitForTabStateCleanup(tabs);

        // All Tabs should be a candidate for the FlatBuffer migration as they were restored
        // using legacy TabState.
        Assert.assertEquals(tabs.length, store.getTabsToMigrateForTesting().size());
        TabPersistentStore.onDeferredStartup();
        setAllTabStatesForTesting(tabs);

        CallbackHelper helper = new CallbackHelper();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    // Saving 1 Tab will trigger 5 migrations.
                    store.addTabToSaveQueue(tabs[0]);
                    store.saveNextTab();
                    helper.notifyCalled();
                });
        helper.waitForCallback(0);

        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            store.getTabsToMigrateForTesting().size(),
                            Matchers.is(tabs.length - TabPersistentStore.sMaxMigrationsPerSave));
                    Criteria.checkThat(store.getMigrateTabTaskForTesting(), Matchers.nullValue());
                });
        // First 5 (= sMaxMigrationsPerSave) Tabs should be migrated.
        for (Tab tab :
                Arrays.stream(tabs)
                        .limit(TabPersistentStore.sMaxMigrationsPerSave)
                        .collect(Collectors.toList())) {
            File flatBufferFile =
                    TabStateFileManager.getTabStateFile(
                            mMockDirectory.getDataDirectory(),
                            /* tabId= */ tab.getId(),
                            /* encrypted= */ false,
                            /* isFlatBuffer= */ true);
            Assert.assertTrue(
                    "FlatBuffer file should exist " + flatBufferFile, flatBufferFile.exists());
        }
    }

    private void waitForTabStateCleanup(Tab[] tabs) {
        // Wait for legacy TabState files to be cleaned up. This cleanup path covers FlatBuffer
        // files as well so if we're not careful we'll have a race condition where a FlatBuffer file
        // is deleted after it's created and before it's verified.
        for (final Tab tab : tabs) {
            CriteriaHelper.pollInstrumentationThread(
                    () -> {
                        File legacyTabStateFile =
                                TabStateFileManager.getTabStateFile(
                                        mMockDirectory.getDataDirectory(),
                                        /* tabId= */ tab.getId(),
                                        /* encrypted= */ tab.isIncognito(),
                                        /* isFlatBuffer= */ false);
                        Criteria.checkThat(legacyTabStateFile.exists(), Matchers.is(false));
                    });
        }
    }

    private Pair<TabPersistentStore, Tab[]> createStoreAndRestoreTabs() throws Exception {
        TabModelMetaDataInfo info = TestTabModelDirectory.TAB_MODEL_METADATA_V4;
        int numExpectedTabs = info.contents.length;
        mMockDirectory.writeTabModelFiles(info, true);
        MockTabModelSelector mockSelector =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            Profile profile = ProfileManager.getLastUsedRegularProfile();
                            return new MockTabModelSelector(
                                    profile, profile.getPrimaryOTRProfile(true), 0, 0, null);
                        });
        MockTabCreatorManager mockManager = new MockTabCreatorManager(mockSelector);
        MockTabCreator regularCreator = mockManager.getTabCreator(false);
        MockTabPersistentStoreObserver mockObserver = new MockTabPersistentStoreObserver();
        TabPersistencePolicy persistencePolicy = createTabPersistencePolicy(0, false, true);
        final TabPersistentStore store =
                buildTabPersistentStore(persistencePolicy, mockSelector, mockManager);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    store.addObserver(mockObserver);
                    store.loadState(/* ignoreIncognitoFiles */ false);
                });
        mockObserver.initializedCallback.waitForCallback(0, 1);
        mockObserver.detailsReadCallback.waitForCallback(0, numExpectedTabs);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    store.restoreTabs(true);
                });
        regularCreator.callback.waitForCallback(0, 1);
        mockObserver.stateLoadedCallback.waitForCallback(0, 1);
        Tab[] restoredTabs = new Tab[info.numRegularTabs];
        for (int i = 0; i < info.numRegularTabs; i++) {
            restoredTabs[i] = mockSelector.getModel(false).getTabAt(i);
        }
        return Pair.create(store, restoredTabs);
    }

    @Test
    @SmallTest
    @Feature("TabPersistentStore")
    @EnableFeatures({ChromeFeatureList.TAB_STATE_FLAT_BUFFER + "<Study"})
    @CommandLineFlags.Add({
        "force-fieldtrials=Study/Group",
        "force-fieldtrial-params=Study.Group:migrate_stale_tabs/true"
    })
    public void testSaveStateNoFlatBufferPrior() throws Exception {
        Pair<TabPersistentStore, Tab[]> storeAndRestoredTabs = createStoreAndRestoreTabs();
        TabPersistentStore store = storeAndRestoredTabs.first;
        Tab[] tabs = storeAndRestoredTabs.second;
        waitForTabStateCleanup(tabs);
        setAllTabStatesForTesting(tabs);
        // There should be no TabState files for tabs[0] to start with - legacy or FlatBuffer.
        File legacyFile = getLegacyTabStateFile(tabs[0]);
        File flatBufferFile = getFlatBufferTabStateFile(tabs[0]);
        Assert.assertFalse(
                "Legacy TabState File " + legacyFile + " should not exist", legacyFile.exists());
        Assert.assertFalse(
                "FlatBuffer TabState File " + flatBufferFile + " should not exist",
                flatBufferFile.exists());

        // Having tabs[0] in the save queue when saveState() is called should just save
        // the legacy TabState, but not migrate because the FlatBuffer file doesn't exist
        // prior to saveState() being called.
        CallbackHelper helper = new CallbackHelper();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    store.getTabsToSaveForTesting().add(tabs[0]);
                    store.saveState();
                    helper.notifyCalled();
                });
        helper.waitForCallback(0);

        Assert.assertTrue(
                "Legacy TabState File " + legacyFile + " should exist", legacyFile.exists());
        Assert.assertFalse(
                "FlatBuffer TabState File " + flatBufferFile + " should not exist",
                flatBufferFile.exists());
    }

    @Test
    @SmallTest
    @Feature("TabPersistentStore")
    @EnableFeatures({ChromeFeatureList.TAB_STATE_FLAT_BUFFER + "<Study"})
    @CommandLineFlags.Add({
        "force-fieldtrials=Study/Group",
        "force-fieldtrial-params=Study.Group:migrate_stale_tabs/true"
    })
    public void testSaveStateFlatBufferParityRootIdChange() throws Exception {
        Pair<TabPersistentStore, Tab[]> storeAndRestoredTabs = createStoreAndRestoreTabs();
        TabPersistentStore store = storeAndRestoredTabs.first;
        Tab[] tabs = storeAndRestoredTabs.second;
        waitForTabStateCleanup(tabs);
        setAllTabStatesForTesting(tabs);
        TabPersistentStore.onDeferredStartup();
        updateRootIdForTabStateSave(tabs[0].getId(), PREV_ROOT_ID);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    store.addTabToSaveQueue(tabs[0]);
                    store.saveNextTab();
                });
        waitForAllSavesAndMigrations(store);

        Assert.assertEquals(PREV_ROOT_ID, getRootIdFromLegacyTabStateFile(tabs[0]));
        Assert.assertEquals(PREV_ROOT_ID, getRootIdFromFlatBufferTabStateFile(tabs[0]));

        // Next save of tabs[0] should result in a legacy and FlatBuffer files with NEW_ROOT_ID
        updateRootIdForTabStateSave(tabs[0].getId(), NEW_ROOT_ID);

        CallbackHelper helper = new CallbackHelper();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    TabStateAttributes.from(tabs[0])
                            .setStateForTesting(TabStateAttributes.DirtinessState.DIRTY);
                    store.getTabsToSaveForTesting().add(tabs[0]);
                    store.saveState();
                    helper.notifyCalled();
                });

        helper.waitForCallback(0);
        // There should be parity between legacy and FlatBuffer TabState files with respect
        // to the attribute change (in this case root id).
        Assert.assertEquals(NEW_ROOT_ID, getRootIdFromLegacyTabStateFile(tabs[0]));
        Assert.assertEquals(NEW_ROOT_ID, getRootIdFromFlatBufferTabStateFile(tabs[0]));
    }

    @Test
    @SmallTest
    @Feature("TabPersistentStore")
    @EnableFeatures({ChromeFeatureList.TAB_STATE_FLAT_BUFFER + "<Study"})
    @CommandLineFlags.Add({
        "force-fieldtrials=Study/Group",
        "force-fieldtrial-params=Study.Group:migrate_stale_tabs/true"
    })
    public void testInFlightMigration() throws Exception {
        Pair<TabPersistentStore, Tab[]> storeAndRestoredTabs = createStoreAndRestoreTabs();
        TabPersistentStore store = storeAndRestoredTabs.first;
        Tab[] tabs = storeAndRestoredTabs.second;
        waitForTabStateCleanup(tabs);
        setAllTabStatesForTesting(tabs);
        TabPersistentStore.onDeferredStartup();

        CallbackHelper helper = new CallbackHelper();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    store.getTabsToMigrateForTesting().clear();
                    store.setMigrateTabTaskForTesting(store.new MigrateTabTask(tabs[0], 1));
                    Assert.assertNotNull(store.getMigrateTabTaskForTesting());
                    Assert.assertEquals(0, store.getTabsToMigrateForTesting().size());
                    store.saveState();
                    Assert.assertNull(store.getMigrateTabTaskForTesting());
                    Assert.assertEquals(1, store.getTabsToMigrateForTesting().size());
                    Assert.assertEquals(tabs[0], store.getTabsToMigrateForTesting().getFirst());
                    helper.notifyCalled();
                });
        helper.waitForCallback(0);
    }

    @Test
    @SmallTest
    @Feature("TabPersistentStore")
    @EnableFeatures({ChromeFeatureList.TAB_STATE_FLAT_BUFFER + "<Study"})
    @CommandLineFlags.Add({
        "force-fieldtrials=Study/Group",
        "force-fieldtrial-params=Study.Group:migrate_stale_tabs/true"
    })
    public void testUpdateMigratedFiles() throws Exception {
        Pair<TabPersistentStore, Tab[]> storeAndRestoredTabs = createStoreAndRestoreTabs();
        TabPersistentStore store = storeAndRestoredTabs.first;
        Tab[] tabs = storeAndRestoredTabs.second;
        waitForTabStateCleanup(tabs);
        setAllTabStatesForTesting(tabs);
        TabPersistentStore.onDeferredStartup();
        updateRootIdForTabStateSave(tabs[0].getId(), PREV_ROOT_ID);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    store.addTabToSaveQueue(tabs[0]);
                    store.saveNextTab();
                });
        waitForAllSavesAndMigrations(store);

        File flatBufferFile = getFlatBufferTabStateFile(tabs[0]);
        Assert.assertTrue(
                "FlatBuffer TabState File " + flatBufferFile + " should exist",
                flatBufferFile.exists());
        Assert.assertEquals(PREV_ROOT_ID, getRootIdFromFlatBufferTabStateFile(tabs[0]));

        updateRootIdForTabStateSave(tabs[0].getId(), NEW_ROOT_ID);

        CallbackHelper helper = new CallbackHelper();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    store.getTabsToMigrateForTesting().clear();
                    store.getTabsToMigrateForTesting().add(tabs[0]);
                    Assert.assertEquals(1, store.getTabsToMigrateForTesting().size());
                    Assert.assertEquals(tabs[0], store.getTabsToMigrateForTesting().getFirst());
                    store.setMigrateTabTaskForTesting(store.new MigrateTabTask(tabs[0], 1));
                    store.updateMigratedFiles();
                    Assert.assertEquals(0, store.getTabsToMigrateForTesting().size());
                    Assert.assertEquals(NEW_ROOT_ID, getRootIdFromFlatBufferTabStateFile(tabs[0]));
                    helper.notifyCalled();
                });
        helper.waitForCallback(0);
    }

    private static void updateRootIdForTabStateSave(int tabId, int rootId) {
        TabState tabState = new TabState();
        ByteBuffer buffer = ByteBuffer.allocateDirect(4);
        buffer.put(new byte[] {1, 2, 3, 4});
        tabState.contentsState = new WebContentsState(buffer);
        tabState.rootId = rootId;
        TabStateExtractor.setTabStateForTesting(tabId, tabState);
    }

    private static void waitForAllSavesAndMigrations(TabPersistentStore store) {
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            store.isSavingAndMigratingIdleForTesting(), Matchers.is(true));
                });
    }

    private int getRootIdFromLegacyTabStateFile(Tab tab) {
        return TabStateFileManager.restoreTabState(
                        mMockDirectory.getDataDirectory(),
                        tab.getId(),
                        sCipherFactory,
                        /* useFlatBuffer= */ false)
                .rootId;
    }

    private int getRootIdFromFlatBufferTabStateFile(Tab tab) {
        return TabStateFileManager.restoreTabState(
                        mMockDirectory.getDataDirectory(),
                        tab.getId(),
                        sCipherFactory,
                        /* useFlatBuffer= */ true)
                .rootId;
    }

    private File getLegacyTabStateFile(Tab tab) {
        return TabStateFileManager.getTabStateFile(
                mMockDirectory.getDataDirectory(),
                /* tabId= */ tab.getId(),
                /* encrypted= */ false,
                /* isFlatBuffer= */ false);
    }

    private File getFlatBufferTabStateFile(Tab tab) {
        return TabStateFileManager.getTabStateFile(
                mMockDirectory.getDataDirectory(),
                /* tabId= */ tab.getId(),
                /* encrypted= */ false,
                /* isFlatBuffer= */ true);
    }

    /**
     * TabStateExtractor expects a Tab to be initialized in order to acquire a TabState. In the
     * absence of this, TabStateExtractor#from has an early return which can be set via
     * setTabStateForTesting. This method creates a placeholder TabState for each Tab to activate
     * this early return.
     *
     * @param tabs {@link Tab}s to setTabStateForTesting for
     */
    private static void setAllTabStatesForTesting(Tab[] tabs) throws TimeoutException {
        CallbackHelper helper = new CallbackHelper();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    for (Tab tab : tabs) {
                        TabState tabState = new TabState();
                        ByteBuffer buffer = ByteBuffer.allocateDirect(4);
                        buffer.put(new byte[] {1, 2, 3, 4});
                        tabState.contentsState = new WebContentsState(buffer);
                        TabStateExtractor.setTabStateForTesting(tab.getId(), tabState);
                    }
                    helper.notifyCalled();
                });
        helper.waitForCallback(0);
    }

    @Test
    @SmallTest
    @Feature("TabPersistentStore")
    @EnableFeatures({ChromeFeatureList.TAB_STATE_FLAT_BUFFER + "<Study"})
    @CommandLineFlags.Add({
        "force-fieldtrials=Study/Group",
        "force-fieldtrial-params=Study.Group:migrate_stale_tabs/true"
    })
    public void testRemoveMigration_crbug_340580707() throws Exception {
        Pair<TabPersistentStore, Tab[]> storeAndRestoredTabs = createStoreAndRestoreTabs();
        TabPersistentStore store = storeAndRestoredTabs.first;
        Tab[] tabs = storeAndRestoredTabs.second;
        CallbackHelper helper = new CallbackHelper();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    store.setMigrateTabTaskForTesting(store.new MigrateTabTask(tabs[0], 1));
                    store.removeTabFromQueues(tabs[0]);
                    helper.notifyCalled();
                });
        helper.waitForCallback(0);
    }

    @Test
    @SmallTest
    @Feature("TabPersistentStore")
    public void testMaintenance() throws Exception {
        Looper.prepare();
        mMockDirectory.writeTabModelFiles(TestTabModelDirectory.GOOGLE_CA_GOOGLE_COM, true, 0);
        mMockDirectory.writeTabModelFiles(TestTabModelDirectory.TEXTAREA_DUCK_DUCK_GO, true, 1);

        // Set up the TabPersistentStore.
        MockTabModelSelector mockSelector =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            Profile profile = ProfileManager.getLastUsedRegularProfile();
                            return new MockTabModelSelector(
                                    profile, profile.getPrimaryOTRProfile(true), 0, 0, null);
                        });

        MockTabCreatorManager mockManager = new MockTabCreatorManager(mockSelector);
        TabPersistencePolicy persistencePolicy = createTabPersistencePolicy(0, false, true);
        final TabPersistentStore store =
                buildTabPersistentStore(persistencePolicy, mockSelector, mockManager);
        CallbackHelper helper = new CallbackHelper();
        // Tabs 1, 3, 4, 5 are in the Tab metadata files
        // Tab 2 is considered orphaned (a {@link PersistedTabData} entry will be added for it).
        // However, Tab 2 is not in the Tab model so the data is considered orphaned.
        // Incognito Tabs 6 and 7 are in the metadata file
        // TestTabModelDirectory.TEXTAREA_DUCK_DUCK_GO Test includes them to ensure these do not
        // impact maintenance. There are no PersistedTabData entries for incognito Tabs, but we need
        // to ensure that including incognito Tabs in a global collection of Tabs passed to the
        // maintenance function doesn't impact maintenance.
        MockTab[] tabs = new MockTab[6];
        for (int tabId = 1; tabId < tabs.length; tabId++) {
            tabs[tabId] = createTabAndPersistedEntry(tabId);
        }

        // Maintenance should remove the entry for Tab 2 which is orphaned.
        store.performPersistedTabDataMaintenance(
                new Runnable() {
                    @Override
                    public void run() {
                        helper.notifyCalled();
                    }
                });
        helper.waitForCallback(0);
        for (int i = 1; i < tabs.length; i++) {
            // Tab 2 is orphaned and shouldn't exist
            // All other persisted Tab entries should be intact.
            checkEntryExists(tabs[i], i != 2);
        }
    }

    private static MockTab createTabAndPersistedEntry(final int tabId)
            throws TimeoutException, ExecutionException {
        CallbackHelper helper = new CallbackHelper();
        MockTab tab =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            MockTab newTab =
                                    new MockTab(tabId, ProfileManager.getLastUsedRegularProfile());
                            ObservableSupplierImpl<Boolean> observableSupplier =
                                    new ObservableSupplierImpl<>();
                            observableSupplier.set(true);
                            ShoppingPersistedTabData.from(newTab)
                                    .registerIsTabSaveEnabledSupplier(observableSupplier);
                            ShoppingPersistedTabData.from(newTab).save();
                            ShoppingPersistedTabData.from(newTab)
                                    .existsInStorage(
                                            (res) -> {
                                                Assert.assertTrue(res);
                                                helper.notifyCalled();
                                            });
                            return newTab;
                        });
        helper.waitForCallback(0);
        return tab;
    }

    private static void checkEntryExists(Tab tab, boolean expectedExists) throws TimeoutException {
        CallbackHelper helper = new CallbackHelper();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ShoppingPersistedTabData.from(tab)
                            .existsInStorage(
                                    (res) -> {
                                        Assert.assertEquals(expectedExists, res);
                                        helper.notifyCalled();
                                    });
                });
        helper.waitForCallback(0);
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
        MockTabModelSelector firstSelector =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            Profile profile = ProfileManager.getLastUsedRegularProfile();
                            return new MockTabModelSelector(
                                    profile, profile.getPrimaryOTRProfile(true), 0, 0, null);
                        });

        MockTabCreatorManager firstManager = new MockTabCreatorManager(firstSelector);
        MockTabPersistentStoreObserver firstObserver = new MockTabPersistentStoreObserver();
        TabPersistencePolicy firstPersistencePolicy = createTabPersistencePolicy(0, false, true);
        final TabPersistentStore firstStore =
                buildTabPersistentStore(firstPersistencePolicy, firstSelector, firstManager);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    firstStore.addObserver(firstObserver);
                    firstStore.loadState(/* ignoreIncognitoFiles= */ false);
                });
        firstObserver.initializedCallback.waitForCallback(0, 1);
        Assert.assertEquals(numExpectedTabs, firstObserver.mTabCountAtStartup);
        firstObserver.detailsReadCallback.waitForCallback(0, numExpectedTabs);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    firstStore.saveState();
                });

        // Prepare a second TabPersistentStore.
        MockTabModelSelector secondSelector =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            Profile profile = ProfileManager.getLastUsedRegularProfile();
                            return new MockTabModelSelector(
                                    profile, profile.getPrimaryOTRProfile(true), 0, 0, null);
                        });

        MockTabCreatorManager secondManager = new MockTabCreatorManager(secondSelector);
        MockTabCreator secondCreator = secondManager.getTabCreator(false);
        MockTabPersistentStoreObserver secondObserver = new MockTabPersistentStoreObserver();
        TabPersistencePolicy secondPersistencePolicy = createTabPersistencePolicy(0, false, true);

        final TabPersistentStore secondStore =
                buildTabPersistentStore(secondPersistencePolicy, secondSelector, secondManager);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    secondStore.addObserver(secondObserver);

                    // The second TabPersistentStore reads the file written by the first
                    // TabPersistentStore.
                    // Make sure that all of the Tabs appear in the new one -- even though the new
                    // file was written before the first TabPersistentStore loaded any TabState
                    // files and added them to the TabModels.
                    secondStore.loadState(/* ignoreIncognitoFiles= */ false);
                });
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
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    secondStore.restoreTabs(true);
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
        MockTabModelSelector mockSelector =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            Profile profile = ProfileManager.getLastUsedRegularProfile();
                            return new MockTabModelSelector(
                                    profile, profile.getPrimaryOTRProfile(true), 0, 0, null);
                        });

        MockTabCreatorManager mockManager = new MockTabCreatorManager(mockSelector);
        MockTabPersistentStoreObserver mockObserver = new MockTabPersistentStoreObserver();
        TabPersistencePolicy persistencePolicy = createTabPersistencePolicy(0, false, true);
        final TabPersistentStore store =
                buildTabPersistentStore(persistencePolicy, mockSelector, mockManager);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    store.addObserver(mockObserver);

                    // Make sure the metadata file loads properly and in order.
                    store.loadState(/* ignoreIncognitoFiles= */ false);
                });
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

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    // Restore the TabStates, and confirm that the correct number of tabs is created
                    // even with one missing.
                    store.restoreTabs(true);
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
        MockTabModelSelector mockSelector =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            Profile profile = ProfileManager.getLastUsedRegularProfile();
                            return new MockTabModelSelector(
                                    profile, profile.getPrimaryOTRProfile(true), 0, 0, null);
                        });
        MockTabCreatorManager mockManager = new MockTabCreatorManager(mockSelector);
        MockTabPersistentStoreObserver mockObserver = new MockTabPersistentStoreObserver();
        TabPersistencePolicy persistencePolicy = createTabPersistencePolicy(0, false, true);
        final TabPersistentStore store =
                buildTabPersistentStore(persistencePolicy, mockSelector, mockManager);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    store.addObserver(mockObserver);

                    // Load the TabModel metadata.
                    store.loadState(/* ignoreIncognitoFiles= */ false);
                });
        mockObserver.initializedCallback.waitForCallback(0, 1);
        Assert.assertEquals(numExpectedTabs, mockObserver.mTabCountAtStartup);
        mockObserver.detailsReadCallback.waitForCallback(0, numExpectedTabs);
        Assert.assertEquals(numExpectedTabs, mockObserver.details.size());

        // TODO(dfalcantara): Expand MockTabModel* to support Incognito Tab decryption.

        // Restore the TabStates, and confirm that the correct number of tabs is created even with
        // one missing.  No Incognito tabs should be created because the TabState is missing.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    store.restoreTabs(true);
                });
        mockObserver.stateLoadedCallback.waitForCallback(0, 1);
        Assert.assertEquals(info.numRegularTabs, mockSelector.getModel(false).getCount());
        Assert.assertEquals(0, mockSelector.getModel(true).getCount());
    }

    @Test
    @SmallTest
    @Feature({"TabPersistentStore"})
    public void testSerializeDuringRestore() throws Exception {
        TabStateInfo regularTab =
                new TabStateInfo(false, 2, 2, "https://google.com", "Google", null);
        TabStateInfo regularTab2 = new TabStateInfo(false, 2, 3, "https://foo.com", "Foo", null);
        TabStateInfo incognitoTab =
                new TabStateInfo(true, 2, 17, "https://incognito.com", "Incognito", null);

        MockTabModelSelector mockSelector =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            Profile profile = ProfileManager.getLastUsedRegularProfile();
                            return new MockTabModelSelector(
                                    profile, profile.getPrimaryOTRProfile(true), 0, 0, null);
                        });
        MockTabCreatorManager mockManager = new MockTabCreatorManager(mockSelector);
        final MockTabPersistentStoreObserver mockObserver = new MockTabPersistentStoreObserver();
        TabPersistencePolicy persistencePolicy = createTabPersistencePolicy(0, false, true);
        final TabPersistentStore store =
                buildTabPersistentStore(persistencePolicy, mockSelector, mockManager);

        // Without loading state, simulate three tabs pending restore, then save state to write
        // out to disk.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    store.addObserver(mockObserver);

                    store.addTabToRestoreForTesting(
                            new TabRestoreDetails(
                                    regularTab.tabId, 0, false, regularTab.url, false));
                    store.addTabToRestoreForTesting(
                            new TabRestoreDetails(
                                    incognitoTab.tabId, 0, true, incognitoTab.url, false));
                    store.addTabToRestoreForTesting(
                            new TabRestoreDetails(
                                    regularTab2.tabId, 1, false, regularTab2.url, false));

                    store.saveState();
                    store.destroy();
                });

        TabModelMetaDataInfo info =
                new TabModelMetaDataInfo(
                        5, 1, 1, new TabStateInfo[] {incognitoTab, regularTab, regularTab2}, null);

        // Create and restore a real tab model, validating proper state. Incognito cannot be
        // restored since there are no state files for individual tabs on disk (just a tab metadata
        // file).
        TestTabModelSelector testSelector = createAndRestoreRealTabModelImpls(info, false, false);
        MockTabPersistentStoreObserver otherMockObserver = testSelector.mTabPersistentStoreObserver;

        // Assert state on tab details restored from metadata file.
        Assert.assertTrue(
                "First restored tab should be incognito.",
                otherMockObserver.details.get(0).isIncognito);
        Assert.assertEquals(
                "Incorrect URL for first restored tab.",
                incognitoTab.url,
                otherMockObserver.details.get(0).url);

        Assert.assertFalse(
                "Second restored tab should be regular.",
                otherMockObserver.details.get(1).isIncognito);
        Assert.assertEquals(
                "Incorrect URL for second restored tab.",
                regularTab.url,
                otherMockObserver.details.get(1).url);

        Assert.assertFalse(
                "Third restored tab should be regular.",
                otherMockObserver.details.get(2).isIncognito);
        Assert.assertEquals(
                "Incorrect URL for third restored tab.",
                regularTab2.url,
                otherMockObserver.details.get(2).url);
    }

    @Test
    @SmallTest
    @Feature({"TabPersistentStore"})
    public void testPrefetchActiveTab() throws Exception {
        final TabModelMetaDataInfo info = TestTabModelDirectory.TAB_MODEL_METADATA_V5_NO_M18;
        mMockDirectory.writeTabModelFiles(info, true);

        // Set to pre-fetch
        mPreferences.writeInt(ChromePreferenceKeys.TABMODEL_ACTIVE_TAB_ID, info.selectedTabId);

        // Initialize the classes.
        MockTabModelSelector mockSelector =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            Profile profile = ProfileManager.getLastUsedRegularProfile();
                            return new MockTabModelSelector(
                                    profile, profile.getPrimaryOTRProfile(true), 0, 0, null);
                        });
        MockTabCreatorManager mockManager = new MockTabCreatorManager(mockSelector);
        MockTabPersistentStoreObserver mockObserver = new MockTabPersistentStoreObserver();
        TabPersistencePolicy persistencePolicy = createTabPersistencePolicy(0, false, true);
        final TabPersistentStore store =
                buildTabPersistentStore(persistencePolicy, mockSelector, mockManager);
        ThreadUtils.runOnUiThreadBlocking(() -> store.addObserver(mockObserver));
        store.waitForMigrationToFinish();

        Assert.assertNotNull(store.getPrefetchTabStateActiveTabTaskForTesting());

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    store.loadState(/* ignoreIncognitoFiles= */ false);
                    store.restoreTabs(true);
                });

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    // Confirm that the pre-fetched active tab state was used, must be done here on
                    // the
                    // UI thread as the message to finish the task is posted here.
                    Assert.assertEquals(
                            AsyncTask.Status.FINISHED,
                            store.getPrefetchTabStateActiveTabTaskForTesting().getStatus());

                    // Confirm that the correct active tab ID is updated when saving state.
                    mPreferences.writeInt(ChromePreferenceKeys.TABMODEL_ACTIVE_TAB_ID, -1);

                    store.saveState();
                });

        Assert.assertEquals(
                info.selectedTabId,
                mPreferences.readInt(ChromePreferenceKeys.TABMODEL_ACTIVE_TAB_ID, -1));
    }

    /**
     * Tests that a real {@link TabModelImpl} will use the {@link TabPersistentStore} to write out
     * valid a valid metadata file and the TabModel's associated TabStates after closing and
     * canceling the closure of all the tabs simultaneously.
     */
    @Test
    @SmallTest
    @Feature({"TabPersistentStore"})
    public void testUndoCloseAllTabsWritesTabListFile() throws Exception {
        final TabModelMetaDataInfo info = TestTabModelDirectory.TAB_MODEL_METADATA_V5_NO_M18;
        mMockDirectory.writeTabModelFiles(info, true);

        for (int i = 0; i < 2; i++) {
            final TestTabModelSelector selector = createAndRestoreRealTabModelImpls(info);

            // Undoing tab closures one-by-one results in the first tab always being selected after
            // the initial restoration.
            Tab currentTab = ThreadUtils.runOnUiThreadBlocking(selector::getCurrentTab);
            if (i == 0) {
                Assert.assertEquals(info.selectedTabId, currentTab.getId());
            } else {
                Assert.assertEquals(info.contents[0].tabId, currentTab.getId());
            }

            ThreadUtils.runOnUiThreadBlocking(
                    () -> {
                        closeAllTabsThenUndo(selector, info);

                        // Synchronously save the data out to simulate minimizing Chrome.
                        selector.mTabPersistentStore.saveState();
                    });

            // Load up each TabState and confirm that values are still correct.
            for (int j = 0; j < info.numRegularTabs; j++) {
                if (restoredFromDisk(selector.getModel(false).getTabAt(j))) {
                    TabState currentState =
                            TabStateFileManager.restoreTabState(
                                    mMockDirectory.getDataDirectory(),
                                    info.contents[j].tabId,
                                    sCipherFactory);
                    Assert.assertEquals(
                            info.contents[j].title,
                            currentState.contentsState.getDisplayTitleFromState());
                    Assert.assertEquals(
                            info.contents[j].url,
                            currentState.contentsState.getVirtualUrlFromState());
                }
            }
        }
    }

    /**
     * Determines if {@link Tab} was restored from disk or not. Assumes the {@link Tab} was restored
     * from disk if there was not record of how it was created.
     */
    private static boolean restoredFromDisk(Tab tab) throws ExecutionException {
        return ThreadUtils.runOnUiThreadBlocking(
                new Callable<Boolean>() {
                    @Override
                    public Boolean call() {
                        if (tab.getUserDataHost().getUserData(MockTabAttributes.class) == null) {
                            return true;
                        }
                        return tab.getUserDataHost()
                                .getUserData(MockTabAttributes.class)
                                .restoredFromDisk;
                    }
                });
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

    @Test
    @SmallTest
    @Feature({"TabPersistentStore"})
    public void testTabRestoreMethodEnumValues() {
        // These enums are recorded in the metrics and should not be changed.
        Assert.assertEquals(0, TabRestoreMethod.TAB_STATE);
        Assert.assertEquals(1, TabRestoreMethod.CRITICAL_PERSISTED_TAB_DATA);
        Assert.assertEquals(2, TabRestoreMethod.CREATE_NEW_TAB);
        Assert.assertEquals(3, TabRestoreMethod.FAILED_TO_RESTORE);
        Assert.assertEquals(4, TabRestoreMethod.SKIPPED_NTP);
        Assert.assertEquals(5, TabRestoreMethod.SKIPPED_EMPTY_URL);
    }

    private void addTabsToSaveQueue(TabPersistentStore store, Tab[] tabsToSave) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    for (int i = 0; i < tabsToSave.length; i++) {
                        // Tabs are uninitialized so TabState won't save unless we override here.
                        // It doesn't matter what TabState is saved for the tests which use this
                        // function only that it is saved. So an arbitrary TabState is used.
                        TabStateExtractor.setTabStateForTesting(
                                tabsToSave[i].getId(), new TabState());
                        TabStateAttributes.from(tabsToSave[i])
                                .setStateForTesting(TabStateAttributes.DirtinessState.DIRTY);
                        store.addTabToSaveQueue(tabsToSave[i]);
                    }
                });
    }

    private TestTabModelSelector createAndRestoreRealTabModelImpls(TabModelMetaDataInfo info)
            throws Exception {
        return createAndRestoreRealTabModelImpls(info, true, true);
    }

    /**
     * @param info TabModelMetaDataInfo to check restore tab models against.
     * @param restoreIncognito Whether incognito tabs should be restored. In order for restore to
     *     succeed, there must be a readable tab state file on disk.
     * @param expectMatchingIds Whether restored tab id's are expected to match those in {@coe
     *     info}. If there is no tab state file for a given entry in the metadata file,
     *     TabPersistentStore currently creates a new tab with the last known URL, in which case the
     *     new tab's id won't match the id in the metadata file.
     * @return A {@link TestTabModelSelector} with the restored tabs.
     */
    private TestTabModelSelector createAndRestoreRealTabModelImpls(
            TabModelMetaDataInfo info, boolean restoreIncognito, boolean expectMatchingIds)
            throws Exception {
        TestTabModelSelector selector =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            ApplicationStatus.onStateChangeForTesting(
                                    mChromeActivity, ActivityState.STARTED);
                            // Clear any existing TestTabModelSelector (required when
                            // createAndRestoreRealTabModelImpls is called multiple times in one
                            // test).
                            sTabWindowManager.onActivityStateChange(
                                    mChromeActivity, ActivityState.DESTROYED);
                            var profileProvider =
                                    new ActivityProfileProvider(
                                            mChromeActivity.getLifecycleDispatcher());
                            return (TestTabModelSelector)
                                    sTabWindowManager.requestSelector(
                                                    mChromeActivity,
                                                    profileProvider,
                                                    mChromeActivity,
                                                    null,
                                                    (activityAtRequestedIndex,
                                                            isActivityInAppTasks,
                                                            isActivityInSameTask) -> false,
                                                    0)
                                            .second;
                        });

        final TabPersistentStore store = selector.mTabPersistentStore;
        MockTabPersistentStoreObserver mockObserver = selector.mTabPersistentStoreObserver;

        // Load up the TabModel metadata.
        int numExpectedTabs = info.numRegularTabs + info.numIncognitoTabs;
        ThreadUtils.runOnUiThreadBlocking(
                () -> store.loadState(/* ignoreIncognitoFiles= */ !restoreIncognito));
        mockObserver.initializedCallback.waitForCallback(0, 1);
        Assert.assertEquals(numExpectedTabs, mockObserver.mTabCountAtStartup);
        mockObserver.detailsReadCallback.waitForCallback(0, info.contents.length);

        Assert.assertEquals(numExpectedTabs, mockObserver.details.size());

        // Restore the TabStates, check that things were restored correctly, in the right tab order.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    store.restoreTabs(true);
                });
        mockObserver.stateLoadedCallback.waitForCallback(0, 1);

        int numIncognitoExpected = restoreIncognito ? info.numIncognitoTabs : 0;
        Assert.assertEquals(
                "Incorrect number of regular tabs.",
                info.numRegularTabs,
                selector.getModel(false).getCount());
        Assert.assertEquals(
                "Incorrect number of incognito tabs.",
                numIncognitoExpected,
                selector.getModel(true).getCount());

        int tabInfoIndex = info.numIncognitoTabs;
        for (int i = 0; i < info.numRegularTabs; i++) {
            Tab tab = selector.getModel(false).getTabAt(i);

            if (expectMatchingIds) {
                if (restoredFromDisk(tab)) {
                    Assert.assertEquals(
                            "Incorrect regular tab at position " + i,
                            info.contents[tabInfoIndex].tabId,
                            tab.getId());
                } else {
                    String url =
                            ThreadUtils.runOnUiThreadBlocking(
                                    () -> {
                                        return tab.getUrl().getSpec();
                                    });
                    Assert.assertEquals(
                            "Unexpected URL on Tab", info.contents[tabInfoIndex].url, url);
                }
            }
            tabInfoIndex++;
        }

        for (int i = 0; i < numIncognitoExpected; i++) {
            Tab tab = selector.getModel(true).getTabAt(i);
            if (expectMatchingIds) {
                if (restoredFromDisk(tab)) {
                    Assert.assertEquals(
                            "Incorrect incognito tab at position " + i,
                            info.contents[i].tabId,
                            tab.getId());
                } else {
                    String url =
                            ThreadUtils.runOnUiThreadBlocking(
                                    () -> {
                                        return tab.getUrl().getSpec();
                                    });
                    Assert.assertEquals(
                            "Unexpected URL on Tab", info.contents[tabInfoIndex].url, url);
                }
            }
        }

        return selector;
    }

    /**
     * Close all Tabs in the regular TabModel, then undo the operation to restore the Tabs. This
     * simulates how {@link StripLayoutHelper} and {@link UndoBarController} would close all of a
     * {@link TabModel}'s tabs on tablets.
     */
    private void closeAllTabsThenUndo(TabModelSelector selector, TabModelMetaDataInfo info) {
        // Close all the tabs, using an Observer to determine what is actually being closed.
        TabModel regularModel = selector.getModel(false);
        final List<Integer> closedTabIds = new ArrayList<>();
        TabModelObserver closeObserver =
                new TabModelObserver() {
                    @Override
                    public void multipleTabsPendingClosure(List<Tab> tabs, boolean isAllTabs) {
                        for (Tab tab : tabs) closedTabIds.add(tab.getId());
                    }
                };
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    regularModel.addObserver(closeObserver);
                    regularModel.closeTabs(TabClosureParams.closeAllTabs().build());
                });
        Assert.assertEquals(info.numRegularTabs, closedTabIds.size());

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    // Cancel closing each tab.
                    for (Integer id : closedTabIds) regularModel.cancelTabClosure(id);
                });
        Assert.assertEquals(info.numRegularTabs, regularModel.getCount());
    }
}

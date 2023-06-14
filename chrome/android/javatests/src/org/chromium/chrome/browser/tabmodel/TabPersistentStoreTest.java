// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import android.app.Activity;
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
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mockito;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ApplicationStatus.ActivityStateListener;
import org.chromium.base.ContextUtils;
import org.chromium.base.task.AsyncTask;
import org.chromium.base.test.params.ParameterAnnotations;
import org.chromium.base.test.params.ParameterProvider;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.AdvancedMockContext;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.chrome.browser.accessibility_tab_switcher.OverviewListLayout;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.app.metrics.LaunchCauseMetrics;
import org.chromium.chrome.browser.app.tabmodel.AsyncTabParamsManagerSingleton;
import org.chromium.chrome.browser.app.tabmodel.ChromeTabModelFilterFactory;
import org.chromium.chrome.browser.app.tabmodel.TabModelOrchestrator;
import org.chromium.chrome.browser.app.tabmodel.TabWindowManagerSingleton;
import org.chromium.chrome.browser.compositor.layouts.content.TabContentManager;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutHelper;
import org.chromium.chrome.browser.flags.ActivityType;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.MockTabAttributes;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabImpl;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tab.TabState;
import org.chromium.chrome.browser.tab.TabStateAttributes;
import org.chromium.chrome.browser.tab.TabStateExtractor;
import org.chromium.chrome.browser.tab.state.CriticalPersistedTabData;
import org.chromium.chrome.browser.tab.state.FilePersistedTabDataStorage;
import org.chromium.chrome.browser.tab.state.PersistedTabDataConfiguration;
import org.chromium.chrome.browser.tab.state.SerializedCriticalPersistedTabData;
import org.chromium.chrome.browser.tabmodel.NextTabPolicy.NextTabPolicySupplier;
import org.chromium.chrome.browser.tabmodel.TabPersistentStore.TabModelSelectorMetadata;
import org.chromium.chrome.browser.tabmodel.TabPersistentStore.TabPersistentStoreObserver;
import org.chromium.chrome.browser.tabmodel.TabPersistentStore.TabRestoreDetails;
import org.chromium.chrome.browser.tabmodel.TabPersistentStore.TabRestoreMethod;
import org.chromium.chrome.browser.tabmodel.TestTabModelDirectory.TabModelMetaDataInfo;
import org.chromium.chrome.browser.tabmodel.TestTabModelDirectory.TabStateInfo;
import org.chromium.chrome.browser.tabpersistence.TabStateDirectory;
import org.chromium.chrome.browser.tabpersistence.TabStateFileManager;
import org.chromium.chrome.test.ChromeBrowserTestRule;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.chrome.test.util.browser.tabmodel.MockTabCreator;
import org.chromium.chrome.test.util.browser.tabmodel.MockTabCreatorManager;
import org.chromium.chrome.test.util.browser.tabmodel.MockTabModelSelector;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.concurrent.Callable;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeoutException;

/** Tests for the TabPersistentStore. */

// TODO(crbug.com/1174662) reintroduce batching - batching was removed because introducing
// parameterized tests caused cross-talk between tests.
@RunWith(ParameterizedRunner.class)
@ParameterAnnotations.UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@CommandLineFlags.
Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE, "force-fieldtrials=Study/Group"})
public class TabPersistentStoreTest {
    // Test activity type that does not restore tab on cold restart.
    // Any type other than ActivityType.TABBED works.
    private static final @ActivityType int NO_RESTORE_TYPE = ActivityType.CUSTOM_TAB;

    @Rule
    public final ChromeBrowserTestRule mBrowserTestRule = new ChromeBrowserTestRule();

    @Rule
    public TestRule mProcessor = new Features.InstrumentationProcessor();

    private ChromeActivity mChromeActivity;

    private static final int SELECTOR_INDEX = 0;

    private static class TabRestoredDetails {
        public int index;
        public int id;
        public String url;
        public boolean isStandardActiveIndex;
        public boolean isIncognitoActiveIndex;
        public Boolean isIncognito;

        /** Store information about a Tab that's been restored. */
        TabRestoredDetails(int index, int id, String url, boolean isStandardActiveIndex,
                boolean isIncognitoActiveIndex, Boolean isIncognito) {
            this.index = index;
            this.id = id;
            this.url = url;
            this.isStandardActiveIndex = isStandardActiveIndex;
            this.isIncognitoActiveIndex = isIncognitoActiveIndex;
            this.isIncognito = isIncognito;
        }
    }

    /**
     * Used when testing interactions of TabPersistentStore with real {@link TabModelImpl}s.
     */
    static class TestTabModelSelector extends TabModelSelectorBase
            implements TabModelDelegate {
        final TabPersistentStore mTabPersistentStore;
        final MockTabPersistentStoreObserver mTabPersistentStoreObserver;
        private final TabModelOrderController mTabModelOrderController;
        // Required to ensure TabContentManager is not null.
        private final TabContentManager mMockTabContentManager;

        public TestTabModelSelector(Activity activity) throws Exception {
            super(new MockTabCreatorManager(), new ChromeTabModelFilterFactory(activity), false);
            ((MockTabCreatorManager) getTabCreatorManager()).initialize(this);
            mTabPersistentStoreObserver = new MockTabPersistentStoreObserver();
            // Use of a mockito object here is ok as this object is not important to the test. A
            // real object is not available from {@link ChromeActivity} due to the test structure.
            // {@link TabModelImpl} requires a non-null {@link TabContentManager} to initialize.
            mMockTabContentManager = Mockito.mock(TabContentManager.class);
            mTabPersistentStore =
                    TestThreadUtils.runOnUiThreadBlocking(new Callable<TabPersistentStore>() {
                        @Override
                        public TabPersistentStore call() {
                            TabPersistencePolicy persistencePolicy =
                                    createTabPersistencePolicy(0, true, true);
                            persistencePolicy.setTabContentManager(mMockTabContentManager);
                            TabPersistentStore tabPersistentStore =
                                    new TabPersistentStore(persistencePolicy,
                                            TestTabModelSelector.this, getTabCreatorManager());
                            tabPersistentStore.addObserver(mTabPersistentStoreObserver);
                            return tabPersistentStore;
                        }
                    });
            mTabModelOrderController = new TabModelOrderControllerImpl(this);
            NextTabPolicySupplier nextTabPolicySupplier = () -> NextTabPolicy.HIERARCHICAL;

            Callable<TabModelImpl> callable = new Callable<TabModelImpl>() {
                @Override
                public TabModelImpl call() {
                    return new TabModelImpl(Profile.getLastUsedRegularProfile(), NO_RESTORE_TYPE,
                            getTabCreatorManager().getTabCreator(false),
                            getTabCreatorManager().getTabCreator(true), mTabModelOrderController,
                            mMockTabContentManager, nextTabPolicySupplier,
                            AsyncTabParamsManagerSingleton.getInstance(), TestTabModelSelector.this,
                            true);
                }
            };
            TabModelImpl regularTabModel = TestThreadUtils.runOnUiThreadBlocking(callable);
            IncognitoTabModel incognitoTabModel =
                    new IncognitoTabModelImpl(new IncognitoTabModelImplCreator(null,
                            getTabCreatorManager().getTabCreator(false),
                            getTabCreatorManager().getTabCreator(true), mTabModelOrderController,
                            null, nextTabPolicySupplier,
                            AsyncTabParamsManagerSingleton.getInstance(), NO_RESTORE_TYPE, this));
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
        public void onDetailsRead(int index, int id, String url, boolean isStandardActiveIndex,
                boolean isIncognitoActiveIndex, Boolean isIncognito) {
            details.add(new TabRestoredDetails(
                    index, id, url, isStandardActiveIndex, isIncognitoActiveIndex, isIncognito));
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
                public TabModelSelector buildSelector(Activity activity,
                        TabCreatorManager tabCreatorManager,
                        NextTabPolicySupplier nextTabPolicySupplier, int selectorIndex) {
                    try {
                        return new TestTabModelSelector(activity);
                    } catch (Exception e) {
                        throw new RuntimeException(e);
                    }
                }
            };
    private static TabWindowManagerImpl sTabWindowManager;

    /**
     * Parameterizes whether CriticalPersistedTabData flag should be turned on
     * or off
     */
    public static class StoreParamProvider implements ParameterProvider {
        @Override
        public Iterable<ParameterSet> getParameters() {
            return Arrays.asList(new ParameterSet().value(false).name("TabState"),
                    new ParameterSet().value(true).name("CriticalPersistedTabData"));
        }
    }

    /** Class for mocking out the directory containing all of the TabState files. */
    private TestTabModelDirectory mMockDirectory;
    private AdvancedMockContext mAppContext;
    private SharedPreferencesManager mPreferences;

    // This is used to pretend we've started the activity, so we can attach a base context to the
    // activity.
    private final ActivityStateListener mActivityStateListener = (activity, state) -> {
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

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            sTabWindowManager = (TabWindowManagerImpl) TabWindowManagerSingleton.getInstance();
        });
    }

    @AfterClass
    public static void afterClassTearDown() {
        TabWindowManagerSingleton.resetTabModelSelectorFactoryForTesting();
    }

    @Before
    public void setUp() {
        ChromeFeatureList.sCriticalPersistedTabData.setForTesting(false);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
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

                // This is intended to pretend we've started the activity, so we can attach a base
                // context to the activity.
                @Override
                public void onStart() {
                    if (getBaseContext() == null) {
                        attachBaseContext(mAppContext);
                    }
                }
            };
            ApplicationStatus.onStateChangeForTesting(mChromeActivity, ActivityState.CREATED);
        });

        // Using an AdvancedMockContext allows us to use a fresh in-memory SharedPreference.
        mAppContext = new AdvancedMockContext(InstrumentationRegistry.getInstrumentation()
                                                      .getTargetContext()
                                                      .getApplicationContext());
        ContextUtils.initApplicationContextForTests(mAppContext);
        mPreferences = SharedPreferencesManager.getInstance();
        mMockDirectory = new TestTabModelDirectory(
                mAppContext, "TabPersistentStoreTest", Integer.toString(SELECTOR_INDEX));
        TabStateDirectory.setBaseStateDirectoryForTests(mMockDirectory.getBaseDirectory());

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ApplicationStatus.registerStateListenerForActivity(
                    mActivityStateListener, mChromeActivity);
        });
    }

    @After
    public void tearDown() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            sTabWindowManager.onActivityStateChange(mChromeActivity, ActivityState.DESTROYED);
            ApplicationStatus.onStateChangeForTesting(mChromeActivity, ActivityState.DESTROYED);
            ApplicationStatus.unregisterActivityStateListener(mActivityStateListener);
        });
        mMockDirectory.tearDown();
    }

    private TabPersistentStore buildTabPersistentStore(final TabPersistencePolicy persistencePolicy,
            final TabModelSelector modelSelector, final TabCreatorManager creatorManager) {
        return TestThreadUtils.runOnUiThreadBlockingNoException(() -> {
            return new TabPersistentStore(persistencePolicy, modelSelector, creatorManager);
        });
    }

    private static TabbedModeTabPersistencePolicy createTabPersistencePolicy(
            int selectorIndex, boolean mergeTabs, boolean tabMergingEnabled) {
        return TestThreadUtils.runOnUiThreadBlockingNoException(() -> {
            return new TabbedModeTabPersistencePolicy(selectorIndex, mergeTabs, tabMergingEnabled,
                    TabWindowManagerSingleton.getInstance().getMaxSimultaneousSelectors());
        });
    }

    @Test
    @SmallTest
    @Feature("TabPersistentStore")
    @ParameterAnnotations.UseMethodParameter(StoreParamProvider.class)
    public void testBasic(boolean isCriticalPersistedTabDataEnabled) throws Exception {
        ChromeFeatureList.sCriticalPersistedTabData.setForTesting(
                isCriticalPersistedTabDataEnabled);
        TabModelMetaDataInfo info = TestTabModelDirectory.TAB_MODEL_METADATA_V4;
        int numExpectedTabs = info.contents.length;

        mMockDirectory.writeTabModelFiles(info, true);

        // Set up the TabPersistentStore.
        MockTabModelSelector mockSelector =
                TestThreadUtils.runOnUiThreadBlocking(() -> new MockTabModelSelector(0, 0, null));
        MockTabCreatorManager mockManager = new MockTabCreatorManager(mockSelector);
        MockTabCreator regularCreator = mockManager.getTabCreator(false);
        MockTabPersistentStoreObserver mockObserver = new MockTabPersistentStoreObserver();
        TabPersistencePolicy persistencePolicy = createTabPersistencePolicy(0, false, true);
        final TabPersistentStore store =
                buildTabPersistentStore(persistencePolicy, mockSelector, mockManager);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            store.addObserver(mockObserver);

            // Should not prefetch with no prior active tab preference stored.
            Assert.assertNull(store.getPrefetchTabStateActiveTabTaskForTesting());

            // Make sure the metadata file loads properly and in order.
            store.loadState(false /* ignoreIncognitoFiles */);
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
        TestThreadUtils.runOnUiThreadBlocking(() -> { store.restoreTabs(true); });
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
    @DisableFeatures({ChromeFeatureList.CRITICAL_PERSISTED_TAB_DATA + "<Study"})
    public void testCleanup() throws TimeoutException, ExecutionException {
        MockTabModelSelector mockSelector = TestThreadUtils.runOnUiThreadBlocking(
                () -> { return new MockTabModelSelector(1, 1, null); });
        Tab regularTab = mockSelector.getModel(false).getTabAt(0);
        Tab incognitoTab = mockSelector.getModel(true).getTabAt(0);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            enableTabSaving(regularTab);
            enableTabSaving(incognitoTab);
            CriticalPersistedTabData.from(regularTab).save();
            CriticalPersistedTabData.from(incognitoTab).save();
        });
        verifyIfTabIsSaved(regularTab.getId(), regularTab.isIncognito(), false);
        verifyIfTabIsSaved(incognitoTab.getId(), incognitoTab.isIncognito(), false);

        MockTabCreatorManager mockManager = new MockTabCreatorManager(mockSelector);
        TabPersistencePolicy persistencePolicy =
                createTabPersistencePolicy(/* selectorIndex = */ 0, false, true);
        final TabPersistentStore store =
                buildTabPersistentStore(persistencePolicy, mockSelector, mockManager);
        verifyIfTabIsSaved(regularTab.getId(), regularTab.isIncognito(), true);
        verifyIfTabIsSaved(incognitoTab.getId(), incognitoTab.isIncognito(), true);
    }

    public void verifyIfTabIsSaved(int tabId, boolean isIncognito, boolean isNull)
            throws TimeoutException {
        CPTDCallbackHelper callbackHelper = new CPTDCallbackHelper();
        int chCount = callbackHelper.getCallCount();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            CriticalPersistedTabData.restore(
                    tabId, isIncognito, (res) -> { callbackHelper.notifyCalled(res); });
        });
        callbackHelper.waitForCallback(chCount);
        Assert.assertEquals(isNull, callbackHelper.getRes() == null);
    }

    private static class CPTDCallbackHelper extends CallbackHelper {
        private SerializedCriticalPersistedTabData mSerializedCriticalPersistedTabData;

        /**
         * Called when {@link SerializedCriticalPersistedTabData} is acquired
         * @param res {@link SerializedCriticalPersistedTabData} acquired
         */
        public void notifyCalled(
                SerializedCriticalPersistedTabData serializedCriticalPersistedTabData) {
            mSerializedCriticalPersistedTabData = serializedCriticalPersistedTabData;
            notifyCalled();
        }

        /**
         * @return ByteBuffer acquired during callback
         */
        public SerializedCriticalPersistedTabData getRes() {
            return mSerializedCriticalPersistedTabData;
        }
    }

    private void enableTabSaving(Tab tab) {
        tab.setIsTabSaveEnabled(true);
        ((TabImpl) tab).registerTabSaving();
        CriticalPersistedTabData.from(tab).setShouldSaveForTesting(true);
    }

    @Test
    @SmallTest
    @Feature({"TabPersistentStore"})
    @ParameterAnnotations.UseMethodParameter(StoreParamProvider.class)
    public void testInterruptedButStillRestoresAllTabs(boolean isCriticalPersistedTabDataEnabled)
            throws Exception {
        ChromeFeatureList.sCriticalPersistedTabData.setForTesting(
                isCriticalPersistedTabDataEnabled);
        TabModelMetaDataInfo info = TestTabModelDirectory.TAB_MODEL_METADATA_V4;
        int numExpectedTabs = info.contents.length;

        mMockDirectory.writeTabModelFiles(info, true);

        // Load up one TabPersistentStore, but don't load up the TabState files.  This prevents the
        // Tabs from being added to the TabModel.
        MockTabModelSelector firstSelector =
                TestThreadUtils.runOnUiThreadBlocking(() -> new MockTabModelSelector(0, 0, null));
        MockTabCreatorManager firstManager = new MockTabCreatorManager(firstSelector);
        MockTabPersistentStoreObserver firstObserver = new MockTabPersistentStoreObserver();
        TabPersistencePolicy firstPersistencePolicy = createTabPersistencePolicy(0, false, true);
        final TabPersistentStore firstStore =
                buildTabPersistentStore(firstPersistencePolicy, firstSelector, firstManager);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            firstStore.addObserver(firstObserver);
            firstStore.loadState(false /* ignoreIncognitoFiles */);
        });
        firstObserver.initializedCallback.waitForCallback(0, 1);
        Assert.assertEquals(numExpectedTabs, firstObserver.mTabCountAtStartup);
        firstObserver.detailsReadCallback.waitForCallback(0, numExpectedTabs);

        TestThreadUtils.runOnUiThreadBlocking(() -> { firstStore.saveState(); });

        // Prepare a second TabPersistentStore.
        MockTabModelSelector secondSelector =
                TestThreadUtils.runOnUiThreadBlocking(() -> new MockTabModelSelector(0, 0, null));
        MockTabCreatorManager secondManager = new MockTabCreatorManager(secondSelector);
        MockTabCreator secondCreator = secondManager.getTabCreator(false);
        MockTabPersistentStoreObserver secondObserver = new MockTabPersistentStoreObserver();
        TabPersistencePolicy secondPersistencePolicy = createTabPersistencePolicy(0, false, true);

        final TabPersistentStore secondStore =
                buildTabPersistentStore(secondPersistencePolicy, secondSelector, secondManager);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            secondStore.addObserver(secondObserver);

            // The second TabPersistentStore reads the file written by the first TabPersistentStore.
            // Make sure that all of the Tabs appear in the new one -- even though the new file was
            // written before the first TabPersistentStore loaded any TabState files and added them
            // to the TabModels.
            secondStore.loadState(false /* ignoreIncognitoFiles */);
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
        TestThreadUtils.runOnUiThreadBlocking(() -> { secondStore.restoreTabs(true); });

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
                TestThreadUtils.runOnUiThreadBlocking(() -> new MockTabModelSelector(0, 0, null));
        MockTabCreatorManager mockManager = new MockTabCreatorManager(mockSelector);
        MockTabPersistentStoreObserver mockObserver = new MockTabPersistentStoreObserver();
        TabPersistencePolicy persistencePolicy = createTabPersistencePolicy(0, false, true);
        final TabPersistentStore store =
                buildTabPersistentStore(persistencePolicy, mockSelector, mockManager);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            store.addObserver(mockObserver);

            // Make sure the metadata file loads properly and in order.
            store.loadState(false /* ignoreIncognitoFiles */);
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

        TestThreadUtils.runOnUiThreadBlocking(() -> {
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
                TestThreadUtils.runOnUiThreadBlocking(() -> new MockTabModelSelector(0, 0, null));
        MockTabCreatorManager mockManager = new MockTabCreatorManager(mockSelector);
        MockTabPersistentStoreObserver mockObserver = new MockTabPersistentStoreObserver();
        TabPersistencePolicy persistencePolicy = createTabPersistencePolicy(0, false, true);
        final TabPersistentStore store =
                buildTabPersistentStore(persistencePolicy, mockSelector, mockManager);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            store.addObserver(mockObserver);

            // Load the TabModel metadata.
            store.loadState(false /* ignoreIncognitoFiles */);
        });
        mockObserver.initializedCallback.waitForCallback(0, 1);
        Assert.assertEquals(numExpectedTabs, mockObserver.mTabCountAtStartup);
        mockObserver.detailsReadCallback.waitForCallback(0, numExpectedTabs);
        Assert.assertEquals(numExpectedTabs, mockObserver.details.size());

        // TODO(dfalcantara): Expand MockTabModel* to support Incognito Tab decryption.

        // Restore the TabStates, and confirm that the correct number of tabs is created even with
        // one missing.  No Incognito tabs should be created because the TabState is missing.
        TestThreadUtils.runOnUiThreadBlocking(() -> { store.restoreTabs(true); });
        mockObserver.stateLoadedCallback.waitForCallback(0, 1);
        Assert.assertEquals(info.numRegularTabs, mockSelector.getModel(false).getCount());
        Assert.assertEquals(0, mockSelector.getModel(true).getCount());
    }

    @Test
    @SmallTest
    @Feature({"TabPersistentStore"})
    public void testFallbackTabStateEmptyByteBufferCriticalPersistedTabData() throws Exception {
        ChromeFeatureList.sCriticalPersistedTabData.setForTesting(true);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            // Always return Empty Byte buffer (ByteBuffer with limit = 0 from storage)
            PersistedTabDataConfiguration.setUseEmptyByteBufferTestConfig(true);
        });
        TabModelMetaDataInfo info = TestTabModelDirectory.TAB_MODEL_METADATA_V5_WITH_INCOGNITO;
        int numExpectedTabs = info.contents.length;

        // Write out Tab Model and TabState files
        mMockDirectory.writeTabModelFiles(info, false);
        for (int i = 0; i < info.contents.length; i++) {
            mMockDirectory.writeTabStateFile(info.contents[i]);
        }

        // Initialize the classes.
        MockTabModelSelector mockSelector =
                TestThreadUtils.runOnUiThreadBlocking(() -> new MockTabModelSelector(0, 0, null));
        MockTabCreatorManager mockManager = new MockTabCreatorManager(mockSelector);
        MockTabPersistentStoreObserver mockObserver = new MockTabPersistentStoreObserver();
        TabPersistencePolicy persistencePolicy = createTabPersistencePolicy(0, false, true);
        final TabPersistentStore store =
                buildTabPersistentStore(persistencePolicy, mockSelector, mockManager);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            store.addObserver(mockObserver);

            // Load the TabModel metadata.
            store.loadState(false /* ignoreIncognitoFiles */);
        });
        mockObserver.initializedCallback.waitForCallback(0, 1);
        Assert.assertEquals(numExpectedTabs, mockObserver.mTabCountAtStartup);
        mockObserver.detailsReadCallback.waitForCallback(0, numExpectedTabs);
        Assert.assertEquals(numExpectedTabs, mockObserver.details.size());

        // Restore the TabStates, and confirm that the correct number of tabs is created.
        TestThreadUtils.runOnUiThreadBlocking(() -> { store.restoreTabs(true); });
        mockObserver.stateLoadedCallback.waitForCallback(0, 1);
        Assert.assertEquals(info.numRegularTabs, mockSelector.getModel(false).getCount());
        // No incognito TabState files were written.
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

        MockTabModelSelector mockSelector = new MockTabModelSelector(0, 0, null);
        MockTabCreatorManager mockManager = new MockTabCreatorManager(mockSelector);
        final MockTabPersistentStoreObserver mockObserver = new MockTabPersistentStoreObserver();
        TabPersistencePolicy persistencePolicy = createTabPersistencePolicy(0, false, true);
        final TabPersistentStore store =
                buildTabPersistentStore(persistencePolicy, mockSelector, mockManager);

        // Without loading state, simulate three tabs pending restore, then save state to write
        // out to disk.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            store.addObserver(mockObserver);

            store.addTabToRestoreForTesting(
                    new TabRestoreDetails(regularTab.tabId, 0, false, regularTab.url, false));
            store.addTabToRestoreForTesting(
                    new TabRestoreDetails(incognitoTab.tabId, 0, true, incognitoTab.url, false));
            store.addTabToRestoreForTesting(
                    new TabRestoreDetails(regularTab2.tabId, 1, false, regularTab2.url, false));

            store.saveState();
            store.destroy();
        });

        TabModelMetaDataInfo info = new TabModelMetaDataInfo(
                5, 1, 1, new TabStateInfo[] {incognitoTab, regularTab, regularTab2}, null);

        // Create and restore a real tab model, validating proper state. Incognito cannot be
        // restored since there are no state files for individual tabs on disk (just a tab metadata
        // file).
        TestTabModelSelector testSelector = createAndRestoreRealTabModelImpls(info, false, false);
        MockTabPersistentStoreObserver otherMockObserver = testSelector.mTabPersistentStoreObserver;

        // Assert state on tab details restored from metadata file.
        Assert.assertTrue("First restored tab should be incognito.",
                otherMockObserver.details.get(0).isIncognito);
        Assert.assertEquals("Incorrect URL for first restored tab.", incognitoTab.url,
                otherMockObserver.details.get(0).url);

        Assert.assertFalse("Second restored tab should be regular.",
                otherMockObserver.details.get(1).isIncognito);
        Assert.assertEquals("Incorrect URL for second restored tab.", regularTab.url,
                otherMockObserver.details.get(1).url);

        Assert.assertFalse("Third restored tab should be regular.",
                otherMockObserver.details.get(2).isIncognito);
        Assert.assertEquals("Incorrect URL for third restored tab.", regularTab2.url,
                otherMockObserver.details.get(2).url);
    }

    @Test
    @SmallTest
    @Feature({"TabPersistentStore"})
    @ParameterAnnotations.UseMethodParameter(StoreParamProvider.class)
    public void testPrefetchActiveTab(boolean isCriticalPersistedTabDataEnabled) throws Exception {
        ChromeFeatureList.sCriticalPersistedTabData.setForTesting(
                isCriticalPersistedTabDataEnabled);
        final TabModelMetaDataInfo info = TestTabModelDirectory.TAB_MODEL_METADATA_V5_NO_M18;
        mMockDirectory.writeTabModelFiles(info, true);

        // Set to pre-fetch
        mPreferences.writeInt(ChromePreferenceKeys.TABMODEL_ACTIVE_TAB_ID, info.selectedTabId);

        // Initialize the classes.
        MockTabModelSelector mockSelector =
                TestThreadUtils.runOnUiThreadBlocking(() -> new MockTabModelSelector(0, 0, null));
        MockTabCreatorManager mockManager = new MockTabCreatorManager(mockSelector);
        MockTabPersistentStoreObserver mockObserver = new MockTabPersistentStoreObserver();
        TabPersistencePolicy persistencePolicy = createTabPersistencePolicy(0, false, true);
        final TabPersistentStore store =
                buildTabPersistentStore(persistencePolicy, mockSelector, mockManager);
        TestThreadUtils.runOnUiThreadBlocking(() -> store.addObserver(mockObserver));
        store.waitForMigrationToFinish();

        if (isCriticalPersistedTabDataEnabled) {
            Assert.assertNotNull(
                    store.getPrefetchCriticalPersistedTabDataActiveTabTaskForTesting());
        } else {
            Assert.assertNotNull(store.getPrefetchTabStateActiveTabTaskForTesting());
        }

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            store.loadState(false /* ignoreIncognitoFiles */);
            store.restoreTabs(true);
        });

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            // Confirm that the pre-fetched active tab state was used, must be done here on the
            // UI thread as the message to finish the task is posted here.
            if (isCriticalPersistedTabDataEnabled) {
                Assert.assertEquals(AsyncTask.Status.FINISHED,
                        store.getPrefetchCriticalPersistedTabDataActiveTabTaskForTesting()
                                .getStatus());
            } else {
                Assert.assertEquals(AsyncTask.Status.FINISHED,
                        store.getPrefetchTabStateActiveTabTaskForTesting().getStatus());
            }

            // Confirm that the correct active tab ID is updated when saving state.
            mPreferences.writeInt(ChromePreferenceKeys.TABMODEL_ACTIVE_TAB_ID, -1);

            store.saveState();
        });

        Assert.assertEquals(info.selectedTabId,
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
            if (i == 0) {
                Assert.assertEquals(info.selectedTabId, selector.getCurrentTab().getId());
            } else {
                Assert.assertEquals(info.contents[0].tabId, selector.getCurrentTab().getId());
            }

            TestThreadUtils.runOnUiThreadBlocking(() -> {
                closeAllTabsThenUndo(selector, info);

                // Synchronously save the data out to simulate minimizing Chrome.
                selector.mTabPersistentStore.saveState();
            });

            // Load up each TabState and confirm that values are still correct.
            for (int j = 0; j < info.numRegularTabs; j++) {
                if (restoredFromDisk(selector.getModel(false).getTabAt(j))) {
                    TabState currentState = TabStateFileManager.restoreTabState(
                            mMockDirectory.getDataDirectory(), info.contents[j].tabId);
                    Assert.assertEquals(info.contents[j].title,
                            currentState.contentsState.getDisplayTitleFromState());
                    Assert.assertEquals(info.contents[j].url,
                            currentState.contentsState.getVirtualUrlFromState());
                }
            }
        }
    }

    /**
     * Determines if {@link Tab} was restored from disk or not. Assumes the {@link Tab} was
     * restored from disk if there was not record of how it was created.
     */
    private static boolean restoredFromDisk(Tab tab) throws ExecutionException {
        return TestThreadUtils.runOnUiThreadBlocking(new Callable<Boolean>() {
            @Override
            public Boolean call() {
                if (tab.getUserDataHost().getUserData(MockTabAttributes.class) == null) {
                    return true;
                }
                return tab.getUserDataHost().getUserData(MockTabAttributes.class).restoredFromDisk;
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

    @Test
    @SmallTest
    @Feature("TabPersistentStore")
    public void testMigrateStaleTabsToCriticalPersistedTabData() throws Exception {
        ChromeFeatureList.sCriticalPersistedTabData.setForTesting(true);
        Pair<TabPersistentStore, Tab[]> res = restoreTabsFromTabStateAndPrepareForSaving();
        TabPersistentStore store = res.first;
        Tab[] restoredTabs = res.second;

        // Save the TabState for 2 Tabs. There are info.numRegularTabs (7) Tabs
        // to migrate and we migrate MIGRATE_TO_CRITICAL_PERSISTED_TAB_DATA_DEFAULT_BATCH_SIZE (5)
        // Tabs for every TabState save, so we need to save 2 Tabs to migrate all
        // Tabs.
        addTabsToSaveQueue(store, new Tab[] {restoredTabs[0], restoredTabs[1]});
        // Verify all Tabs except the first are migrated. The first Tab has an empty
        // URL so it won't be saved (this is by design).
        for (int i = 1; i < restoredTabs.length; i++) {
            Tab tab = restoredTabs[i];
            CriteriaHelper.pollUiThread(
                    () -> FilePersistedTabDataStorage.exists(tab.getId(), false));
        }
    }

    @Test
    @SmallTest
    @Feature("TabPersistentStore")
    public void testDontMigrateDestroyedTab() throws Exception {
        // Variation on testMigrateStaleTabsToCriticalPersistedTabData. See comments in
        // testMigrateStaleTabsToCriticalPersistedTabData.
        ChromeFeatureList.sCriticalPersistedTabData.setForTesting(true);
        Pair<TabPersistentStore, Tab[]> res = restoreTabsFromTabStateAndPrepareForSaving();
        TabPersistentStore store = res.first;
        Tab[] restoredTabs = res.second;

        TestThreadUtils.runOnUiThreadBlocking(restoredTabs[2] ::destroy);
        addTabsToSaveQueue(store, new Tab[] {restoredTabs[0], restoredTabs[1]});

        for (int i = 1; i < restoredTabs.length; i++) {
            // restoredTabs[2] shouldn't have been migrated because it was destroyed.
            if (i == 2) continue;
            Tab tab = restoredTabs[i];
            CriteriaHelper.pollUiThread(
                    () -> FilePersistedTabDataStorage.exists(tab.getId(), false));
        }
        // Check we didn't migrate restoredTabs[2].
        Assert.assertFalse(FilePersistedTabDataStorage.exists(restoredTabs[2].getId(), false));
    }

    @Test
    @SmallTest
    @Feature("TabPersistentStore")
    public void testDontMigrateClosedTab() throws Exception {
        // Variation on testMigrateStaleTabsToCriticalPersistedTabData. See comments in
        // testMigrateStaleTabsToCriticalPersistedTabData
        ChromeFeatureList.sCriticalPersistedTabData.setForTesting(true);
        Pair<TabPersistentStore, Tab[]> res = restoreTabsFromTabStateAndPrepareForSaving();
        TabPersistentStore store = res.first;
        Tab[] restoredTabs = res.second;

        addTabsToSaveQueue(store, new Tab[] {restoredTabs[0], restoredTabs[1]});
        store.removeTabFromQueues(restoredTabs[2]);

        for (int i = 1; i < restoredTabs.length; i++) {
            // restoredTabs[2] should not have been migrated because it was removed from the
            // save queue.
            if (i == 2) continue;
            Tab tab = restoredTabs[i];
            CriteriaHelper.pollUiThread(
                    () -> FilePersistedTabDataStorage.exists(tab.getId(), false));
        }
        // Check we didn't migrate Tab 2.
        Assert.assertFalse(FilePersistedTabDataStorage.exists(restoredTabs[2].getId(), false));
    }

    private Pair<TabPersistentStore, Tab[]> restoreTabsFromTabStateAndPrepareForSaving()
            throws Exception {
        TabModelMetaDataInfo info = TestTabModelDirectory.TAB_MODEL_METADATA_V4;
        int numExpectedTabs = info.contents.length;
        mMockDirectory.writeTabModelFiles(info, true);
        MockTabModelSelector mockSelector =
                TestThreadUtils.runOnUiThreadBlocking(() -> new MockTabModelSelector(0, 0, null));
        MockTabCreatorManager mockManager = new MockTabCreatorManager(mockSelector);
        MockTabCreator regularCreator = mockManager.getTabCreator(false);
        MockTabPersistentStoreObserver mockObserver = new MockTabPersistentStoreObserver();
        TabPersistencePolicy persistencePolicy = createTabPersistencePolicy(0, false, true);
        final TabPersistentStore store =
                buildTabPersistentStore(persistencePolicy, mockSelector, mockManager);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            store.addObserver(mockObserver);
            store.loadState(false /* ignoreIncognitoFiles */);
        });
        mockObserver.initializedCallback.waitForCallback(0, 1);
        mockObserver.detailsReadCallback.waitForCallback(0, numExpectedTabs);
        TestThreadUtils.runOnUiThreadBlocking(() -> { store.restoreTabs(true); });
        regularCreator.callback.waitForCallback(0, 1);
        mockObserver.stateLoadedCallback.waitForCallback(0, 1);
        Tab[] restoredTabs = new Tab[info.numRegularTabs];
        for (int i = 0; i < info.numRegularTabs; i++) {
            restoredTabs[i] = mockSelector.getModel(false).getTabAt(i);
        }
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            for (Tab tab : restoredTabs) {
                ((TabImpl) tab).registerTabSaving();
                CriticalPersistedTabData.from(tab).setShouldSave();
            }
        });
        return Pair.create(store, restoredTabs);
    }

    private void addTabsToSaveQueue(TabPersistentStore store, Tab[] tabsToSave) {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            for (int i = 0; i < tabsToSave.length; i++) {
                // Tabs are uninitialized so TabState won't save unless we override here.
                // It doesn't matter what TabState is saved for the tests which use this function
                // only that it is saved. So an arbitrary TabState is used.
                TabStateExtractor.setTabStateForTesting(tabsToSave[i].getId(), new TabState());
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
     *
     * @param info TabModelMetaDataInfo to check restore tab models against.
     * @param restoreIncognito Whether incognito tabs should be restored. In order for restore to
     *                         succeed, there must be a readable tab state file on disk.
     * @param expectMatchingIds Whether restored tab id's are expected to match those in
     *                          {@coe info}. If there is no tab state file for a given entry in the
     *                          metadata file, TabPersistentStore currently creates a new tab with
     *                          the last known URL, in which case the new tab's id won't match the
     *                          id in the metadata file.
     * @return A {@link TestTabModelSelector} with the restored tabs.
     * @throws Exception
     */
    private TestTabModelSelector createAndRestoreRealTabModelImpls(TabModelMetaDataInfo info,
            boolean restoreIncognito, boolean expectMatchingIds) throws Exception {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ApplicationStatus.onStateChangeForTesting(mChromeActivity, ActivityState.STARTED);
        });

        TestTabModelSelector selector =
                TestThreadUtils.runOnUiThreadBlocking(new Callable<TestTabModelSelector>() {
                    @Override
                    public TestTabModelSelector call() {
                        // Clear any existing TestTabModelSelector (required when
                        // createAndRestoreRealTabModelImpls is called multiple times in one test).
                        sTabWindowManager.onActivityStateChange(
                                mChromeActivity, ActivityState.DESTROYED);
                        return (TestTabModelSelector) sTabWindowManager
                                .requestSelector(mChromeActivity, mChromeActivity, null, 0)
                                .second;
                    }
                });

        final TabPersistentStore store = selector.mTabPersistentStore;
        MockTabPersistentStoreObserver mockObserver = selector.mTabPersistentStoreObserver;

        // Load up the TabModel metadata.
        int numExpectedTabs = info.numRegularTabs + info.numIncognitoTabs;
        TestThreadUtils.runOnUiThreadBlocking(
                () -> store.loadState(!restoreIncognito /* ignoreIncognitoFiles */));
        mockObserver.initializedCallback.waitForCallback(0, 1);
        Assert.assertEquals(numExpectedTabs, mockObserver.mTabCountAtStartup);
        mockObserver.detailsReadCallback.waitForCallback(0, info.contents.length);

        Assert.assertEquals(numExpectedTabs, mockObserver.details.size());

        // Restore the TabStates, check that things were restored correctly, in the right tab order.
        TestThreadUtils.runOnUiThreadBlocking(() -> { store.restoreTabs(true); });
        mockObserver.stateLoadedCallback.waitForCallback(0, 1);

        int numIncognitoExpected = restoreIncognito ? info.numIncognitoTabs : 0;
        Assert.assertEquals("Incorrect number of regular tabs.", info.numRegularTabs,
                selector.getModel(false).getCount());
        Assert.assertEquals("Incorrect number of incognito tabs.", numIncognitoExpected,
                selector.getModel(true).getCount());

        int tabInfoIndex = info.numIncognitoTabs;
        for (int i = 0; i < info.numRegularTabs; i++) {
            Tab tab = selector.getModel(false).getTabAt(i);

            if (expectMatchingIds) {
                if (restoredFromDisk(tab)) {
                    Assert.assertEquals("Incorrect regular tab at position " + i,
                            info.contents[tabInfoIndex].tabId, tab.getId());
                } else {
                    String url = TestThreadUtils.runOnUiThreadBlocking(() -> {
                        return CriticalPersistedTabData.from(tab).getUrl().getSpec();
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
                    Assert.assertEquals("Incorrect incognito tab at position " + i,
                            info.contents[i].tabId, tab.getId());
                } else {
                    String url = TestThreadUtils.runOnUiThreadBlocking(() -> {
                        return CriticalPersistedTabData.from(tab).getUrl().getSpec();
                    });
                    Assert.assertEquals(
                            "Unexpected URL on Tab", info.contents[tabInfoIndex].url, url);
                }
            }
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
        TabModelObserver closeObserver = new TabModelObserver() {
            @Override
            public void multipleTabsPendingClosure(List<Tab> tabs, boolean isAllTabs) {
                for (Tab tab : tabs) closedTabIds.add(tab.getId());
            }
        };
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            regularModel.addObserver(closeObserver);
            regularModel.closeAllTabs(false);
        });
        Assert.assertEquals(info.numRegularTabs, closedTabIds.size());

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            // Cancel closing each tab.
            for (Integer id : closedTabIds) regularModel.cancelTabClosure(id);
        });
        Assert.assertEquals(info.numRegularTabs, regularModel.getCount());
    }
}

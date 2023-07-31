// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import static org.junit.Assert.assertEquals;

import android.content.Context;

import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.base.test.UiThreadTest;
import org.chromium.base.test.params.ParameterAnnotations.UseMethodParameter;
import org.chromium.base.test.params.ParameterAnnotations.UseRunnerDelegate;
import org.chromium.base.test.params.ParameterProvider;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.WarmupManager.SpareTabFinalStatus;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.init.ChromeBrowserInitializer;
import org.chromium.chrome.browser.profiles.OTRProfileID;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.chrome.test.util.browser.signin.AccountManagerTestRule;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.content_public.browser.test.util.WebContentsUtils;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.net.test.util.TestWebServer;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.concurrent.Callable;
import java.util.concurrent.Semaphore;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicReference;

/** Tests for {@link WarmupManager} */
@RunWith(ParameterizedRunner.class)
@Batch(Batch.PER_CLASS)
@UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class WarmupManagerTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    public enum ProfileType { REGULAR_PROFILE, PRIMARY_OTR_PROFILE, NON_PRIMARY_OTR_PROFILE }
    private static final String HISTOGRAM_SPARE_TAB_FINAL_STATUS = "Android.SpareTab.FinalStatus";
    private static final String MAIN_FRAME_FILE = "/main_frame.html";

    /** Provides parameter for testPreconnect to run it with both regular and incognito profiles.*/
    public static class ProfileParams implements ParameterProvider {
        @Override
        public Iterable<ParameterSet> getParameters() {
            return Arrays.asList(new ParameterSet()
                                         .value(ProfileType.PRIMARY_OTR_PROFILE.toString())
                                         .name("PrimaryIncognitoProfile"),
                    new ParameterSet()
                            .value(ProfileType.NON_PRIMARY_OTR_PROFILE.toString())
                            .name("NonPrimaryIncognitoProfile"),
                    new ParameterSet()
                            .value(ProfileType.REGULAR_PROFILE.toString())
                            .name("RegularProfile"));
        }
    }

    @Rule
    public final AccountManagerTestRule mAccountManagerTestRule = new AccountManagerTestRule();

    private WarmupManager mWarmupManager;
    private Context mContext;

    private TestWebServer mWebServer;
    private TabModel mTabModel;
    private TabGroupModelFilter mTabGroupModelFilter;

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.startMainActivityFromLauncher();

        mTabModel = mActivityTestRule.getActivity().getTabModelSelector().getModel(false);

        mTabGroupModelFilter = (TabGroupModelFilter) mActivityTestRule.getActivity()
                                       .getTabModelSelector()
                                       .getTabModelFilterProvider()
                                       .getTabModelFilter(false);

        // Unlike most of Chrome, the WarmupManager inflates layouts with the application context.
        // This is because the inflation happens before an activity exists. If you're trying to fix
        // a failing test, it's important to not add extra theme/style information to this context
        // in this test because it could hide a real production issue. See https://crbug.com/1246329
        // for an example.
        mContext = InstrumentationRegistry.getInstrumentation()
                           .getTargetContext()
                           .getApplicationContext();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ChromeBrowserInitializer.getInstance().handleSynchronousStartup();
            mWarmupManager = WarmupManager.getInstance();
        });
        mWebServer = TestWebServer.start();
    }

    @After
    public void tearDown() {
        TestThreadUtils.runOnUiThreadBlocking(() -> mWarmupManager.destroySpareWebContents());
        TestThreadUtils.runOnUiThreadBlocking(() -> mWarmupManager.destroySpareTab());
        WarmupManager.deInitForTesting();
        mWebServer.shutdown();
    }

    private void assertOrderValid(boolean expectedState) {
        boolean isOrderValid = TestThreadUtils.runOnUiThreadBlockingNoException(
                () -> { return mTabGroupModelFilter.isOrderValid(); });
        assertEquals(expectedState, isOrderValid);
    }

    /**
     * Helper methods to create tabs, adding new tabs, and getting current tabs in the tab model.
     */
    private void prepareTabs(List<Integer> tabsPerGroup) {
        for (int tabsToCreate : tabsPerGroup) {
            List<Tab> tabs = new ArrayList<>();
            for (int i = 0; i < tabsToCreate; i++) {
                Tab tab = ChromeTabUtils.fullyLoadUrlInNewTab(
                        InstrumentationRegistry.getInstrumentation(),
                        mActivityTestRule.getActivity(), "about:blank", /*incognito=*/false);
                tabs.add(tab);
            }
            TestThreadUtils.runOnUiThreadBlocking(() -> {
                mTabGroupModelFilter.mergeListOfTabsToGroup(tabs, tabs.get(0), false, false);
            });
        }
    }

    private Tab addTabAt(int index, Tab parent) {
        final String data = "<html><head></head><body><p>Hello World</p></body></html>";
        final String url = mWebServer.setResponse(MAIN_FRAME_FILE, data, null);

        Tab tab = TestThreadUtils.runOnUiThreadBlockingNoException(() -> {
            @TabLaunchType
            int type =
                    parent != null ? TabLaunchType.FROM_TAB_GROUP_UI : TabLaunchType.FROM_CHROME_UI;
            TabCreator tabCreator =
                    mActivityTestRule.getActivity().getTabCreator(/*incognito=*/false);
            return tabCreator.createNewTab(new LoadUrlParams(url), type, parent, index);
        });
        return tab;
    }

    private List<Tab> getCurrentTabs() {
        List<Tab> tabs = new ArrayList<>();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            for (int i = 0; i < mTabModel.getCount(); i++) {
                tabs.add(mTabModel.getTabAt(i));
            }
        });
        return tabs;
    }

    private static Profile getNonPrimaryOTRProfile() {
        return TestThreadUtils.runOnUiThreadBlockingNoException((Callable<Profile>) () -> {
            OTRProfileID otrProfileID = OTRProfileID.createUnique("CCT:Incognito");
            return Profile.getLastUsedRegularProfile().getOffTheRecordProfile(
                    otrProfileID, /*createIfNeeded=*/true);
        });
    }

    private static Profile getPrimaryOTRProfile() {
        return TestThreadUtils.runOnUiThreadBlockingNoException(
                (Callable<Profile>) ()
                        -> Profile.getLastUsedRegularProfile().getPrimaryOTRProfile(
                                /*createIfNeeded=*/true));
    }

    private static Profile getRegularProfile() {
        return TestThreadUtils.runOnUiThreadBlockingNoException(
                (Callable<Profile>) () -> Profile.getLastUsedRegularProfile());
    }

    private static Profile getProfile(ProfileType profileType) {
        switch (profileType) {
            case NON_PRIMARY_OTR_PROFILE:
                return getNonPrimaryOTRProfile();
            case PRIMARY_OTR_PROFILE:
                return getPrimaryOTRProfile();
            default:
                return getRegularProfile();
        }
    }

    @Test
    @SmallTest
    public void testCreateAndTakeSpareRenderer() {
        final AtomicBoolean isRenderFrameLive = new AtomicBoolean();
        final AtomicReference<WebContents> webContentsReference = new AtomicReference<>();

        PostTask.runOrPostTask(TaskTraits.UI_DEFAULT, () -> {
            mWarmupManager.createSpareWebContents();
            Assert.assertTrue(mWarmupManager.hasSpareWebContents());
            WebContents webContents = mWarmupManager.takeSpareWebContents(false, false);
            Assert.assertNotNull(webContents);
            Assert.assertFalse(mWarmupManager.hasSpareWebContents());

            if (webContents.getMainFrame().isRenderFrameLive()) {
                isRenderFrameLive.set(true);
            }

            webContentsReference.set(webContents);
        });
        CriteriaHelper.pollUiThread(
                () -> isRenderFrameLive.get(), "Spare renderer is not initialized");
        PostTask.runOrPostTask(TaskTraits.UI_DEFAULT, () -> webContentsReference.get().destroy());
    }

    /** Tests that taking a spare WebContents makes it unavailable to subsequent callers. */
    @Test
    @SmallTest
    @UiThreadTest
    public void testTakeSpareWebContents() {
        mWarmupManager.createSpareWebContents();
        WebContents webContents = mWarmupManager.takeSpareWebContents(false, false);
        Assert.assertNotNull(webContents);
        Assert.assertFalse(mWarmupManager.hasSpareWebContents());
        webContents.destroy();
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testTakeSpareWebContentsChecksArguments() {
        mWarmupManager.createSpareWebContents();
        Assert.assertNull(mWarmupManager.takeSpareWebContents(true, false));
        Assert.assertNull(mWarmupManager.takeSpareWebContents(true, true));
        Assert.assertTrue(mWarmupManager.hasSpareWebContents());
        Assert.assertNotNull(mWarmupManager.takeSpareWebContents(false, true));
        Assert.assertFalse(mWarmupManager.hasSpareWebContents());
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testClearsDeadWebContents() {
        mWarmupManager.createSpareWebContents();
        WebContentsUtils.simulateRendererKilled(mWarmupManager.mSpareWebContents);
        Assert.assertNull(mWarmupManager.takeSpareWebContents(false, false));
    }

    /** Checks that the View inflation works. */
    @Test
    @SmallTest
    @UiThreadTest
    public void testInflateLayout() {
        int layoutId = R.layout.custom_tabs_control_container;
        int toolbarId = R.layout.custom_tabs_toolbar;
        mWarmupManager.initializeViewHierarchy(mContext, layoutId, toolbarId);
        Assert.assertTrue(mWarmupManager.hasViewHierarchyWithToolbar(layoutId));
    }

    /**
     * Tests that pre-connects can be initiated from the Java side.
     *
     * @param profileParameter String value to indicate which profile to use for pre-connect. This
     *         is passed by {@link ProfileParams}.
     * @throws InterruptedException May come from tryAcquire method call.
     */
    @Test
    @SmallTest
    @UseMethodParameter(ProfileParams.class)
    public void testPreconnect(String profileParameter) throws InterruptedException {
        ProfileType profileType = ProfileType.valueOf(profileParameter);
        Profile profile = getProfile(profileType);
        EmbeddedTestServer server = new EmbeddedTestServer();
        // The predictor prepares 2 connections when asked to preconnect. Initializes the
        // semaphore to be unlocked after 2 connections.
        final Semaphore connectionsSemaphore = new Semaphore(1 - 2);
        // Cannot use EmbeddedTestServer#createAndStartServer(), as we need to add the
        // connection listener.
        server.initializeNative(mContext, EmbeddedTestServer.ServerHTTPSSetting.USE_HTTP);
        server.addDefaultHandlers("");
        server.setConnectionListener(new EmbeddedTestServer.ConnectionListener() {
            @Override
            public void acceptedSocket(long socketId) {
                connectionsSemaphore.release();
            }
        });
        server.start();
        final String url = server.getURL("/hello_world.html");
        PostTask.runOrPostTask(TaskTraits.UI_DEFAULT,
                () -> { mWarmupManager.maybePreconnectUrlAndSubResources(profile, url); });
        boolean isAcquired = connectionsSemaphore.tryAcquire(5, TimeUnit.SECONDS);
        if (profileType == ProfileType.REGULAR_PROFILE && !isAcquired) {
            // Starts at -1.
            int actualConnections = connectionsSemaphore.availablePermits() + 1;
            Assert.fail("Pre-connect failed for regular profile: Expected 2 connections, got "
                    + actualConnections);
        } else if (profileType != ProfileType.REGULAR_PROFILE && isAcquired) {
            Assert.fail("Pre-connect should fail for incognito profiles.");
        }
    }

    // Test to check the functionality of spare tab creation with initializing renderer.
    // TODO(crbug.com/1412572): Add tests to track navigation related WebContentsObserver events
    // with spare tab creation.
    @Test
    @MediumTest
    @Feature({"SpareTab"})
    @EnableFeatures({ChromeFeatureList.SPARE_TAB})
    public void testCreateAndTakeSpareTabWithInitializeRenderer() {
        // Set the param to true allowing renderer initialization.
        WarmupManager.SPARE_TAB_INITIALIZE_RENDERER.setForTesting(true);

        final AtomicBoolean isRenderFrameLive = new AtomicBoolean();

        PostTask.runOrPostTask(TaskTraits.UI_DEFAULT, () -> {
            mWarmupManager.createSpareTab(mActivityTestRule.getActivity().getCurrentTabCreator(),
                    TabLaunchType.FROM_START_SURFACE);
            Assert.assertTrue(mWarmupManager.hasSpareTab());
            Tab tab = mWarmupManager.takeSpareTab(false, TabLaunchType.FROM_START_SURFACE);
            WebContents webContents = tab.getWebContents();
            Assert.assertNotNull(tab);
            Assert.assertNotNull(webContents);
            Assert.assertFalse(mWarmupManager.hasSpareTab());

            // RenderFrame should become live synchronously during WebContents creation when
            // SPARE_TAB_INITIALIZE_RENDERER is set.
            if (webContents.getMainFrame().isRenderFrameLive()) {
                isRenderFrameLive.set(true);
            }
        });
        CriteriaHelper.pollUiThread(
                () -> isRenderFrameLive.get(), "Spare renderer is not initialized");
    }

    // Test to check the functionality of spare tab creation without initializing renderer.
    // Disable CreateNewTabInitializeRenderer to test spare tab without renderer initialization.
    @Test
    @MediumTest
    @Feature({"SpareTab"})
    @EnableFeatures({ChromeFeatureList.SPARE_TAB})
    @DisableFeatures(ChromeFeatureList.CREATE_NEW_TAB_INITIALIZE_RENDERER)
    public void testCreateAndTakeSpareTabWithoutInitializeRenderer() {
        WarmupManager.SPARE_TAB_INITIALIZE_RENDERER.setForTesting(false);

        final AtomicBoolean isRenderFrameLive = new AtomicBoolean();

        PostTask.runOrPostTask(TaskTraits.UI_DEFAULT, () -> {
            mWarmupManager.createSpareTab(mActivityTestRule.getActivity().getCurrentTabCreator(),
                    TabLaunchType.FROM_START_SURFACE);
            Assert.assertTrue(mWarmupManager.hasSpareTab());
            Tab tab = mWarmupManager.takeSpareTab(false, TabLaunchType.FROM_START_SURFACE);
            WebContents webContents = tab.getWebContents();
            Assert.assertNotNull(tab);
            Assert.assertNotNull(webContents);
            Assert.assertFalse(mWarmupManager.hasSpareTab());

            // RenderFrame shouldn't be created when the SPARE_TAB_INITIALIZE_RENDERER is false.
            Assert.assertFalse(webContents.getMainFrame().isRenderFrameLive());
        });
        CriteriaHelper.pollUiThread(
                () -> !isRenderFrameLive.get(), "Spare renderer is initialized");
    }

    /** Tests that taking a spare Tab makes it unavailable to subsequent callers. */
    @Test
    @MediumTest
    @Feature({"SpareTab"})
    @EnableFeatures({ChromeFeatureList.SPARE_TAB})
    @UiThreadTest
    public void testTakeSpareTab() {
        var histogramWatcher = HistogramWatcher.newSingleRecordWatcher(
                HISTOGRAM_SPARE_TAB_FINAL_STATUS, SpareTabFinalStatus.TAB_USED);
        mWarmupManager.createSpareTab(mActivityTestRule.getActivity().getCurrentTabCreator(),
                TabLaunchType.FROM_START_SURFACE);
        Tab tab = mWarmupManager.takeSpareTab(false, TabLaunchType.FROM_START_SURFACE);
        Assert.assertNotNull(tab);
        Assert.assertFalse(mWarmupManager.hasSpareTab());
        histogramWatcher.assertExpected();
    }

    /**
     * Tests that deleting a spare Tab makes it unavailable to subsequent callers and record
     * correct metrics.
     */
    @Test
    @MediumTest
    @Feature({"SpareTab"})
    @EnableFeatures({ChromeFeatureList.SPARE_TAB})
    @UiThreadTest
    public void testDestroySpareTab() {
        var histogramWatcher = HistogramWatcher.newSingleRecordWatcher(
                HISTOGRAM_SPARE_TAB_FINAL_STATUS, SpareTabFinalStatus.TAB_DESTROYED);

        mWarmupManager.createSpareTab(mActivityTestRule.getActivity().getCurrentTabCreator(),
                TabLaunchType.FROM_START_SURFACE);
        Assert.assertTrue(mWarmupManager.hasSpareTab());

        // Destroy the created spare tab.
        mWarmupManager.destroySpareTab();
        Assert.assertFalse(mWarmupManager.hasSpareTab());

        histogramWatcher.assertExpected();
    }

    /** Tests that when SpareTab is not destroyed when the renderer is killed. */
    @Test
    @MediumTest
    @Feature({"SpareTab"})
    @EnableFeatures(ChromeFeatureList.SPARE_TAB)
    @UiThreadTest
    public void testDontDestroySpareTabWhenRendererKilled() {
        // Set the param to true allowing renderer initialization.
        WarmupManager.SPARE_TAB_INITIALIZE_RENDERER.setForTesting(true);

        var histogramWatcher = HistogramWatcher.newSingleRecordWatcher(
                HISTOGRAM_SPARE_TAB_FINAL_STATUS, SpareTabFinalStatus.TAB_USED);

        mWarmupManager.createSpareTab(mActivityTestRule.getActivity().getCurrentTabCreator(),
                TabLaunchType.FROM_START_SURFACE);

        // Kill the renderer process, this shouldn't kill the associated spare tab and record
        // TAB_CREATED status.
        WebContentsUtils.simulateRendererKilled(mWarmupManager.mSpareTab.getWebContents());
        Assert.assertNotNull(mWarmupManager.takeSpareTab(false, TabLaunchType.FROM_START_SURFACE));

        histogramWatcher.assertExpected();
    }

    /** Tests that when SpareTab is disabled we don't create any spare tab. */
    @Test
    @MediumTest
    @Feature({"SpareTab"})
    @DisableFeatures(ChromeFeatureList.SPARE_TAB)
    @UiThreadTest
    public void testTakeSpareTabWhenFeatureDisabled() {
        Assert.assertNotNull(mActivityTestRule.getActivity().getCurrentTabCreator());
        mWarmupManager.createSpareTab(mActivityTestRule.getActivity().getCurrentTabCreator(),
                TabLaunchType.FROM_START_SURFACE);
        Tab tab = mWarmupManager.takeSpareTab(false, TabLaunchType.FROM_START_SURFACE);
        Assert.assertNull(tab);
    }

    /** Tests that we are able to load url in the spare tab once it is created. */
    @Test
    @MediumTest
    @Feature({"SpareTab"})
    @EnableFeatures({ChromeFeatureList.SPARE_TAB})
    public void testLoadURLInSpareTab() {
        var histogramWatcher = HistogramWatcher.newSingleRecordWatcher(
                HISTOGRAM_SPARE_TAB_FINAL_STATUS, SpareTabFinalStatus.TAB_USED);
        Assert.assertNotNull(mActivityTestRule.getActivity().getCurrentTabCreator());

        // Create spare tab so that it can be used for navigation from TAB_GROUP_UI.
        PostTask.runOrPostTask(TaskTraits.UI_DEFAULT, () -> {
            mWarmupManager.createSpareTab(mActivityTestRule.getActivity().getCurrentTabCreator(),
                    TabLaunchType.FROM_TAB_GROUP_UI);
            Assert.assertTrue(mWarmupManager.hasSpareTab());
        });

        prepareTabs(Arrays.asList(new Integer[] {3, 1}));
        List<Tab> tabs = getCurrentTabs();

        // Tab 0
        // Tab (tab added here), 1, 2, 3
        // Tab 4 - this uses spare tab.
        Tab tab = addTabAt(/*index=*/0, /*parent=*/tabs.get(1));
        tabs.add(1, tab);
        assertEquals(tabs, getCurrentTabs());
        assertOrderValid(true);

        PostTask.runOrPostTask(
                TaskTraits.UI_DEFAULT, () -> { Assert.assertFalse(mWarmupManager.hasSpareTab()); });
        histogramWatcher.assertExpected();
    }

    /** Tests that page load metrics are recorded when the spare tab is used for navigation */
    @Test
    @MediumTest
    @Feature({"SpareTab"})
    @EnableFeatures({ChromeFeatureList.SPARE_TAB})
    public void testMetricsRecordedWithSpareTab() {
        Assert.assertNotNull(mActivityTestRule.getActivity().getCurrentTabCreator());

        // Create spare tab so that it can be used for navigation from TAB_GROUP_UI.
        PostTask.runOrPostTask(TaskTraits.UI_DEFAULT, () -> {
            mWarmupManager.createSpareTab(mActivityTestRule.getActivity().getCurrentTabCreator(),
                    TabLaunchType.FROM_TAB_GROUP_UI);
            Assert.assertTrue(mWarmupManager.hasSpareTab());
        });

        prepareTabs(Arrays.asList(new Integer[] {1, 1}));
        List<Tab> tabs = getCurrentTabs();

        // Check that the First Paint (FP) and First Contentful Paint (FCP) metrics are recorded
        // correctly when using the SpareTab feature.
        var pageLoadHistogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectAnyRecordTimes("PageLoad.PaintTiming.NavigationToFirstPaint", 1)
                        .expectAnyRecordTimes(
                                "PageLoad.PaintTiming.NavigationToFirstContentfulPaint", 1)
                        .build();

        // Navigate and this should record PageLoadMetrics.
        Tab tab = addTabAt(/*index=*/0, /*parent=*/tabs.get(1));
        tabs.add(1, tab);

        // PageLoadMetrics should be recorded when SpareTab is used for navigation.
        PostTask.runOrPostTask(
                TaskTraits.UI_DEFAULT, () -> { Assert.assertFalse(mWarmupManager.hasSpareTab()); });
        pageLoadHistogramWatcher.pollInstrumentationThreadUntilSatisfied();
    }

    /**
     * Tests that we are able to create new tab along with initializing renderer when the feature
     * CREATE_NEW_TAB_INITIALIZE_RENDERER is enabled.
     */
    @Test
    @MediumTest
    @Feature({"SpareTab"})
    @UiThreadTest
    @EnableFeatures(
            {ChromeFeatureList.SPARE_TAB, ChromeFeatureList.CREATE_NEW_TAB_INITIALIZE_RENDERER})
    public void
    testOnTabCreationWithInitializeRenderer() {
        mWarmupManager.createSpareTab(mActivityTestRule.getActivity().getCurrentTabCreator(),
                TabLaunchType.FROM_TAB_GROUP_UI);
        Assert.assertTrue(mWarmupManager.hasSpareTab());

        Tab tab = mWarmupManager.takeSpareTab(false, TabLaunchType.FROM_TAB_GROUP_UI);
        WebContents webContents = tab.getWebContents();
        Assert.assertNotNull(tab);
        Assert.assertNotNull(webContents);

        // RenderFrame should be created when the CREATE_NEW_TAB_INITIALIZE_RENDERER is enabled.
        Assert.assertTrue(webContents.getMainFrame().isRenderFrameLive());
    }
}

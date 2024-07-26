// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotEquals;

import android.content.Context;

import androidx.test.annotation.UiThreadTest;
import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
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
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.WarmupManager.SpareTabFinalStatus;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.init.ChromeBrowserInitializer;
import org.chromium.chrome.browser.profiles.OTRProfileID;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.batch.BlankCTATabInitialStateRule;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.browser.signin.AccountManagerTestRule;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.test.util.WebContentsUtils;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.net.test.util.TestWebServer;
import org.chromium.ui.display.DisplayUtil;
import org.chromium.ui.test.util.DeviceRestriction;

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
    @ClassRule
    public static ChromeTabbedActivityTestRule sActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Rule
    public BlankCTATabInitialStateRule mBlankCTATabInitialStateRule =
            new BlankCTATabInitialStateRule(sActivityTestRule, false);

    public enum ProfileType {
        REGULAR_PROFILE,
        PRIMARY_OTR_PROFILE,
        NON_PRIMARY_OTR_PROFILE
    }

    private static final String HISTOGRAM_SPARE_TAB_FINAL_STATUS = "Android.SpareTab.FinalStatus";
    private static final String MAIN_FRAME_FILE = "/main_frame.html";

    /** Provides parameter for testPreconnect to run it with both regular and incognito profiles. */
    public static class ProfileParams implements ParameterProvider {
        @Override
        public Iterable<ParameterSet> getParameters() {
            return Arrays.asList(
                    new ParameterSet()
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
        mTabModel = sActivityTestRule.getActivity().getTabModelSelector().getModel(false);

        mTabGroupModelFilter =
                (TabGroupModelFilter)
                        sActivityTestRule
                                .getActivity()
                                .getTabModelSelector()
                                .getTabModelFilterProvider()
                                .getTabModelFilter(false);

        // Unlike most of Chrome, the WarmupManager inflates layouts with the application context.
        // This is because the inflation happens before an activity exists. If you're trying to fix
        // a failing test, it's important to not add extra theme/style information to this context
        // in this test because it could hide a real production issue. See https://crbug.com/1246329
        // for an example.
        mContext =
                InstrumentationRegistry.getInstrumentation()
                        .getTargetContext()
                        .getApplicationContext();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ChromeBrowserInitializer.getInstance().handleSynchronousStartup();
                    mWarmupManager = WarmupManager.getInstance();
                });
        mWebServer = TestWebServer.start();
    }

    @After
    public void tearDown() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mWarmupManager.destroySpareWebContents();
                    mWarmupManager.destroySpareTab();
                    WarmupManager.deInitForTesting();
                });
        mWebServer.shutdown();
    }

    private void assertOrderValid(boolean expectedState) {
        boolean isOrderValid =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            return mTabGroupModelFilter.isOrderValid();
                        });
        assertEquals(expectedState, isOrderValid);
    }

    /**
     * Helper methods to create tabs, adding new tabs, and getting current tabs in the tab model.
     */
    private void prepareTabs(List<Integer> tabsPerGroup) {
        for (int tabsToCreate : tabsPerGroup) {
            List<Tab> tabs = new ArrayList<>();
            for (int i = 0; i < tabsToCreate; i++) {
                Tab tab =
                        ChromeTabUtils.fullyLoadUrlInNewTab(
                                InstrumentationRegistry.getInstrumentation(),
                                sActivityTestRule.getActivity(),
                                "about:blank",
                                /* incognito= */ false);
                tabs.add(tab);
            }
            ThreadUtils.runOnUiThreadBlocking(
                    () -> {
                        mTabGroupModelFilter.mergeListOfTabsToGroup(tabs, tabs.get(0), false);
                    });
        }
    }

    private Tab addTabAt(int index, Tab parent) {
        final String data = "<html><head></head><body><p>Hello World</p></body></html>";
        final String url = mWebServer.setResponse(MAIN_FRAME_FILE, data, null);

        Tab tab =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            @TabLaunchType
                            int type =
                                    parent != null
                                            ? TabLaunchType.FROM_TAB_GROUP_UI
                                            : TabLaunchType.FROM_CHROME_UI;
                            TabCreator tabCreator =
                                    sActivityTestRule
                                            .getActivity()
                                            .getTabCreator(/* incognito= */ false);
                            return tabCreator.createNewTab(
                                    new LoadUrlParams(url), type, parent, index);
                        });
        ChromeTabUtils.waitForTabPageLoaded(tab, url);
        return tab;
    }

    private List<Tab> getCurrentTabs() {
        List<Tab> tabs = new ArrayList<>();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    for (int i = 0; i < mTabModel.getCount(); i++) {
                        tabs.add(mTabModel.getTabAt(i));
                    }
                });
        return tabs;
    }

    private static Profile getNonPrimaryOTRProfile() {
        return ThreadUtils.runOnUiThreadBlocking(
                (Callable<Profile>)
                        () -> {
                            OTRProfileID otrProfileID = OTRProfileID.createUnique("CCT:Incognito");
                            return ProfileManager.getLastUsedRegularProfile()
                                    .getOffTheRecordProfile(
                                            otrProfileID, /* createIfNeeded= */ true);
                        });
    }

    private static Profile getPrimaryOTRProfile() {
        return ThreadUtils.runOnUiThreadBlocking(
                (Callable<Profile>)
                        () ->
                                ProfileManager.getLastUsedRegularProfile()
                                        .getPrimaryOTRProfile(/* createIfNeeded= */ true));
    }

    private static Profile getRegularProfile() {
        return ThreadUtils.runOnUiThreadBlocking(
                (Callable<Profile>) () -> ProfileManager.getLastUsedRegularProfile());
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

        PostTask.runOrPostTask(
                TaskTraits.UI_DEFAULT,
                () -> {
                    mWarmupManager.createSpareWebContents(sActivityTestRule.getProfile(false));
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
        mWarmupManager.createSpareWebContents(sActivityTestRule.getProfile(false));
        WebContents webContents = mWarmupManager.takeSpareWebContents(false, false);
        Assert.assertNotNull(webContents);
        Assert.assertFalse(mWarmupManager.hasSpareWebContents());
        webContents.destroy();
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testTakeSpareWebContentsChecksArguments() {
        mWarmupManager.createSpareWebContents(sActivityTestRule.getProfile(false));
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
        mWarmupManager.createSpareWebContents(sActivityTestRule.getProfile(false));
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
        Assert.assertTrue(mWarmupManager.hasViewHierarchyWithToolbar(layoutId, mContext));
    }

    /**
     * Tests that pre-connects can be initiated from the Java side.
     *
     * @param profileParameter String value to indicate which profile to use for pre-connect. This
     *     is passed by {@link ProfileParams}.
     * @throws InterruptedException May come from tryAcquire method call.
     */
    @Test
    @SmallTest
    @UseMethodParameter(ProfileParams.class)
    public void testPreconnect(String profileParameter) throws InterruptedException {
        ProfileType profileType = ProfileType.valueOf(profileParameter);
        Profile profile = getProfile(profileType);
        EmbeddedTestServer server = new EmbeddedTestServer();
        // The predictor prepares 1 or 2 connections when asked to preconnect. Initializes the
        // semaphore to be unlocked after 1 or 2 connections.
        int expectedConnections =
                ChromeFeatureList.isEnabled(
                                ChromeFeatureList.LOADING_PREDICTOR_LIMIT_PRECONNECT_SOCKET_COUNT)
                        ? 1
                        : 2;
        final Semaphore connectionsSemaphore = new Semaphore(1 - expectedConnections);
        // Cannot use EmbeddedTestServer#createAndStartServer(), as we need to add the
        // connection listener.
        server.initializeNative(mContext, EmbeddedTestServer.ServerHTTPSSetting.USE_HTTP);
        server.addDefaultHandlers("");
        server.setConnectionListener(
                new EmbeddedTestServer.ConnectionListener() {
                    @Override
                    public void acceptedSocket(long socketId) {
                        connectionsSemaphore.release();
                    }
                });
        server.start();
        final String url = server.getURL("/hello_world.html");
        PostTask.runOrPostTask(
                TaskTraits.UI_DEFAULT,
                () -> {
                    mWarmupManager.maybePreconnectUrlAndSubResources(profile, url);
                });
        boolean isAcquired = connectionsSemaphore.tryAcquire(5, TimeUnit.SECONDS);
        if (profileType == ProfileType.REGULAR_PROFILE && !isAcquired) {
            // Starts at -1.
            int actualConnections = connectionsSemaphore.availablePermits() + 1;
            Assert.fail(
                    String.format(
                            "Pre-connect failed for regular profile: Expected %d connections, got"
                                    + " %d",
                            expectedConnections, actualConnections));
        } else if (profileType != ProfileType.REGULAR_PROFILE && isAcquired) {
            Assert.fail("Pre-connect should fail for incognito profiles.");
        }
    }

    // Test to check the functionality of spare tab creation with initializing renderer.
    // TODO(crbug.com/40255340): Add tests to track navigation related WebContentsObserver events
    // with spare tab creation.
    @Test
    @MediumTest
    @Feature({"SpareTab"})
    public void testCreateAndTakeSpareTabWithInitializeRenderer() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Profile profile = getProfile(ProfileType.REGULAR_PROFILE);
                    mWarmupManager.createRegularSpareTab(profile);
                    Assert.assertTrue(mWarmupManager.hasSpareTab(profile));
                    Tab tab = mWarmupManager.takeSpareTab(profile, TabLaunchType.FROM_CHROME_UI);
                    WebContents webContents = tab.getWebContents();
                    Assert.assertNotNull(tab);
                    Assert.assertNotNull(webContents);
                    Assert.assertFalse(mWarmupManager.hasSpareTab(profile));
                    Assert.assertEquals(TabLaunchType.FROM_CHROME_UI, tab.getLaunchType());

                    // RenderFrame should become live synchronously during WebContents creation when
                    // SPARE_TAB_INITIALIZE_RENDERER is set.
                    Assert.assertTrue(webContents.getMainFrame().isRenderFrameLive());
                    tab.destroy();
                });
    }

    /** Tests that taking a spare Tab makes it unavailable to subsequent callers. */
    @Test
    @MediumTest
    @Feature({"SpareTab"})
    @UiThreadTest
    public void testTakeSpareTab() {
        var histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        HISTOGRAM_SPARE_TAB_FINAL_STATUS, SpareTabFinalStatus.TAB_USED);
        Profile profile = getProfile(ProfileType.REGULAR_PROFILE);
        mWarmupManager.createRegularSpareTab(profile);
        Tab tab = mWarmupManager.takeSpareTab(profile, TabLaunchType.FROM_CHROME_UI);
        Assert.assertNotNull(tab);
        Assert.assertFalse(mWarmupManager.hasSpareTab(profile));
        Assert.assertEquals(TabLaunchType.FROM_CHROME_UI, tab.getLaunchType());
        histogramWatcher.assertExpected();
        tab.destroy();
    }

    /**
     * Tests that deleting a spare Tab makes it unavailable to subsequent callers and record correct
     * metrics.
     */
    @Test
    @MediumTest
    @Feature({"SpareTab"})
    @UiThreadTest
    public void testDestroySpareTab() {
        var histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        HISTOGRAM_SPARE_TAB_FINAL_STATUS, SpareTabFinalStatus.TAB_DESTROYED);

        Profile profile = getProfile(ProfileType.REGULAR_PROFILE);
        mWarmupManager.createRegularSpareTab(profile);
        Assert.assertTrue(mWarmupManager.hasSpareTab(profile));
        Assert.assertFalse(mWarmupManager.hasSpareTab(getProfile(ProfileType.PRIMARY_OTR_PROFILE)));

        // Destroy the created spare tab.
        mWarmupManager.destroySpareTab();
        Assert.assertFalse(mWarmupManager.hasSpareTab(profile));

        histogramWatcher.assertExpected();
    }

    /** Tests that when SpareTab is not destroyed when the renderer is killed. */
    @Test
    @MediumTest
    @Feature({"SpareTab"})
    @UiThreadTest
    public void testDontDestroySpareTabWhenRendererKilled() {
        var histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        HISTOGRAM_SPARE_TAB_FINAL_STATUS, SpareTabFinalStatus.TAB_USED);

        Profile profile = getProfile(ProfileType.REGULAR_PROFILE);
        mWarmupManager.createRegularSpareTab(profile);

        // Kill the renderer process, this shouldn't kill the associated spare tab and record
        // TAB_CREATED status.
        WebContentsUtils.simulateRendererKilled(mWarmupManager.mSpareTab.getWebContents());
        Tab tab = mWarmupManager.takeSpareTab(profile, TabLaunchType.FROM_CHROME_UI);
        Assert.assertNotNull(tab);
        histogramWatcher.assertExpected();
        tab.destroy();
    }

    /** Tests that we are able to load url in the spare tab once it is created. */
    @Test
    @MediumTest
    @Feature({"SpareTab"})
    public void testLoadURLInSpareTab() {
        var histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        HISTOGRAM_SPARE_TAB_FINAL_STATUS, SpareTabFinalStatus.TAB_USED);
        Assert.assertNotNull(sActivityTestRule.getActivity().getCurrentTabCreator());

        prepareTabs(Arrays.asList(new Integer[] {3, 1}));
        List<Tab> tabs = getCurrentTabs();

        Profile profile = getProfile(ProfileType.REGULAR_PROFILE);
        // Create spare tab so that it can be used for navigation from TAB_GROUP_UI.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mWarmupManager.createRegularSpareTab(profile);
                    Assert.assertTrue(mWarmupManager.hasSpareTab(profile));
                });

        // Tab 0
        // Tab (tab added here), 1, 2, 3
        // Tab 4 - this uses spare tab.
        Tab tab = addTabAt(/* index= */ 0, /* parent= */ tabs.get(1));
        tabs.add(1, tab);
        assertEquals(tabs, getCurrentTabs());
        assertOrderValid(true);
        Assert.assertEquals(TabLaunchType.FROM_TAB_GROUP_UI, tab.getLaunchType());

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertFalse(mWarmupManager.hasSpareTab(profile));
                });
        histogramWatcher.assertExpected();
    }

    /** Tests that page load metrics are recorded when the spare tab is used for navigation */
    @Test
    @MediumTest
    @Feature({"SpareTab"})
    public void testMetricsRecordedWithSpareTab() {
        Assert.assertNotNull(sActivityTestRule.getActivity().getCurrentTabCreator());

        prepareTabs(Arrays.asList(new Integer[] {1, 1}));
        List<Tab> tabs = getCurrentTabs();

        Profile profile = getProfile(ProfileType.REGULAR_PROFILE);
        // Create spare tab so that it can be used for navigation from TAB_GROUP_UI.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mWarmupManager.createRegularSpareTab(profile);
                    Assert.assertTrue(mWarmupManager.hasSpareTab(profile));
                });

        // Check that the First Paint (FP) and First Contentful Paint (FCP) metrics are recorded
        // correctly when using the SpareTab feature.
        var pageLoadHistogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectAnyRecordTimes("PageLoad.PaintTiming.NavigationToFirstPaint", 1)
                        .expectAnyRecordTimes(
                                "PageLoad.PaintTiming.NavigationToFirstContentfulPaint", 1)
                        .build();

        // Navigate and this should record PageLoadMetrics.
        Tab tab = addTabAt(/* index= */ 0, /* parent= */ tabs.get(1));
        tabs.add(1, tab);
        Assert.assertEquals(TabLaunchType.FROM_TAB_GROUP_UI, tab.getLaunchType());

        // PageLoadMetrics should be recorded when SpareTab is used for navigation.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertFalse(mWarmupManager.hasSpareTab(profile));
                });
        pageLoadHistogramWatcher.pollInstrumentationThreadUntilSatisfied();
    }

    @Test
    @SmallTest
    @Restriction({DeviceRestriction.RESTRICTION_TYPE_NON_AUTO})
    public void testApplyContextOverridesOnNonAutomotive() {
        Context baseContext = mContext.getApplicationContext();
        Context updatedContext = WarmupManager.applyContextOverrides(baseContext);

        assertEquals(
                "The updated context should be the same as the original context.",
                baseContext,
                updatedContext);
    }

    @Test
    @SmallTest
    @Restriction({DeviceRestriction.RESTRICTION_TYPE_AUTO})
    public void testApplyContextOverridesOnAutomotive() {
        Context baseContext = mContext.getApplicationContext();
        Context updatedContext = WarmupManager.applyContextOverrides(baseContext);

        assertNotEquals(
                "The updated context should be different from the original context.",
                baseContext,
                updatedContext);
        assertEquals(
                "The updated context should have a scaled up densityDpi",
                (int)
                        (baseContext.getResources().getDisplayMetrics().densityDpi
                                * DisplayUtil.getUiScalingFactorForAutomotive()),
                updatedContext.getResources().getDisplayMetrics().densityDpi);
    }
}

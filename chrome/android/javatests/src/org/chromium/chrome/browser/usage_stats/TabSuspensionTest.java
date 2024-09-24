// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.usage_stats;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doReturn;

import android.content.Context;
import android.content.Intent;
import android.media.AudioManager;
import android.os.Build;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.MediumTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.IntentUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.JniMocker;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.ChromeTabbedActivity2;
import org.chromium.chrome.browser.MockSafeBrowsingApiHandler;
import org.chromium.chrome.browser.customtabs.CustomTabActivityTestRule;
import org.chromium.chrome.browser.customtabs.CustomTabsIntentTestUtils;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.multiwindow.MultiWindowTestHelper;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tab.TabTestUtils;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.MenuUtils;
import org.chromium.components.safe_browsing.SafeBrowsingApiBridge;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.test.util.DOMUtils;
import org.chromium.content_public.common.ContentSwitches;
import org.chromium.media.MediaSwitches;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.PageTransition;

import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeoutException;

/** Integration tests for {@link PageViewObserver} and {@link SuspendedTab} */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({
    ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
    // Direct all hostnames to EmbeddedTestServer running on 127.0.0.1.
    ContentSwitches.HOST_RESOLVER_RULES + "=MAP * 127.0.0.1",
    "ignore-certificate-errors",
    MediaSwitches.AUTOPLAY_NO_GESTURE_REQUIRED_POLICY
})
@MinAndroidSdkLevel(Build.VERSION_CODES.Q)
public class TabSuspensionTest {
    private static final String STARTING_FQDN = "example.com";
    private static final String DIFFERENT_FQDN = "www.google.com";

    private static final String MEDIA_FILE_TEST_PATH =
            "/content/test/data/media/session/media-session.html";
    private static final String VIDEO_ID = "long-video";

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Rule
    public CustomTabActivityTestRule mCustomTabActivityTestRule = new CustomTabActivityTestRule();

    @Rule public JniMocker jniMocker = new JniMocker();

    @Mock private UsageStatsBridge.Natives mUsageStatsNativeMock;
    @Mock private UsageStatsBridge mUsageStatsBridge;
    @Mock private SuspensionTracker mSuspensionTracker;

    private ChromeTabbedActivity mActivity;
    private PageViewObserver mPageViewObserver;
    private PageViewObserver mPageViewObserver2;
    private TokenTracker mTokenTracker;
    private EventTracker mEventTracker;
    private Tab mTab;
    private EmbeddedTestServer mTestServer;
    private String mStartingUrl;
    private String mDifferentUrl;

    @Before
    public void setUp() throws InterruptedException {
        MockitoAnnotations.initMocks(this);
        SafeBrowsingApiBridge.setSafeBrowsingApiHandler(new MockSafeBrowsingApiHandler());
        jniMocker.mock(UsageStatsBridgeJni.TEST_HOOKS, mUsageStatsNativeMock);
        doReturn(123456L).when(mUsageStatsNativeMock).init(any(), any());
        // TokenTracker and EventTracker hold a promise, and Promises can only be used on a single
        // thread, so we have to initialize them on the thread where they will be used.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mTokenTracker = new TokenTracker(mUsageStatsBridge);
                    mEventTracker = new EventTracker(mUsageStatsBridge);
                });
        mTestServer =
                EmbeddedTestServer.createAndStartServer(
                        ApplicationProvider.getApplicationContext());
        mStartingUrl = mTestServer.getURLWithHostName(STARTING_FQDN, "/defaultresponse");
        mDifferentUrl = mTestServer.getURLWithHostName(DIFFERENT_FQDN, "/defaultresponse");

        mActivityTestRule.startMainActivityOnBlankPage();
        mActivity = mActivityTestRule.getActivity();
        mTab = mActivity.getActivityTab();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mPageViewObserver =
                            new PageViewObserver(
                                    mActivity,
                                    mActivity.getActivityTabProvider(),
                                    mEventTracker,
                                    mTokenTracker,
                                    mSuspensionTracker,
                                    mActivity.getTabContentManagerSupplier());
                });
    }

    @Test
    @MediumTest
    public void testNavigateToSuspended() {
        doReturn(true).when(mSuspensionTracker).isWebsiteSuspended(STARTING_FQDN);
        startLoadingUrl(mTab, mStartingUrl);
        waitForSuspendedTabToShow(mTab, STARTING_FQDN);

        startLoadingUrl(mTab, mDifferentUrl);
        ChromeTabUtils.waitForTabPageLoaded(mTab, mDifferentUrl);
        assertSuspendedTabHidden(mTab);
    }

    @Test
    @MediumTest
    public void testNavigateToSuspendedDomain_differentPage() {
        doReturn(true).when(mSuspensionTracker).isWebsiteSuspended(STARTING_FQDN);
        startLoadingUrl(mTab, mStartingUrl);
        waitForSuspendedTabToShow(mTab, STARTING_FQDN);

        startLoadingUrl(mTab, mStartingUrl + "foo.html");
        assertSuspendedTabShowing(mTab, STARTING_FQDN);
    }

    @Test
    @MediumTest
    public void testNewTabSuspended() {
        mActivityTestRule.loadUrl(mStartingUrl);

        doReturn(true).when(mSuspensionTracker).isWebsiteSuspended(DIFFERENT_FQDN);
        // We can't use loadUrlInNewTab because the site being suspended will prevent loading from
        // completing, and loadUrlInNewTab expects loading to succeed.
        ChromeTabUtils.newTabFromMenu(
                InstrumentationRegistry.getInstrumentation(), mActivityTestRule.getActivity());
        Tab tab2 = mActivity.getActivityTab();

        startLoadingUrl(tab2, mDifferentUrl);
        waitForSuspendedTabToShow(tab2, DIFFERENT_FQDN);
    }

    @Test
    @MediumTest
    public void testTabSwitchBackToSuspended() {
        mActivityTestRule.loadUrl(mStartingUrl);
        final int originalTabIndex =
                mActivity.getTabModelSelector().getCurrentModel().indexOf(mTab);
        Tab tab2 = mActivityTestRule.loadUrlInNewTab(mDifferentUrl);

        doReturn(true).when(mSuspensionTracker).isWebsiteSuspended(STARTING_FQDN);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mActivity
                            .getTabModelSelector()
                            .getCurrentModel()
                            .setIndex(originalTabIndex, TabSelectionType.FROM_USER);
                });
        waitForSuspendedTabToShow(mTab, STARTING_FQDN);
    }

    @Test
    @MediumTest
    @DisabledTest(message = "https://crbug.com/1345655")
    public void testEagerSuspension() {
        mActivityTestRule.loadUrl(mStartingUrl);
        CriteriaHelper.pollUiThread(() -> !mTab.isLoading());
        suspendDomain(STARTING_FQDN);
        waitForSuspendedTabToShow(mTab, STARTING_FQDN);

        // Suspending again shouldn't crash or otherwise affect the state of the world.
        suspendDomain(STARTING_FQDN);
        assertSuspendedTabShowing(mTab, STARTING_FQDN);

        // A single un-suspend should be sufficient even though we triggered suspension twice.
        unsuspendDomain(STARTING_FQDN);
        assertSuspendedTabHidden(mTab);
    }

    @Test
    @MediumTest
    @DisableIf.Build(sdk_is_greater_than = 29, message = "https://crbug.com/1036556")
    public void testMediaSuspension() throws TimeoutException {
        mActivityTestRule.loadUrl(
                mTestServer.getURLWithHostName(STARTING_FQDN, MEDIA_FILE_TEST_PATH));
        assertTrue(DOMUtils.isMediaPaused(mTab.getWebContents(), VIDEO_ID));
        DOMUtils.playMedia(mTab.getWebContents(), VIDEO_ID);
        DOMUtils.waitForMediaPlay(mTab.getWebContents(), VIDEO_ID);
        AudioManager audioManager =
                (AudioManager)
                        mActivityTestRule
                                .getActivity()
                                .getApplicationContext()
                                .getSystemService(Context.AUDIO_SERVICE);
        assertTrue(audioManager.isMusicActive());

        suspendDomain(STARTING_FQDN);
        waitForSuspendedTabToShow(mTab, STARTING_FQDN);
        DOMUtils.waitForMediaPauseBeforeEnd(mTab.getWebContents(), VIDEO_ID);
        CriteriaHelper.pollUiThread(
                () -> {
                    return !audioManager.isMusicActive();
                },
                "No audio should be playing",
                5000,
                50);

        unsuspendDomain(STARTING_FQDN);
        assertSuspendedTabHidden(mTab);
        DOMUtils.waitForMediaPlay(mTab.getWebContents(), VIDEO_ID);
        CriteriaHelper.pollUiThread(
                () -> {
                    return audioManager.isMusicActive();
                },
                "Audio should play after un-suspension",
                5000,
                50);
    }

    @Test
    @MediumTest
    @DisabledTest(message = "https://crbug.com/1345655")
    public void testMultiWindow() {
        mActivityTestRule.loadUrl(mStartingUrl);
        Tab tab2 = mActivityTestRule.loadUrlInNewTab(mDifferentUrl);
        CriteriaHelper.pollUiThread(() -> !tab2.isLoading());
        suspendDomain(DIFFERENT_FQDN);
        doReturn(true).when(mSuspensionTracker).isWebsiteSuspended(DIFFERENT_FQDN);
        waitForSuspendedTabToShow(tab2, DIFFERENT_FQDN);

        MultiWindowUtils.getInstance().setIsInMultiWindowModeForTesting(true);

        MenuUtils.invokeCustomMenuActionSync(
                InstrumentationRegistry.getInstrumentation(),
                mActivity,
                R.id.move_to_other_window_menu_id);
        final ChromeTabbedActivity2 activity2 =
                MultiWindowTestHelper.waitForSecondChromeTabbedActivity();
        // Each PageViewObserver is associated with a single ChromeTabbedActivity, so we need to
        // create a new one for the other window. This needs to be done on the UI thread since it
        // can trigger view manipulation.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mPageViewObserver2 =
                            new PageViewObserver(
                                    activity2,
                                    activity2.getActivityTabProvider(),
                                    mEventTracker,
                                    mTokenTracker,
                                    mSuspensionTracker,
                                    activity2.getTabContentManagerSupplier());
                });

        MultiWindowTestHelper.waitForTabs(
                "CTA", activity2, /* expectedTotalTabCount= */ 1, tab2.getId());
        waitForSuspendedTabToShow(tab2, DIFFERENT_FQDN);

        doReturn(true).when(mSuspensionTracker).isWebsiteSuspended(STARTING_FQDN);
        suspendDomain(STARTING_FQDN);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mPageViewObserver2.notifySiteSuspensionChanged(DIFFERENT_FQDN, false);
                });
        // Suspending and un-suspending should work in both activities/windows.
        assertSuspendedTabHidden(tab2);
        MultiWindowTestHelper.moveActivityToFront(mActivity);
        waitForSuspendedTabToShow(mTab, STARTING_FQDN);
    }

    @Test
    @MediumTest
    @DisableIf.Build(sdk_is_greater_than = 30, message = "https://crbug.com/1036556")
    public void testTabAddedFromCustomTab() {
        Intent intent =
                CustomTabsIntentTestUtils.createMinimalCustomTabIntent(
                        ApplicationProvider.getApplicationContext(), mStartingUrl);
        IntentUtils.addTrustedIntentExtras(intent);
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);
        doReturn(true).when(mSuspensionTracker).isWebsiteSuspended(STARTING_FQDN);

        MenuUtils.invokeCustomMenuActionSync(
                InstrumentationRegistry.getInstrumentation(),
                mCustomTabActivityTestRule.getActivity(),
                R.id.open_in_browser_id);

        MultiWindowTestHelper.waitForTabs("CustomTab", mActivity, 2, Tab.INVALID_TAB_ID);
        waitForSuspendedTabToShow(mActivity.getActivityTab(), STARTING_FQDN);
    }

    @Test
    @MediumTest
    public void testTabAddedInBackground() throws ExecutionException {
        Tab bgTab =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            return mActivity
                                    .getCurrentTabCreator()
                                    .createNewTab(
                                            new LoadUrlParams(mStartingUrl),
                                            TabLaunchType.FROM_LONGPRESS_BACKGROUND,
                                            mTab);
                        });
        ChromeTabUtils.waitForTabPageLoaded(bgTab, mStartingUrl);

        suspendDomain(STARTING_FQDN);
        assertSuspendedTabHidden(bgTab);
    }

    @Test
    @MediumTest
    public void testTabUnsuspendedInBackground() {
        doReturn(true).when(mSuspensionTracker).isWebsiteSuspended(STARTING_FQDN);
        startLoadingUrl(mTab, mStartingUrl);
        waitForSuspendedTabToShow(mTab, STARTING_FQDN);
        final int originalTabIndex =
                mActivity.getTabModelSelector().getCurrentModel().indexOf(mTab);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mPageViewObserver.notifySiteSuspensionChanged(STARTING_FQDN, false);
                    doReturn(false).when(mSuspensionTracker).isWebsiteSuspended(STARTING_FQDN);
                    mActivity
                            .getTabModelSelector()
                            .getCurrentModel()
                            .setIndex(originalTabIndex, TabSelectionType.FROM_USER);
                });

        assertSuspendedTabHidden(mTab);
    }

    @Test
    @MediumTest
    @DisabledTest(message = "https://crbug.com/1345655")
    public void testNavigationFromSuspendedTabToInterstitial() {
        doReturn(true).when(mSuspensionTracker).isWebsiteSuspended(STARTING_FQDN);
        startLoadingUrl(mTab, mStartingUrl);
        waitForSuspendedTabToShow(mTab, STARTING_FQDN);

        MockSafeBrowsingApiHandler.addMockResponse(
                mDifferentUrl, MockSafeBrowsingApiHandler.SOCIAL_ENGINEERING_CODE);
        startLoadingUrl(mTab, mDifferentUrl);

        waitForSuspendedTabToHide(mTab);
    }

    @Test
    @MediumTest
    public void testRendererCrashOnSuspendedTab() {
        doReturn(true).when(mSuspensionTracker).isWebsiteSuspended(STARTING_FQDN);
        startLoadingUrl(mTab, mStartingUrl);
        waitForSuspendedTabToShow(mTab, STARTING_FQDN);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    TabTestUtils.simulateCrash(mTab, true);
                    assertSuspendedTabHidden(mTab);
                });
    }

    @Test
    @MediumTest
    // TODO(crbug.com/339003346): Failing on tablets, fix and re-enable.
    @Restriction(DeviceFormFactor.PHONE)
    public void testSuspendNullCurrentTab() {
        mActivityTestRule.loadUrl(mStartingUrl);
        ChromeTabUtils.closeAllTabs(InstrumentationRegistry.getInstrumentation(), mActivity);

        doReturn(true).when(mSuspensionTracker).isWebsiteSuspended(STARTING_FQDN);
        suspendDomain(STARTING_FQDN);

        // We can't use loadUrlInNewTab because the site being suspended will prevent loading from
        // completing, and loadUrlInNewTab expects loading to succeed.
        ChromeTabUtils.newTabFromMenu(
                InstrumentationRegistry.getInstrumentation(), mActivityTestRule.getActivity());
        Tab tab2 = mActivity.getActivityTab();

        startLoadingUrl(tab2, mStartingUrl);
        waitForSuspendedTabToShow(tab2, STARTING_FQDN);
    }

    @Test
    @MediumTest
    public void testSuspendUninitializedCurrentTab() {
        mActivityTestRule.loadUrl(mStartingUrl);
        ThreadUtils.runOnUiThreadBlocking(() -> mTab.destroy());

        doReturn(true).when(mSuspensionTracker).isWebsiteSuspended(STARTING_FQDN);
        suspendDomain(STARTING_FQDN);
    }

    private void startLoadingUrl(Tab tab, String url) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    tab.loadUrl(new LoadUrlParams(url, PageTransition.TYPED));
                });
    }

    private void assertSuspendedTabHidden(Tab tab) {
        assertSuspendedTabState(tab, false, null);
    }

    private void assertSuspendedTabShowing(Tab tab, String fqdn) {
        assertSuspendedTabState(tab, true, fqdn);
    }

    private void assertSuspendedTabState(Tab tab, boolean showing, String fqdn) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    SuspendedTab suspendedTab =
                            SuspendedTab.from(tab, mActivity.getTabContentManagerSupplier());
                    assertEquals(suspendedTab.isShowing(), showing);
                    assertEquals(suspendedTab.isViewAttached(), showing);
                    assertTrue(
                            (suspendedTab.getFqdn() == null && fqdn == null)
                                    || fqdn.equals(suspendedTab.getFqdn()));
                });
    }

    private void suspendDomain(String domain) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mPageViewObserver.notifySiteSuspensionChanged(domain, true);
                });
    }

    private void unsuspendDomain(String domain) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mPageViewObserver.notifySiteSuspensionChanged(domain, false);
                });
    }

    private void waitForSuspendedTabToShow(Tab tab, String fqdn) {
        CriteriaHelper.pollUiThread(
                () -> {
                    return SuspendedTab.from(tab, mActivity.getTabContentManagerSupplier())
                            .isShowing();
                },
                "Suspended tab should be showing",
                10000,
                50);

        assertSuspendedTabShowing(tab, fqdn);
    }

    private void waitForSuspendedTabToHide(Tab tab) {
        CriteriaHelper.pollUiThread(
                () -> !SuspendedTab.from(tab, mActivity.getTabContentManagerSupplier()).isShowing(),
                "Suspended tab should be hidden",
                10000,
                50);
        assertSuspendedTabHidden(tab);
    }
}

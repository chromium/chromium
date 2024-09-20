// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import static org.hamcrest.Matchers.is;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;

import android.app.Activity;
import android.app.Instrumentation;
import android.content.Context;
import android.content.Intent;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.SmallTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.hamcrest.Matchers;
import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.RuleChain;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;

import org.chromium.base.ObserverList;
import org.chromium.base.ThreadUtils;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.app.metrics.LaunchCauseMetrics;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.crypto.CipherFactory;
import org.chromium.chrome.browser.customtabs.content.CustomTabIntentHandler;
import org.chromium.chrome.browser.customtabs.dependency_injection.BaseCustomTabActivityModule;
import org.chromium.chrome.browser.dependency_injection.ModuleOverridesRule;
import org.chromium.chrome.browser.firstrun.FirstRunStatus;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabHidingType;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tab.TabTestUtils;
import org.chromium.chrome.browser.theme.TopUiThemeColorProvider;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.browser.LocationSettingsTestUtil;
import org.chromium.content_public.browser.test.util.DOMUtils;
import org.chromium.content_public.browser.test.util.WebContentsUtils;
import org.chromium.net.test.EmbeddedTestServer;

import java.util.concurrent.TimeoutException;

/**
 * Tests for the "open in Chrome" functionality that reparents a tab from a CustomTabActivity to a
 * ChromeTabbedActivity.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
public class TabReparentingTest {
    private static final String TEST_PAGE = "/chrome/test/data/android/google.html";
    private static final String SELECT_POPUP_PAGE = "/chrome/test/data/android/select.html";

    @Rule
    public CustomTabActivityTestRule mCustomTabActivityTestRule = new CustomTabActivityTestRule();

    private final TestRule mModuleOverridesRule =
            new ModuleOverridesRule()
                    .setOverride(
                            BaseCustomTabActivityModule.Factory.class,
                            (BrowserServicesIntentDataProvider intentDataProvider,
                                    CustomTabNightModeStateController nightModeController,
                                    CustomTabIntentHandler.IntentIgnoringCriterion
                                            intentIgnoringCriterion,
                                    TopUiThemeColorProvider topUiThemeColorProvider,
                                    DefaultBrowserProviderImpl customTabDefaultBrowserProvider,
                                    CipherFactory cipherFactory) ->
                                    new BaseCustomTabActivityModule(
                                            intentDataProvider,
                                            nightModeController,
                                            intentIgnoringCriterion,
                                            topUiThemeColorProvider,
                                            new FakeDefaultBrowserProviderImpl(),
                                            cipherFactory));

    @Rule
    public RuleChain mRuleChain =
            RuleChain.emptyRuleChain()
                    .around(mCustomTabActivityTestRule)
                    .around(mModuleOverridesRule);

    private String mTestPage;
    private EmbeddedTestServer mTestServer;

    @Before
    public void setUp() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(() -> FirstRunStatus.setFirstRunFlowComplete(true));

        Context appContext =
                InstrumentationRegistry.getInstrumentation()
                        .getTargetContext()
                        .getApplicationContext();
        mTestServer = EmbeddedTestServer.createAndStartServer(appContext);
        mTestPage = mTestServer.getURL(TEST_PAGE);
        LibraryLoader.getInstance().ensureInitialized();
    }

    @After
    public void tearDown() {
        ThreadUtils.runOnUiThreadBlocking(() -> FirstRunStatus.setFirstRunFlowComplete(false));
    }

    private CustomTabActivity getActivity() {
        return mCustomTabActivityTestRule.getActivity();
    }

    /**
     * @see CustomTabsIntentTestUtils#createMinimalCustomTabIntent(Context, String).
     */
    private Intent createMinimalCustomTabIntent() {
        return CustomTabsIntentTestUtils.createMinimalCustomTabIntent(
                ApplicationProvider.getApplicationContext(), mTestPage);
    }

    private ChromeActivity reparentAndVerifyTab() {
        final Instrumentation.ActivityMonitor monitor =
                InstrumentationRegistry.getInstrumentation()
                        .addMonitor(
                                ChromeTabbedActivity.class.getName(), /* result= */ null, false);
        final Tab tabToBeReparented = getActivity().getActivityTab();
        final CallbackHelper tabHiddenHelper = new CallbackHelper();
        TabObserver observer =
                new EmptyTabObserver() {
                    @Override
                    public void onHidden(Tab tab, @TabHidingType int type) {
                        tabHiddenHelper.notifyCalled();
                    }
                };
        ThreadUtils.runOnUiThreadBlocking(() -> tabToBeReparented.addObserver(observer));
        PostTask.postTask(
                TaskTraits.UI_DEFAULT,
                () -> {
                    getActivity()
                            .getComponent()
                            .resolveNavigationController()
                            .openCurrentUrlInBrowser();
                    assertNull(getActivity().getActivityTab());
                });
        // Use the extended CriteriaHelper timeout to make sure we get an activity
        final Activity lastActivity =
                monitor.waitForActivityWithTimeout(CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL);
        Assert.assertNotNull(
                "Monitor did not get an activity before hitting the timeout", lastActivity);
        Assert.assertTrue(
                "Expected lastActivity to be a ChromeActivity, was "
                        + lastActivity.getClass().getName(),
                lastActivity instanceof ChromeActivity);
        final ChromeActivity newActivity = (ChromeActivity) lastActivity;
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(newActivity.getActivityTab(), Matchers.notNullValue());
                    Criteria.checkThat(newActivity.getActivityTab(), is(tabToBeReparented));
                });
        assertEquals(newActivity.getWindowAndroid(), tabToBeReparented.getWindowAndroid());
        assertEquals(
                newActivity.getWindowAndroid(),
                tabToBeReparented.getWebContents().getTopLevelNativeWindow());
        Assert.assertFalse(
                TabTestUtils.getDelegateFactory(tabToBeReparented)
                        instanceof CustomTabDelegateFactory);
        assertEquals(
                "The tab should never be hidden during the reparenting process",
                0,
                tabHiddenHelper.getCallCount());
        Assert.assertFalse(TabTestUtils.isCustomTab(tabToBeReparented));
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    tabToBeReparented.removeObserver(observer);
                    ObserverList.RewindableIterator<TabObserver> observers =
                            TabTestUtils.getTabObservers(tabToBeReparented);
                    while (observers.hasNext()) {
                        Assert.assertFalse(observers.next() instanceof CustomTabObserver);
                    }
                });
        return newActivity;
    }

    /** Test whether a custom tab can be reparented to a new activity. */
    @Test
    @SmallTest
    @DisabledTest(message = "crbug.com/1434800")
    public void testTabReparentingBasic() {
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(createMinimalCustomTabIntent());
        assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        LaunchCauseMetrics.LAUNCH_CAUSE_HISTOGRAM,
                        LaunchCauseMetrics.LaunchCause.CUSTOM_TAB));
        reparentAndVerifyTab();
        assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        LaunchCauseMetrics.LAUNCH_CAUSE_HISTOGRAM,
                        LaunchCauseMetrics.LaunchCause.OPEN_IN_BROWSER_FROM_MENU));
    }

    /**
     * Test whether a custom tab can be reparented to a new activity and the select element is still
     * interactable after reparenting.
     */
    @SmallTest
    @Test
    public void testTabReparentingSelectPopup() throws TimeoutException {
        LocationSettingsTestUtil.setSystemLocationSettingEnabled(true);
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(
                CustomTabsIntentTestUtils.createMinimalCustomTabIntent(
                        ApplicationProvider.getApplicationContext(),
                        mTestServer.getURL(SELECT_POPUP_PAGE)));
        CriteriaHelper.pollUiThread(
                () -> {
                    Tab currentTab = mCustomTabActivityTestRule.getActivity().getActivityTab();
                    Criteria.checkThat(currentTab, Matchers.notNullValue());
                    Criteria.checkThat(currentTab.getWebContents(), Matchers.notNullValue());
                });

        DOMUtils.clickNode(mCustomTabActivityTestRule.getWebContents(), "select");
        CriteriaHelper.pollUiThread(
                () -> isSelectPopupVisible(mCustomTabActivityTestRule.getActivity()));

        final ChromeActivity newActivity = reparentAndVerifyTab();
        DOMUtils.clickNode(newActivity.getActivityTab().getWebContents(), "select");
        CriteriaHelper.pollUiThread(() -> isSelectPopupVisible(newActivity));
    }

    private static boolean isSelectPopupVisible(ChromeActivity activity) {
        Tab tab = activity.getActivityTab();
        if (tab == null || tab.getWebContents() == null) return false;
        return WebContentsUtils.isSelectPopupVisible(tab.getWebContents());
    }
}

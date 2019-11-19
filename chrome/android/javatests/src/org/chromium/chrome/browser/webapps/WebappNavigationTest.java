// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import static org.chromium.base.ApplicationState.HAS_DESTROYED_ACTIVITIES;
import static org.chromium.base.ApplicationState.HAS_PAUSED_ACTIVITIES;
import static org.chromium.base.ApplicationState.HAS_STOPPED_ACTIVITIES;

import android.content.Intent;
import android.graphics.Color;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.LargeTest;
import android.support.test.filters.SmallTest;
import android.util.Base64;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.CommandLine;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.ShortcutHelper;
import org.chromium.chrome.browser.customtabs.CustomTabActivity;
import org.chromium.chrome.browser.externalnav.ExternalNavigationHandler.OverrideUrlLoadingResult;
import org.chromium.chrome.browser.firstrun.FirstRunStatus;
import org.chromium.chrome.browser.tab.InterceptNavigationDelegateImpl;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.styles.ChromeColors;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.MenuUtils;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.chrome.test.util.browser.contextmenu.ContextMenuUtils;
import org.chromium.chrome.test.util.browser.contextmenu.RevampedContextMenuUtils;
import org.chromium.chrome.test.util.browser.webapps.WebappTestPage;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.test.NativeLibraryTestRule;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.DOMUtils;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.content_public.common.ContentSwitches;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.ui.base.PageTransition;

/**
 * Tests web navigations originating from a WebappActivity.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
        ContentSwitches.HOST_RESOLVER_RULES + "=MAP * 127.0.0.1"})
public class WebappNavigationTest {
    private static final String YOUTUBE_URL = "https://www.youtube.com/watch?v=EYmjoW4vIX8";

    @Rule
    public final WebappActivityTestRule mActivityTestRule = new WebappActivityTestRule();

    @Rule
    public final NativeLibraryTestRule mNativeLibraryTestRule = new NativeLibraryTestRule();

    @Before
    public void setUp() {
        mNativeLibraryTestRule.loadNativeLibraryNoBrowserProcess();
    }

    /**
     * Test that navigating a webapp whose launch intent does not specify a theme colour outside of
     * the webapp scope by tapping a regular link:
     * - Shows a CCT-like webapp toolbar.
     * - Uses the default theme colour as the toolbar colour.
     */
    @Test
    @SmallTest
    @Feature({"Webapps"})
    @RetryOnFailure
    public void testRegularLinkOffOriginNoWebappThemeColor() throws Exception {
        WebappActivity activity = runWebappActivityAndWaitForIdle(mActivityTestRule.createIntent());
        WebappActivityTestRule.assertToolbarShowState(activity, false);

        addAnchorAndClick(offOriginUrl(), "_self");

        ChromeTabUtils.waitForTabPageLoaded(activity.getActivityTab(), offOriginUrl());
        WebappActivityTestRule.assertToolbarShowState(activity, true);
        Assert.assertEquals(
                getDefaultPrimaryColor(), activity.getToolbarManager().getPrimaryColor());
    }

    /**
     * Test that navigating a webapp whose launch intent specifies a theme colour outside of the
     * webapp scope by tapping a regular link:
     * - Shows a CCT-like webapp toolbar.
     * - Uses the webapp theme colour as the toolbar colour.
     */
    @Test
    @SmallTest
    @Feature({"Webapps"})
    @RetryOnFailure
    public void testRegularLinkOffOriginThemeColor() throws Exception {
        WebappActivity activity =
                runWebappActivityAndWaitForIdle(mActivityTestRule.createIntent().putExtra(
                        ShortcutHelper.EXTRA_THEME_COLOR, (long) Color.CYAN));
        WebappActivityTestRule.assertToolbarShowState(activity, false);

        addAnchorAndClick(offOriginUrl(), "_self");

        ChromeTabUtils.waitForTabPageLoaded(activity.getActivityTab(), offOriginUrl());
        WebappActivityTestRule.assertToolbarShowState(activity, true);
        Assert.assertEquals(Color.CYAN, activity.getToolbarManager().getPrimaryColor());
    }

    /**
     * Test that navigating a TWA outside of the TWA scope by tapping a regular link:
     * - Expects the Minimal UI toolbar to be shown.
     * - Uses the TWA theme colour in the Minimal UI toolbar.
     */
    @Test
    @SmallTest
    @Feature({"Webapps"})
    public void testRegularLinkOffOriginTwa() throws Exception {
        Intent launchIntent = mActivityTestRule.createIntent().putExtra(
                ShortcutHelper.EXTRA_THEME_COLOR, (long) Color.CYAN);
        mActivityTestRule.addTwaExtrasToIntent(launchIntent);
        String url = WebappTestPage.getServiceWorkerUrl(mActivityTestRule.getTestServer());
        CommandLine.getInstance().appendSwitchWithValue(
                ChromeSwitches.DISABLE_DIGITAL_ASSET_LINK_VERIFICATION, url);
        mActivityTestRule.startWebappActivity(launchIntent.putExtra(ShortcutHelper.EXTRA_URL, url));
        mActivityTestRule.waitUntilSplashscreenHides();
        mActivityTestRule.waitUntilIdle();
        WebappActivity activity = mActivityTestRule.getActivity();
        WebappActivityTestRule.assertToolbarShowState(activity, false);
        addAnchorAndClick(offOriginUrl(), "_self");
        ChromeTabUtils.waitForTabPageLoaded(activity.getActivityTab(), offOriginUrl());
        WebappActivityTestRule.assertToolbarShowState(activity, true);
        Assert.assertEquals(Color.CYAN, activity.getToolbarManager().getPrimaryColor());
    }

    /**
     * Test that navigating outside of the webapp scope as a result of submitting a form with method
     * "POST":
     * - Shows a CCT-like webapp toolbar.
     * - Preserves the theme color specified in the launch intent.
     */
    @Test
    @SmallTest
    @Feature({"Webapps"})
    @RetryOnFailure
    public void testFormSubmitOffOrigin() throws Exception {
        Intent launchIntent = mActivityTestRule.createIntent().putExtra(
                ShortcutHelper.EXTRA_THEME_COLOR, (long) Color.CYAN);
        mActivityTestRule.addTwaExtrasToIntent(launchIntent);
        WebappActivity activity = runWebappActivityAndWaitForIdleWithUrl(launchIntent,
                mActivityTestRule.getTestServer().getURL("/chrome/test/data/android/form.html"));

        mActivityTestRule.runJavaScriptCodeInCurrentTab(String.format(
                "document.getElementById('form').setAttribute('action', '%s')", offOriginUrl()));
        clickNodeWithId("post_button");

        ChromeTabUtils.waitForTabPageLoaded(activity.getActivityTab(), offOriginUrl());
        Assert.assertEquals(Color.CYAN, activity.getToolbarManager().getPrimaryColor());
    }

    /**
     * Test that navigating outside of the webapp scope by tapping a link with target="_blank":
     * - Launches a CCT.
     * - The Minimal UI toolbar does not use the webapp theme colour.
     */
    @Test
    @SmallTest
    @Feature({"Webapps"})
    @RetryOnFailure
    public void testOffScopeNewTabLinkOpensInCct() throws Exception {
        runWebappActivityAndWaitForIdle(mActivityTestRule.createIntent().putExtra(
                ShortcutHelper.EXTRA_THEME_COLOR, (long) Color.CYAN));
        addAnchorAndClick(offOriginUrl(), "_blank");
        CustomTabActivity customTab = ChromeActivityTestRule.waitFor(CustomTabActivity.class);
        ChromeTabUtils.waitForTabPageLoaded(customTab.getActivityTab(), offOriginUrl());

        Assert.assertEquals(
                getDefaultPrimaryColor(), customTab.getToolbarManager().getPrimaryColor());
    }

    /**
     * Test that navigating within the webapp scope by tapping a link with target="_blank" launches
     * a CCT.
     */
    @Test
    @SmallTest
    @Feature({"Webapps"})
    @RetryOnFailure
    public void testInScopeNewTabLinkOpensInCct() throws Exception {
        String inScopeUrl =
                WebappTestPage.getNonServiceWorkerUrl(mActivityTestRule.getTestServer());
        runWebappActivityAndWaitForIdle(mActivityTestRule.createIntent().putExtra(
                ShortcutHelper.EXTRA_THEME_COLOR, (long) Color.CYAN));
        addAnchorAndClick(inScopeUrl, "_blank");
        CustomTabActivity customTab = ChromeActivityTestRule.waitFor(CustomTabActivity.class);
        ChromeTabUtils.waitForTabPageLoaded(customTab.getActivityTab(), inScopeUrl);
        Assert.assertTrue(
                mActivityTestRule.runJavaScriptCodeInCurrentTab("document.body.textContent")
                        .contains("Do-nothing page with a manifest and a service worker."));
    }

    /**
     * Test that navigating outside of the webapp via window.open():
     * - Launches a CCT.
     * - The CCT toolbar does not use the webapp theme colour.
     */
    @Test
    @SmallTest
    @Feature({"Webapps"})
    @RetryOnFailure
    public void testWindowOpenInCct() throws Exception {
        runWebappActivityAndWaitForIdle(mActivityTestRule.createIntent().putExtra(
                ShortcutHelper.EXTRA_THEME_COLOR, (long) Color.CYAN));

        WebappActivityTestRule.jsWindowOpen(mActivityTestRule.getActivity(), offOriginUrl());
        CustomTabActivity customTab = ChromeActivityTestRule.waitFor(CustomTabActivity.class);
        ChromeTabUtils.waitForTabPageLoaded(customTab.getActivityTab(), offOriginUrl());
        Assert.assertEquals(
                getDefaultPrimaryColor(), customTab.getToolbarManager().getPrimaryColor());
    }

    /**
     * Test that navigating a webapp within the webapp scope by tapping a regular link:
     * - Does not show a CCT-like webapp toolbar.
     * - Does not launch a CCT.
     */
    @Test
    @SmallTest
    @Feature({"Webapps"})
    @RetryOnFailure
    public void testInScopeNavigationStaysInWebapp() throws Exception {
        WebappActivity activity = runWebappActivityAndWaitForIdle(mActivityTestRule.createIntent());
        String otherPageUrl =
                WebappTestPage.getNonServiceWorkerUrl(mActivityTestRule.getTestServer());
        addAnchorAndClick(otherPageUrl, "_self");
        ChromeTabUtils.waitForTabPageLoaded(activity.getActivityTab(), otherPageUrl);

        WebappActivityTestRule.assertToolbarShowState(activity, false);
    }

    @Test
    @SmallTest
    @Feature({"Webapps"})
    @DisableFeatures({ChromeFeatureList.REVAMPED_CONTEXT_MENU})
    @RetryOnFailure
    public void testOpenInChromeFromContextMenuTabbedChrome() throws Exception {
        // Needed to get full context menu.
        FirstRunStatus.setFirstRunFlowComplete(true);
        runWebappActivityAndWaitForIdle(mActivityTestRule.createIntent());

        addAnchor("myTestAnchorId", offOriginUrl(), "_self");

        ContextMenuUtils.selectContextMenuItem(InstrumentationRegistry.getInstrumentation(),
                null /* activity to check for focus after click */,
                mActivityTestRule.getActivity().getActivityTab(), "myTestAnchorId",
                R.id.contextmenu_open_in_chrome);

        ChromeTabbedActivity tabbedChrome =
                ChromeActivityTestRule.waitFor(ChromeTabbedActivity.class);
        ChromeTabUtils.waitForTabPageLoaded(tabbedChrome.getActivityTab(), offOriginUrl());
    }

    @Test
    @SmallTest
    @Feature({"Webapps"})
    @EnableFeatures({ChromeFeatureList.REVAMPED_CONTEXT_MENU})
    public void testOpenInChromeFromRevampedContextMenuTabbedChrome() throws Exception {
        // Needed to get full context menu.
        FirstRunStatus.setFirstRunFlowComplete(true);
        runWebappActivityAndWaitForIdle(mActivityTestRule.createIntent());

        addAnchor("myTestAnchorId", offOriginUrl(), "_self");

        RevampedContextMenuUtils.selectContextMenuItem(InstrumentationRegistry.getInstrumentation(),
                null /* activity to check for focus after click */,
                mActivityTestRule.getActivity().getActivityTab(), "myTestAnchorId",
                R.id.contextmenu_open_in_chrome);

        ChromeTabbedActivity tabbedChrome =
                ChromeActivityTestRule.waitFor(ChromeTabbedActivity.class);
        ChromeTabUtils.waitForTabPageLoaded(tabbedChrome.getActivityTab(), offOriginUrl());
    }

    @Test
    @SmallTest
    @Feature({"Webapps"})
    @RetryOnFailure
    public void testOpenInChromeFromCustomMenuTabbedChrome() {
        WebappActivity activity =
                runWebappActivityAndWaitForIdle(mActivityTestRule.createIntent().putExtra(
                        ShortcutHelper.EXTRA_DISPLAY_MODE, WebDisplayMode.MINIMAL_UI));

        MenuUtils.invokeCustomMenuActionSync(
                InstrumentationRegistry.getInstrumentation(), activity, R.id.open_in_browser_id);

        ChromeTabbedActivity tabbedChrome =
                ChromeActivityTestRule.waitFor(ChromeTabbedActivity.class);
        ChromeTabUtils.waitForTabPageLoaded(tabbedChrome.getActivityTab(),
                WebappTestPage.getServiceWorkerUrl(mActivityTestRule.getTestServer()));
    }

    @Test
    @SmallTest
    @Feature({"Webapps"})
    @RetryOnFailure
    public void testRegularLinkToExternalApp() throws Exception {
        runWebappActivityAndWaitForIdle(mActivityTestRule.createIntent());

        InterceptNavigationDelegateImpl navigationDelegate = TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> InterceptNavigationDelegateImpl.get(
                                mActivityTestRule.getActivity().getActivityTab()));

        addAnchorAndClick(YOUTUBE_URL, "_self");

        waitForExternalAppOrIntentPicker();
        Assert.assertEquals(OverrideUrlLoadingResult.OVERRIDE_WITH_EXTERNAL_INTENT,
                navigationDelegate.getLastOverrideUrlLoadingResultForTests());
    }

    @Test
    @SmallTest
    @Feature({"Webapps"})
    @RetryOnFailure
    public void testNewTabLinkToExternalApp() throws Exception {
        runWebappActivityAndWaitForIdle(mActivityTestRule.createIntent());

        addAnchorAndClick(YOUTUBE_URL, "_blank");

        waitForExternalAppOrIntentPicker();
    }

    @Test
    @SmallTest
    @Feature({"Webapps"})
    @RetryOnFailure
    // Regression test for crbug.com/771174.
    public void testCanNavigateAfterReparentingToTabbedChrome() throws Exception {
        runWebappActivityAndWaitForIdle(
                mActivityTestRule.createIntent()
                        .putExtra(ShortcutHelper.EXTRA_DISPLAY_MODE, WebDisplayMode.MINIMAL_UI)
                        .putExtra(ShortcutHelper.EXTRA_THEME_COLOR, (long) Color.CYAN));

        MenuUtils.invokeCustomMenuActionSync(InstrumentationRegistry.getInstrumentation(),
                mActivityTestRule.getActivity(), R.id.open_in_browser_id);

        ChromeTabbedActivity tabbedChrome =
                ChromeActivityTestRule.waitFor(ChromeTabbedActivity.class);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> tabbedChrome.getActivityTab().loadUrl(new LoadUrlParams(offOriginUrl())));
        ChromeTabUtils.waitForTabPageLoaded(tabbedChrome.getActivityTab(), offOriginUrl());
    }

    @Test
    @SmallTest
    @Feature({"Webapps"})
    @RetryOnFailure
    public void testCloseButtonReturnsToMostRecentInScopeUrl() throws Exception {
        WebappActivity activity = runWebappActivityAndWaitForIdle(mActivityTestRule.createIntent());
        Tab tab = activity.getActivityTab();

        String otherInScopeUrl =
                WebappTestPage.getNonServiceWorkerUrl(mActivityTestRule.getTestServer());
        mActivityTestRule.loadUrlInTab(otherInScopeUrl, PageTransition.LINK, tab);
        Assert.assertEquals(otherInScopeUrl, tab.getUrl());

        mActivityTestRule.loadUrlInTab(offOriginUrl(), PageTransition.LINK, tab);
        String mozillaUrl = mActivityTestRule.getTestServer().getURLWithHostName(
                "mozilla.org", "/defaultresponse");
        mActivityTestRule.loadUrlInTab(mozillaUrl, PageTransition.LINK, tab);

        // Toolbar with the close button should be visible.
        WebappActivityTestRule.assertToolbarShowState(activity, true);

        // Navigate back to in-scope through a close button.
        TestThreadUtils.runOnUiThreadBlocking(()
                                                      -> activity.getToolbarManager()
                                                                 .getToolbarLayoutForTesting()
                                                                 .findViewById(R.id.close_button)
                                                                 .callOnClick());

        // We should end up on most recent in-scope URL.
        ChromeTabUtils.waitForTabPageLoaded(tab, otherInScopeUrl);
    }

    /**
     * When a Minimal UI is shown as a result of a redirect chain, closing the Minimal UI should
     * return the user to the navigation entry prior to the redirect chain.
     */
    @Test
    @LargeTest
    @Feature({"Webapps"})
    public void testCloseButtonReturnsToUrlBeforeRedirects() throws Exception {
        Intent launchIntent = mActivityTestRule.createIntent();
        mActivityTestRule.addTwaExtrasToIntent(launchIntent);
        WebappActivity activity = runWebappActivityAndWaitForIdle(launchIntent);

        EmbeddedTestServer testServer = mActivityTestRule.getTestServer();
        String initialInScopeUrl = WebappTestPage.getServiceWorkerUrl(testServer);
        ChromeTabUtils.waitForTabPageLoaded(activity.getActivityTab(), initialInScopeUrl);

        final String redirectingUrl =
                testServer.getURL("/chrome/test/data/android/redirect/js_redirect.html"
                        + "?replace_text="
                        + Base64.encodeToString(
                                  ApiCompatibilityUtils.getBytesUtf8("PARAM_URL"), Base64.URL_SAFE)
                        + ":"
                        + Base64.encodeToString(ApiCompatibilityUtils.getBytesUtf8(offOriginUrl()),
                                  Base64.URL_SAFE));
        addAnchorAndClick(redirectingUrl, "_self");

        ChromeTabUtils.waitForTabPageLoaded(activity.getActivityTab(), offOriginUrl());

        // Close the Minimal UI.
        WebappActivityTestRule.assertToolbarShowState(activity, true);
        TestThreadUtils.runOnUiThreadBlocking(()
                                                      -> activity.getToolbarManager()
                                                                 .getToolbarLayoutForTesting()
                                                                 .findViewById(R.id.close_button)
                                                                 .callOnClick());

        // The WebappActivity should be navigated to the page prior to the redirect.
        ChromeTabUtils.waitForTabPageLoaded(activity.getActivityTab(), initialInScopeUrl);
    }

    private WebappActivity runWebappActivityAndWaitForIdle(Intent intent) {
        return runWebappActivityAndWaitForIdleWithUrl(
                intent, WebappTestPage.getServiceWorkerUrl(mActivityTestRule.getTestServer()));
    }

    private WebappActivity runWebappActivityAndWaitForIdleWithUrl(Intent intent, String url) {
        mActivityTestRule.startWebappActivity(intent.putExtra(ShortcutHelper.EXTRA_URL, url));
        mActivityTestRule.waitUntilSplashscreenHides();
        mActivityTestRule.waitUntilIdle();
        return mActivityTestRule.getActivity();
    }

    private long getDefaultPrimaryColor() {
        return ChromeColors.getDefaultThemeColor(
                mActivityTestRule.getActivity().getResources(), false);
    }

    private String offOriginUrl() {
        return mActivityTestRule.getTestServer().getURLWithHostName("foo.com", "/defaultresponse");
    }

    private void addAnchor(String id, String url, String target) throws Exception {
        mActivityTestRule.runJavaScriptCodeInCurrentTab(
                String.format("var aTag = document.createElement('a');"
                                + "aTag.id = '%s';"
                                + "aTag.setAttribute('href','%s');"
                                + "aTag.setAttribute('target','%s');"
                                + "aTag.innerHTML = 'Click Me!';"
                                + "document.body.appendChild(aTag);",
                        id, url, target));
    }

    private void clickNodeWithId(String id) throws Exception {
        DOMUtils.clickNode(mActivityTestRule.getActivity().getActivityTab().getWebContents(), id);
    }

    private void addAnchorAndClick(String url, String target) throws Exception {
        addAnchor("testId", url, target);
        clickNodeWithId("testId");
    }

    private void waitForExternalAppOrIntentPicker() {
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return ApplicationStatus.getStateForApplication() == HAS_PAUSED_ACTIVITIES
                        || ApplicationStatus.getStateForApplication() == HAS_STOPPED_ACTIVITIES
                        || ApplicationStatus.getStateForApplication() == HAS_DESTROYED_ACTIVITIES;
            }
        });
    }
}

// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import static org.junit.Assert.assertEquals;

import static org.chromium.base.ApplicationState.HAS_DESTROYED_ACTIVITIES;
import static org.chromium.base.ApplicationState.HAS_PAUSED_ACTIVITIES;
import static org.chromium.base.ApplicationState.HAS_STOPPED_ACTIVITIES;

import android.app.Instrumentation.ActivityMonitor;
import android.content.Intent;
import android.content.IntentFilter;
import android.graphics.Color;
import android.net.Uri;
import android.util.Base64;

import androidx.test.filters.LargeTest;
import androidx.test.filters.SmallTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.hamcrest.Matchers;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.RuleChain;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.CommandLine;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Restriction;
import org.chromium.blink.mojom.DisplayMode;
import org.chromium.cc.input.BrowserControlsState;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.browserservices.intents.WebappConstants;
import org.chromium.chrome.browser.crypto.CipherFactory;
import org.chromium.chrome.browser.customtabs.CustomTabNightModeStateController;
import org.chromium.chrome.browser.customtabs.DefaultBrowserProviderImpl;
import org.chromium.chrome.browser.customtabs.FakeDefaultBrowserProviderImpl;
import org.chromium.chrome.browser.customtabs.content.CustomTabIntentHandler;
import org.chromium.chrome.browser.customtabs.dependency_injection.BaseCustomTabActivityModule;
import org.chromium.chrome.browser.dependency_injection.ModuleOverridesRule;
import org.chromium.chrome.browser.firstrun.FirstRunStatus;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.test.MockCertVerifierRuleAndroid;
import org.chromium.chrome.browser.theme.TopUiThemeColorProvider;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.browser.contextmenu.ContextMenuUtils;
import org.chromium.chrome.test.util.browser.webapps.WebappTestPage;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.components.permissions.PermissionDialogController;
import org.chromium.content_public.browser.test.NativeLibraryTestUtils;
import org.chromium.content_public.browser.test.util.DOMUtils;
import org.chromium.content_public.common.ContentSwitches;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.PageTransition;
import org.chromium.ui.modaldialog.ModalDialogManager;

import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeoutException;

/** Tests web navigations originating from a WebappActivity. */
@RunWith(ChromeJUnit4ClassRunner.class)
@DoNotBatch(reason = "tests run on startup.")
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class WebappNavigationTest {
    public final WebappActivityTestRule mActivityTestRule = new WebappActivityTestRule();

    public MockCertVerifierRuleAndroid mCertVerifierRule =
            new MockCertVerifierRuleAndroid(0 /* net::OK */);

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
                    .around(mActivityTestRule)
                    .around(mCertVerifierRule)
                    .around(mModuleOverridesRule);

    @Before
    public void setUp() {
        NativeLibraryTestUtils.loadNativeLibraryNoBrowserProcess();

        mActivityTestRule.getEmbeddedTestServerRule().setServerUsesHttps(true);
        Uri mapToUri =
                Uri.parse(mActivityTestRule.getEmbeddedTestServerRule().getServer().getURL("/"));
        CommandLine.getInstance()
                .appendSwitchWithValue(
                        ContentSwitches.HOST_RESOLVER_RULES, "MAP * " + mapToUri.getAuthority());
    }

    /**
     * Test that navigating a webapp whose launch intent does not specify a theme colour outside of
     * the webapp scope by tapping a regular link: - Shows a CCT-like webapp toolbar. - Uses the
     * default theme colour as the toolbar colour.
     */
    @Test
    @SmallTest
    @Feature({"Webapps"})
    @Restriction(DeviceFormFactor.PHONE)
    public void testRegularLinkOffOriginNoWebappThemeColor() throws Exception {
        WebappActivity activity = runWebappActivityAndWaitForIdle(mActivityTestRule.createIntent());
        assertEquals(
                BrowserControlsState.HIDDEN, WebappActivityTestRule.getToolbarShowState(activity));

        addAnchorAndClick(offOriginUrl(), "_self");

        ChromeTabUtils.waitForTabPageLoaded(activity.getActivityTab(), offOriginUrl());
        WebappActivityTestRule.assertToolbarShownMaybeHideable(activity);
        assertEquals(getDefaultPrimaryColor(), activity.getToolbarManager().getPrimaryColor());
    }

    /**
     * Test that navigating a webapp whose launch intent specifies a theme colour outside of the
     * webapp scope by tapping a regular link: - Shows a CCT-like webapp toolbar. - Uses the webapp
     * theme colour as the toolbar colour.
     */
    @Test
    @SmallTest
    @Feature({"Webapps"})
    @Restriction(DeviceFormFactor.PHONE)
    public void testRegularLinkOffOriginThemeColor() throws Exception {
        WebappActivity activity =
                runWebappActivityAndWaitForIdle(
                        mActivityTestRule
                                .createIntent()
                                .putExtra(WebappConstants.EXTRA_THEME_COLOR, (long) Color.CYAN));
        assertEquals(
                BrowserControlsState.HIDDEN, WebappActivityTestRule.getToolbarShowState(activity));

        addAnchorAndClick(offOriginUrl(), "_self");

        ChromeTabUtils.waitForTabPageLoaded(activity.getActivityTab(), offOriginUrl());
        WebappActivityTestRule.assertToolbarShownMaybeHideable(activity);
        assertEquals(Color.CYAN, activity.getToolbarManager().getPrimaryColor());
    }

    /**
     * Test that navigating a TWA outside of the TWA scope by tapping a regular link: - Expects the
     * Minimal UI toolbar to be shown. - Uses the TWA theme colour in the Minimal UI toolbar.
     */
    @Test
    @SmallTest
    @Feature({"Webapps"})
    @Restriction(DeviceFormFactor.PHONE)
    public void testRegularLinkOffOriginTwa() throws Exception {
        Intent launchIntent =
                mActivityTestRule
                        .createIntent()
                        .putExtra(WebappConstants.EXTRA_THEME_COLOR, (long) Color.CYAN);
        mActivityTestRule.addTwaExtrasToIntent(launchIntent);
        String url = WebappTestPage.getServiceWorkerUrl(mActivityTestRule.getTestServer());
        CommandLine.getInstance()
                .appendSwitchWithValue(ChromeSwitches.DISABLE_DIGITAL_ASSET_LINK_VERIFICATION, url);
        mActivityTestRule.startWebappActivity(
                launchIntent.putExtra(WebappConstants.EXTRA_URL, url));
        WebappActivity activity = mActivityTestRule.getActivity();
        assertEquals(
                BrowserControlsState.HIDDEN, WebappActivityTestRule.getToolbarShowState(activity));
        addAnchorAndClick(offOriginUrl(), "_self");
        ChromeTabUtils.waitForTabPageLoaded(activity.getActivityTab(), offOriginUrl());
        WebappActivityTestRule.assertToolbarShownMaybeHideable(activity);
        assertEquals(Color.CYAN, activity.getToolbarManager().getPrimaryColor());
    }

    /**
     * Test that navigating outside of the webapp scope as a result of submitting a form with method
     * "POST": - Shows a CCT-like webapp toolbar. - Preserves the theme color specified in the
     * launch intent.
     */
    @Test
    @SmallTest
    @Feature({"Webapps"})
    @Restriction(DeviceFormFactor.PHONE)
    @DisabledTest(message = "Flaky - crbug.com/359629160")
    public void testFormSubmitOffOrigin() throws Exception {
        Intent launchIntent =
                mActivityTestRule
                        .createIntent()
                        .putExtra(WebappConstants.EXTRA_THEME_COLOR, (long) Color.CYAN);
        mActivityTestRule.addTwaExtrasToIntent(launchIntent);
        WebappActivity activity =
                runWebappActivityAndWaitForIdleWithUrl(
                        launchIntent,
                        mActivityTestRule
                                .getTestServer()
                                .getURL("/chrome/test/data/android/form.html"));

        mActivityTestRule.runJavaScriptCodeInCurrentTab(
                String.format(
                        "document.getElementById('form').setAttribute('action', '%s')",
                        offOriginUrl()));
        clickNodeWithId("post_button");

        ChromeTabUtils.waitForTabPageLoaded(activity.getActivityTab(), offOriginUrl());
        assertEquals(Color.CYAN, activity.getToolbarManager().getPrimaryColor());
    }

    /**
     * Test that navigating outside of the webapp scope by tapping a link with target="_blank": -
     * Opens a new tab. - Causes the toolbar to be shown.
     */
    @Test
    @SmallTest
    @Feature({"Webapps"})
    public void testOffScopeNewTabLinkShowsToolbar() throws Exception {
        runWebappActivityAndWaitForIdle(mActivityTestRule.createIntent());
        addAnchorAndClick(offOriginUrl(), "_blank");
        ChromeActivity activity = mActivityTestRule.getActivity();
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            activity.getTabModelSelector().getModel(false).getCount(),
                            Matchers.is(2));
                });
        ChromeTabUtils.waitForTabPageLoaded(activity.getActivityTab(), offOriginUrl());

        WebappActivityTestRule.assertToolbarShownMaybeHideable(activity);
    }

    /**
     * Test that navigating within the webapp scope by tapping a link with target="_blank": -
     * Launches a new tab. - Causes the toolbar to be shown.
     */
    @Test
    @SmallTest
    @Feature({"Webapps"})
    @DisabledTest(message = "Flaky, see crbug.com/352075550")
    public void testInScopeNewTabLinkShowsToolbar() throws Exception {
        String inScopeUrl =
                WebappTestPage.getNonServiceWorkerUrl(mActivityTestRule.getTestServer());
        runWebappActivityAndWaitForIdle(mActivityTestRule.createIntent());
        addAnchorAndClick(inScopeUrl, "_blank");
        ChromeActivity activity = mActivityTestRule.getActivity();
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            activity.getTabModelSelector().getModel(false).getCount(),
                            Matchers.is(2));
                });
        ChromeTabUtils.waitForTabPageLoaded(activity.getActivityTab(), inScopeUrl);

        WebappActivityTestRule.assertToolbarShownMaybeHideable(activity);
    }

    /**
     * Test that navigating a webapp within the webapp scope by tapping a regular link shows a
     * CCT-like webapp toolbar.
     */
    @Test
    @SmallTest
    @Feature({"Webapps"})
    @Restriction(DeviceFormFactor.PHONE)
    public void testInScopeNavigationStaysInWebapp() throws Exception {
        WebappActivity activity = runWebappActivityAndWaitForIdle(mActivityTestRule.createIntent());
        String otherPageUrl =
                WebappTestPage.getNonServiceWorkerUrl(mActivityTestRule.getTestServer());
        addAnchorAndClick(otherPageUrl, "_self");
        ChromeTabUtils.waitForTabPageLoaded(activity.getActivityTab(), otherPageUrl);

        assertEquals(
                BrowserControlsState.HIDDEN, WebappActivityTestRule.getToolbarShowState(activity));
    }

    @Test
    @SmallTest
    @Feature({"Webapps"})
    public void testOpenInChromeFromContextMenuTabbedChrome() throws Exception {
        // Needed to get full context menu.
        FirstRunStatus.setFirstRunFlowComplete(true);
        runWebappActivityAndWaitForIdle(mActivityTestRule.createIntent());

        addAnchor("myTestAnchorId", offOriginUrl(), "_self");

        IntentFilter filter = new IntentFilter(Intent.ACTION_VIEW);
        filter.addDataScheme("https");
        final ActivityMonitor monitor =
                InstrumentationRegistry.getInstrumentation().addMonitor(filter, null, true);

        ContextMenuUtils.selectContextMenuItem(
                InstrumentationRegistry.getInstrumentation(),
                null /* activity to check for focus after click */,
                mActivityTestRule.getActivity().getActivityTab(),
                "myTestAnchorId",
                R.id.contextmenu_open_in_chrome);

        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    return InstrumentationRegistry.getInstrumentation().checkMonitorHit(monitor, 1);
                });
    }

    @Test
    @SmallTest
    @Feature({"Webapps"})
    public void testOpenInChromeFromCustomMenuTabbedChrome() {
        WebappActivity activity =
                runWebappActivityAndWaitForIdle(
                        mActivityTestRule
                                .createIntent()
                                .putExtra(
                                        WebappConstants.EXTRA_DISPLAY_MODE,
                                        DisplayMode.MINIMAL_UI));

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    activity.getComponent().resolveNavigationController().openCurrentUrlInBrowser();
                });

        ChromeTabbedActivity tabbedChrome =
                ChromeActivityTestRule.waitFor(ChromeTabbedActivity.class);
        ChromeTabUtils.waitForTabPageLoaded(
                tabbedChrome.getActivityTab(),
                WebappTestPage.getServiceWorkerUrl(mActivityTestRule.getTestServer()));
    }

    @Test
    @LargeTest
    @Feature({"Webapps"})
    public void testCloseButtonReturnsToMostRecentInScopeUrl() throws Exception {
        WebappActivity activity = runWebappActivityAndWaitForIdle(mActivityTestRule.createIntent());
        Tab tab = activity.getActivityTab();

        String otherInScopeUrl =
                WebappTestPage.getNonServiceWorkerUrl(mActivityTestRule.getTestServer());
        mActivityTestRule.loadUrlInTab(otherInScopeUrl, PageTransition.LINK, tab);
        assertEquals(otherInScopeUrl, ChromeTabUtils.getUrlStringOnUiThread(tab));

        mActivityTestRule.loadUrlInTab(
                offOriginUrl(), PageTransition.LINK, tab, /* secondsToWait= */ 10);
        String mozillaUrl =
                mActivityTestRule
                        .getTestServer()
                        .getURLWithHostName("mozilla.org", "/defaultresponse");
        mActivityTestRule.loadUrlInTab(
                mozillaUrl, PageTransition.LINK, tab, /* secondsToWait= */ 10);

        // Toolbar with the close button should be visible.
        WebappActivityTestRule.assertToolbarShownMaybeHideable(activity);

        // Navigate back to in-scope through a close button.
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        activity.getToolbarManager()
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
                testServer.getURL(
                        "/chrome/test/data/android/redirect/js_redirect.html"
                                + "?replace_text="
                                + Base64.encodeToString(
                                        ApiCompatibilityUtils.getBytesUtf8("PARAM_URL"),
                                        Base64.URL_SAFE)
                                + ":"
                                + Base64.encodeToString(
                                        ApiCompatibilityUtils.getBytesUtf8(offOriginUrl()),
                                        Base64.URL_SAFE));
        addAnchorAndClick(redirectingUrl, "_self");

        ChromeTabUtils.waitForTabPageLoaded(activity.getActivityTab(), offOriginUrl());

        // Close the Minimal UI.
        WebappActivityTestRule.assertToolbarShownMaybeHideable(activity);
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        activity.getToolbarManager()
                                .getToolbarLayoutForTesting()
                                .findViewById(R.id.close_button)
                                .callOnClick());

        // The WebappActivity should be navigated to the page prior to the redirect.
        ChromeTabUtils.waitForTabPageLoaded(activity.getActivityTab(), initialInScopeUrl);
    }

    /** Test a permission dialog can be correctly presented and dismissed by navigation. */
    @Test
    @LargeTest
    @Feature({"Webapps"})
    public void testShowPermissionPrompt() throws TimeoutException, ExecutionException {
        Intent launchIntent = mActivityTestRule.createIntent();
        mActivityTestRule.addTwaExtrasToIntent(launchIntent);

        WebappActivity activity =
                runWebappActivityAndWaitForIdleWithUrl(
                        launchIntent,
                        mActivityTestRule
                                .getTestServer()
                                .getURL("/content/test/data/android/permission_navigation.html"));
        mActivityTestRule.runJavaScriptCodeInCurrentTab("requestGeolocationPermission()");
        CriteriaHelper.pollUiThread(
                () -> PermissionDialogController.getInstance().isDialogShownForTest(),
                "Permission prompt did not appear in allotted time");
        Assert.assertEquals(
                "Only App modal dialog is supported on web apk",
                activity.getModalDialogManager()
                        .getPresenterForTest(ModalDialogManager.ModalDialogType.APP),
                activity.getModalDialogManager().getCurrentPresenterForTest());
        // Launch a new page, which should be in CCT
        mActivityTestRule.runJavaScriptCodeInCurrentTab("navigate()");
        CriteriaHelper.pollUiThread(
                () -> !PermissionDialogController.getInstance().isDialogShownForTest(),
                "Permission prompt is not dismissed.");

        // Toolbar with the close button should be visible.
        WebappActivityTestRule.assertToolbarShownMaybeHideable(activity);

        // Navigate back to in-scope through a close button.
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        activity.getToolbarManager()
                                .getToolbarLayoutForTesting()
                                .findViewById(R.id.close_button)
                                .callOnClick());
        CriteriaHelper.pollUiThread(
                () -> !PermissionDialogController.getInstance().isDialogShownForTest(),
                "Permission prompt is not dismissed.");
    }

    private WebappActivity runWebappActivityAndWaitForIdle(Intent intent) {
        return runWebappActivityAndWaitForIdleWithUrl(
                intent, WebappTestPage.getServiceWorkerUrl(mActivityTestRule.getTestServer()));
    }

    private WebappActivity runWebappActivityAndWaitForIdleWithUrl(Intent intent, String url) {
        mActivityTestRule.startWebappActivity(intent.putExtra(WebappConstants.EXTRA_URL, url));
        return mActivityTestRule.getActivity();
    }

    private long getDefaultPrimaryColor() {
        return ChromeColors.getDefaultThemeColor(mActivityTestRule.getActivity(), false);
    }

    private String offOriginUrl() {
        return mActivityTestRule.getTestServer().getURLWithHostName("foo.com", "/defaultresponse");
    }

    private void addAnchor(String id, String url, String target) throws Exception {
        mActivityTestRule.runJavaScriptCodeInCurrentTab(
                String.format(
                        "var aTag = document.createElement('a');"
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
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            ApplicationStatus.getStateForApplication(),
                            Matchers.isOneOf(
                                    HAS_PAUSED_ACTIVITIES,
                                    HAS_STOPPED_ACTIVITIES,
                                    HAS_DESTROYED_ACTIVITIES));
                });
    }
}

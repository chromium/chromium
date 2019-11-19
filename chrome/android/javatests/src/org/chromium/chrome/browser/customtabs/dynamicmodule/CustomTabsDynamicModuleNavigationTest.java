// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.dynamicmodule;

import android.content.Intent;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.RuleChain;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;

import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.library_loader.LibraryProcessType;
import org.chromium.base.test.params.ParameterAnnotations.ClassParameter;
import org.chromium.base.test.params.ParameterAnnotations.UseRunnerDelegate;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.AppHooksModule;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.customtabs.CustomTabActivity;
import org.chromium.chrome.browser.customtabs.CustomTabActivityTestRule;
import org.chromium.chrome.browser.customtabs.dynamicmodule.CustomTabsDynamicModuleTestUtils.AppHooksModuleForTest;
import org.chromium.chrome.browser.customtabs.dynamicmodule.CustomTabsDynamicModuleTestUtils.FakeCCTActivityDelegate;
import org.chromium.chrome.browser.customtabs.dynamicmodule.CustomTabsDynamicModuleTestUtils.IntentBuilder;
import org.chromium.chrome.browser.dependency_injection.ModuleOverridesRule;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.content_public.browser.test.util.JavaScriptUtils;
import org.chromium.content_public.common.ContentSwitches;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.net.test.ServerCertificate;
import org.chromium.ui.base.PageTransition;

import java.util.Arrays;
import java.util.List;
import java.util.concurrent.TimeoutException;

import androidx.browser.customtabs.CustomTabsCallback;

/**
 * Instrumentation tests for the CustomTabsDynamicModuleNavigationObserver.
 */
@RunWith(ParameterizedRunner.class)
@UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
        ContentSwitches.HOST_RESOLVER_RULES + "=MAP * 127.0.0.1", "ignore-certificate-errors"})
public class CustomTabsDynamicModuleNavigationTest {

    private final TestRule mModuleOverridesRule = new ModuleOverridesRule()
            .setOverride(AppHooksModule.Factory.class, AppHooksModuleForTest::new);

    private final CustomTabActivityTestRule mActivityRule = new CustomTabActivityTestRule();

    @Rule
    public final TestRule mOverrideModulesThenLaunchRule =
            RuleChain.outerRule(mModuleOverridesRule).around(mActivityRule);

    private String mTestPage;
    private String mTestPage2;
    private String mTestPage3;
    private EmbeddedTestServer mTestServer;

    /**
     * Test against different module versions i.e. before and after API was introduced.
     */
    @ClassParameter
    private static List<ParameterSet> sModuleVersionsList = Arrays.asList(
            new ParameterSet().value(1).name("API_1"), new ParameterSet().value(4).name("API_4"));

    public CustomTabsDynamicModuleNavigationTest(int moduleVersion) {
        CustomTabsDynamicModuleTestUtils.setModuleVersion(moduleVersion);
    }

    @Before
    public void setUp() {
        LibraryLoader.getInstance().ensureInitialized(LibraryProcessType.PROCESS_BROWSER);

        // Module managed hosts only work with HTTPS.
        mTestServer = EmbeddedTestServer.createAndStartHTTPSServer(
                InstrumentationRegistry.getInstrumentation().getContext(),
                ServerCertificate.CERT_OK);

        mTestPage = mTestServer.getURLWithHostName(
                "google.com", "/chrome/test/data/android/google.html");
        mTestPage2 = mTestServer.getURLWithHostName(
                "google.com", "/chrome/test/data/android/simple.html");
        mTestPage3 = mTestServer.getURLWithHostName(
                "google.com", "/chrome/test/data/android/about.html");

        // The EmbeddedTestServer uses a non standard port.
        DynamicModuleCoordinator.setAllowNonStandardPortNumber(true);
    }

    @After
    public void tearDown() {
        mTestServer.stopAndDestroyServer();
        DynamicModuleCoordinator.setAllowNonStandardPortNumber(false);
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.CCT_MODULE)
    public void testModuleNavigationNotification() throws TimeoutException {
        Intent intent = new IntentBuilder(mTestPage).build();

        mActivityRule.startCustomTabActivityWithIntent(intent);

        mActivityRule.loadUrlInTab(mTestPage2, PageTransition.LINK,
                getActivity().getActivityTab());
        mActivityRule.loadUrlInTab(mTestPage3, PageTransition.LINK,
                getActivity().getActivityTab());

        FakeCCTActivityDelegate activityDelegate =
                (FakeCCTActivityDelegate) getModuleCoordinator().getActivityDelegateForTesting();

        activityDelegate.waitForNavigationEvent(CustomTabsCallback.NAVIGATION_STARTED,
                0, 3);
        activityDelegate.waitForNavigationEvent(CustomTabsCallback.NAVIGATION_FINISHED,
                0, 3);
        activityDelegate.waitForFirstContentfulPaint(0, 3);
    }

    private CustomTabActivity getActivity() {
        return mActivityRule.getActivity();
    }

    private DynamicModuleCoordinator getModuleCoordinator() {
        return getActivity().getComponent().resolveDynamicModuleCoordinator();
    }

    /**
     * Returns the text content of the document body.
     */
    private String getDocumentContent() throws Exception {
        Tab tab = getActivity().getActivityTab();
        return JavaScriptUtils.executeJavaScriptAndWaitForResult(
                tab.getWebContents(), "document.body.textContent");
    }

    private static final String HEADER_VALUE = "HEADER_VALUE";
    private static final String HEADER_VALUE_QUOTED = "\"" + HEADER_VALUE + "\"";
    private static final String NONE_QUOTED = "\"None\"";

    // The managed url regex matches the URL. The custom header is added.
    @Test
    @SmallTest
    @EnableFeatures(
            {ChromeFeatureList.CCT_MODULE, ChromeFeatureList.CCT_MODULE_CUSTOM_REQUEST_HEADER})
    public void
    testHeaderShown() throws Exception {
        String finalURL = mTestServer.getURLWithHostName(
                "google.com", "/echoheader?" + DynamicModuleConstants.MANAGED_URL_HEADER);
        Intent intent = new IntentBuilder(finalURL)
                                .setModuleManagedUrlRegex(".*")
                                .setModuleManagedUrlHeaderValue(HEADER_VALUE)
                                .build();
        mActivityRule.startCustomTabActivityWithIntent(intent);
        Assert.assertEquals(HEADER_VALUE_QUOTED, getDocumentContent());
    }

    // The managed url regex matches the URL, but CCT_MODULE_CUSTOM_REQUEST_HEADER is disabled. The
    // custom header is not added.
    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.CCT_MODULE)
    @DisableFeatures(ChromeFeatureList.CCT_MODULE_CUSTOM_REQUEST_HEADER)
    public void testHeaderFeatureDisabled() throws Exception {
        String finalURL = mTestServer.getURLWithHostName(
                "google.com", "/echoheader?" + DynamicModuleConstants.MANAGED_URL_HEADER);
        Intent intent = new IntentBuilder(finalURL)
                                .setModuleManagedUrlRegex(".*")
                                .setModuleManagedUrlHeaderValue(HEADER_VALUE)
                                .build();
        mActivityRule.startCustomTabActivityWithIntent(intent);
        Assert.assertEquals(NONE_QUOTED, getDocumentContent());
    }

    // The managed url regex doesn't match the URL. No custom header is added.
    @Test
    @SmallTest
    @EnableFeatures(
            {ChromeFeatureList.CCT_MODULE, ChromeFeatureList.CCT_MODULE_CUSTOM_REQUEST_HEADER})
    public void
    testHeaderNotShown() throws Exception {
        String finalURL = mTestServer.getURLWithHostName(
                "google.com", "/echoheader?" + DynamicModuleConstants.MANAGED_URL_HEADER);
        Intent intent = new IntentBuilder(finalURL)
                                .setModuleManagedUrlRegex("no-match")
                                .setModuleManagedUrlHeaderValue(HEADER_VALUE)
                                .build();
        mActivityRule.startCustomTabActivityWithIntent(intent);
        Assert.assertEquals(NONE_QUOTED, getDocumentContent());
    }

    // The managed url regex doesn't match the initial URL, but match the final
    // URL after a redirect. A custom header is added.
    @Test
    @SmallTest
    @EnableFeatures(
            {ChromeFeatureList.CCT_MODULE, ChromeFeatureList.CCT_MODULE_CUSTOM_REQUEST_HEADER})
    public void
    testHeaderMatchFinalURLOnly() throws Exception {
        String finalURL = mTestServer.getURLWithHostName(
                "google.com", "/echoheader?" + DynamicModuleConstants.MANAGED_URL_HEADER);
        String redirectURL = mTestServer.getURL("/server-redirect?" + finalURL);
        Intent intent = new IntentBuilder(redirectURL)
                                .setModuleManagedUrlRegex("^((?!redirect).)*$")
                                .setModuleManagedUrlHeaderValue(HEADER_VALUE)
                                .build();
        mActivityRule.startCustomTabActivityWithIntent(intent);

        Assert.assertEquals(HEADER_VALUE_QUOTED, getDocumentContent());
    }

    // The managed url regex matches the initial URL, but doesn't match the
    // final URL after a redirect. The custom header must have been removed from
    // the second request.
    @Test
    @SmallTest
    @EnableFeatures(
            {ChromeFeatureList.CCT_MODULE, ChromeFeatureList.CCT_MODULE_CUSTOM_REQUEST_HEADER})
    public void
    testHeaderMatchRedirectURLOnly() throws Exception {
        String finalURL = mTestServer.getURLWithHostName(
                "google.com", "/echoheader?" + DynamicModuleConstants.MANAGED_URL_HEADER);
        String redirectURL = mTestServer.getURL("/server-redirect?" + finalURL);
        Intent intent = new IntentBuilder(redirectURL)
                                .setModuleManagedUrlRegex(".*redirect.*")
                                .setModuleManagedUrlHeaderValue(HEADER_VALUE)
                                .build();
        mActivityRule.startCustomTabActivityWithIntent(intent);

        Assert.assertEquals(NONE_QUOTED, getDocumentContent());
    }

    // The managed url regex matches the initial URL and the final URL after a
    // redirect. The custom header is added to both requests.
    @Test
    @SmallTest
    @EnableFeatures(
            {ChromeFeatureList.CCT_MODULE, ChromeFeatureList.CCT_MODULE_CUSTOM_REQUEST_HEADER})
    public void
    testHeaderMatchBoth() throws Exception {
        String finalURL = mTestServer.getURLWithHostName(
                "google.com", "/echoheader?" + DynamicModuleConstants.MANAGED_URL_HEADER);
        String redirectURL = mTestServer.getURL("/server-redirect?" + finalURL);
        Intent intent = new IntentBuilder(redirectURL)
                                .setModuleManagedUrlRegex(".*")
                                .setModuleManagedUrlHeaderValue(HEADER_VALUE)
                                .build();
        mActivityRule.startCustomTabActivityWithIntent(intent);

        Assert.assertEquals(HEADER_VALUE_QUOTED, getDocumentContent());
    }
}

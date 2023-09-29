// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import static org.junit.Assert.assertEquals;

import static org.chromium.android_webview.test.OnlyRunIn.ProcessMode.MULTI_PROCESS;

import android.util.Pair;

import androidx.test.filters.LargeTest;
import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.android_webview.AwBrowserContext;
import org.chromium.android_webview.AwBrowserProcess;
import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.AwCookieManager;
import org.chromium.android_webview.WebMessageListener;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Feature;
import org.chromium.content_public.browser.test.util.RenderProcessHostUtils;
import org.chromium.net.test.util.TestWebServer;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

/**
 * Tests the management of multiple AwBrowserContexts (profiles)
 */
@RunWith(AwJUnit4ClassRunner.class)
@DoNotBatch(reason = "Tests focus on manipulation of global profile state")
public class MultiProfileTest {
    @Rule
    public AwActivityTestRule mActivityTestRule = new AwActivityTestRule();

    private final TestAwContentsClient mContentsClient = new TestAwContentsClient();

    private AwBrowserContext getContextSync(String name, boolean createIfNeeded) throws Throwable {
        return ThreadUtils.runOnUiThreadBlockingNoException(
                () -> { return AwBrowserContext.getNamedContext(name, createIfNeeded); });
    }

    private void setBrowserContextSync(AwContents awContents, AwBrowserContext browserContext)
            throws Throwable {
        ThreadUtils.runOnUiThreadBlockingNoException(() -> {
            awContents.setBrowserContext(browserContext);
            return null;
        });
    }

    @After
    public void tearDown() {
        mActivityTestRule.tearDown();
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testCreateProfiles() throws Throwable {
        final AwBrowserContext nonDefaultProfile1 = getContextSync("1", true);
        final AwBrowserContext alsoNonDefaultProfile1 = getContextSync("1", true);
        final AwBrowserContext defaultProfile = AwBrowserContext.getDefault();
        final AwBrowserContext defaultProfileByName = getContextSync("Default", true);
        final AwBrowserContext nonDefaultProfile2 = getContextSync("2", true);

        Assert.assertNotNull(nonDefaultProfile1);
        Assert.assertNotNull(nonDefaultProfile2);
        Assert.assertNotNull(defaultProfile);
        Assert.assertSame(nonDefaultProfile1, alsoNonDefaultProfile1);
        Assert.assertSame(defaultProfile, defaultProfileByName);
        Assert.assertNotSame(nonDefaultProfile1, nonDefaultProfile2);
        Assert.assertNotSame(defaultProfile, nonDefaultProfile1);
        Assert.assertNotSame(nonDefaultProfile2, defaultProfile);

        final List<String> names =
                ThreadUtils.runOnUiThreadBlockingNoException(AwBrowserContext::listAllContexts);
        Assert.assertTrue(names.contains("1"));
        Assert.assertTrue(names.contains("2"));
        Assert.assertTrue(names.contains("Default"));
        Assert.assertFalse(names.contains("3"));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testGetProfiles() throws Throwable {
        getContextSync("Exists", true);
        final AwBrowserContext existsProfile1 = getContextSync("Exists", false);
        final AwBrowserContext existsProfile2 = getContextSync("Exists", false);
        final AwBrowserContext defaultProfile = AwBrowserContext.getDefault();
        final AwBrowserContext defaultProfileByName = getContextSync("Default", false);
        final AwBrowserContext notExistsProfile = getContextSync("NotExists", false);

        Assert.assertNotNull(existsProfile1);
        Assert.assertNotNull(defaultProfile);
        Assert.assertNull(notExistsProfile);

        Assert.assertSame(existsProfile1, existsProfile2);
        Assert.assertSame(defaultProfile, defaultProfileByName);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testCannotDeleteDefault() throws Throwable {
        mActivityTestRule.runOnUiThread(() -> {
            Assert.assertThrows(IllegalArgumentException.class,
                    () -> { AwBrowserContext.deleteNamedContext("Default"); });
        });
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testCannotDeleteProfileInUse() throws Throwable {
        getContextSync("myProfile", true);
        mActivityTestRule.runOnUiThread(() -> {
            Assert.assertThrows(IllegalStateException.class,
                    () -> { AwBrowserContext.deleteNamedContext("myProfile"); });
        });
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testCanDeleteNonExistent() throws Throwable {
        mActivityTestRule.runOnUiThread(
                () -> { Assert.assertFalse(AwBrowserContext.deleteNamedContext("DoesNotExist")); });
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testGetName() throws Throwable {
        final AwBrowserContext defaultProfile = AwBrowserContext.getDefault();
        final AwBrowserContext profile1 = getContextSync("AwesomeProfile", true);
        assertEquals("Default", defaultProfile.getName());
        assertEquals("AwesomeProfile", profile1.getName());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testGetRelativePath() throws Throwable {
        final AwBrowserContext defaultProfile = AwBrowserContext.getDefault();
        final AwBrowserContext myCoolProfile = getContextSync("MyCoolProfile", true);
        final AwBrowserContext myOtherCoolProfile = getContextSync("MyOtherCoolProfile", true);
        assertEquals("Default", defaultProfile.getRelativePathForTesting());
        assertEquals("Profile 1", myCoolProfile.getRelativePathForTesting());
        assertEquals("Profile 2", myOtherCoolProfile.getRelativePathForTesting());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testSharedPrefsNamesAreCorrectAndUnique() throws Throwable {
        final String dataDirSuffix = "MyDataDirSuffix";

        AwBrowserProcess.setProcessDataDirSuffixForTesting(dataDirSuffix);
        final AwBrowserContext defaultProfile = AwBrowserContext.getDefault();
        final AwBrowserContext myCoolProfile = getContextSync("MyCoolProfile", true);
        final AwBrowserContext myOtherCoolProfile = getContextSync("MyOtherCoolProfile", true);
        final AwBrowserContext myCoolProfileCopy = getContextSync("MyCoolProfile", true);
        assertEquals("WebViewProfilePrefsDefault_MyDataDirSuffix",
                defaultProfile.getSharedPrefsNameForTesting());
        assertEquals("WebViewProfilePrefsProfile 1_MyDataDirSuffix",
                myCoolProfile.getSharedPrefsNameForTesting());
        assertEquals("WebViewProfilePrefsProfile 2_MyDataDirSuffix",
                myOtherCoolProfile.getSharedPrefsNameForTesting());
        assertEquals(myCoolProfile.getSharedPrefsNameForTesting(),
                myCoolProfileCopy.getSharedPrefsNameForTesting());

        AwBrowserProcess.setProcessDataDirSuffixForTesting(null);
        assertEquals("WebViewProfilePrefsDefault", defaultProfile.getSharedPrefsNameForTesting());
        assertEquals("WebViewProfilePrefsProfile 1", myCoolProfile.getSharedPrefsNameForTesting());
        assertEquals(
                "WebViewProfilePrefsProfile 2", myOtherCoolProfile.getSharedPrefsNameForTesting());
        assertEquals(myCoolProfile.getSharedPrefsNameForTesting(),
                myCoolProfileCopy.getSharedPrefsNameForTesting());
    }

    @Test
    @SmallTest
    @OnlyRunIn(MULTI_PROCESS)
    @Feature({"AndroidWebView"})
    public void testSetBrowserContextOnDestroyedWebViewThrowsException() throws Throwable {
        mActivityTestRule.startBrowserProcess();
        final AwBrowserContext otherProfile = getContextSync("other-profile", true);
        mActivityTestRule.runOnUiThread(() -> {
            AwContents awContents =
                    mActivityTestRule.createAwTestContainerView(mContentsClient).getAwContents();
            awContents.destroy();
            Assert.assertThrows("Cannot set new profile on a WebView that has been destroyed",
                    IllegalStateException.class,
                    () -> { awContents.setBrowserContext(otherProfile); });
        });
    }

    @Test
    @SmallTest
    @OnlyRunIn(MULTI_PROCESS)
    @Feature({"AndroidWebView"})
    public void testSetBrowserContextAfterGetBrowserContextThrowsException() throws Throwable {
        mActivityTestRule.startBrowserProcess();
        final AwBrowserContext otherProfile = getContextSync("other-profile", true);
        mActivityTestRule.runOnUiThread(() -> {
            AwContents awContents =
                    mActivityTestRule.createAwTestContainerView(mContentsClient).getAwContents();
            awContents.getBrowserContext();
            Assert.assertThrows(
                    "Cannot set new profile after the current one has been retrieved via. "
                            + "WebViewCompat#getProfile",
                    IllegalStateException.class,
                    () -> { awContents.setBrowserContext(otherProfile); });
        });
    }

    @Test
    @SmallTest
    @OnlyRunIn(MULTI_PROCESS)
    @Feature({"AndroidWebView"})
    public void testSetBrowserContextAfterPreviouslySetThrowsException() throws Throwable {
        mActivityTestRule.startBrowserProcess();
        final AwBrowserContext myCoolProfile = getContextSync("my-profile", true);
        final AwBrowserContext myOtherCoolProfile = getContextSync("my-other-profile", true);
        mActivityTestRule.runOnUiThread(() -> {
            AwContents awContents =
                    mActivityTestRule.createAwTestContainerView(mContentsClient).getAwContents();
            awContents.setBrowserContext(myCoolProfile);
            Assert.assertThrows("Cannot set new profile after one has already been set"
                            + "via. WebViewCompat#setProfile",
                    IllegalStateException.class,
                    () -> { awContents.setBrowserContext(myOtherCoolProfile); });
        });
    }

    @Test
    @SmallTest
    @OnlyRunIn(MULTI_PROCESS)
    @Feature({"AndroidWebView"})
    public void testSetBrowserContextAfterEvaluateJavascriptThrowsException() throws Throwable {
        mActivityTestRule.startBrowserProcess();
        AwContents awContents =
                mActivityTestRule.createAwTestContainerViewOnMainSync(mContentsClient)
                        .getAwContents();
        ThreadUtils.runOnUiThreadBlockingNoException(() -> {
            awContents.evaluateJavaScript("", null);
            return null;
        });
        final AwBrowserContext myCoolProfile = getContextSync("my-profile", true);
        mActivityTestRule.runOnUiThread(
                ()
                        -> Assert.assertThrows(
                                "Cannot set new profile after call to WebView#evaluateJavascript",
                                IllegalStateException.class,
                                () -> awContents.setBrowserContext(myCoolProfile)));
    }

    @Test
    @MediumTest
    @OnlyRunIn(MULTI_PROCESS)
    @Feature({"AndroidWebView"})
    public void testSetBrowserContextAfterWebViewNavigatedThrowsException() throws Throwable {
        mActivityTestRule.startBrowserProcess();
        final AwBrowserContext myCoolProfile = getContextSync("my-profile", true);
        AwContents awContents =
                mActivityTestRule.createAwTestContainerViewOnMainSync(mContentsClient)
                        .getAwContents();
        TestWebServer webServer = TestWebServer.start();
        String url = webServer.setResponse("/URL.html", "", null);
        mActivityTestRule.loadUrlSync(awContents, mContentsClient.getOnPageFinishedHelper(), url);
        mActivityTestRule.runOnUiThread(() -> {
            Assert.assertThrows(
                    "Cannot set new profile on a WebView that has been previously navigated.",
                    IllegalStateException.class,
                    () -> { awContents.setBrowserContext(myCoolProfile); });
            webServer.shutdown();
        });
    }

    @Test
    @SmallTest
    @OnlyRunIn(MULTI_PROCESS)
    @Feature({"AndroidWebView"})
    public void testSetBrowserContextSetsTheCorrectProfileOnAwContents() throws Throwable {
        mActivityTestRule.startBrowserProcess();
        final AwBrowserContext defaultProfile = AwBrowserContext.getDefault();
        final AwBrowserContext otherProfile = getContextSync("my-profile", true);
        final AwContents firstAwContents =
                mActivityTestRule.createAwTestContainerViewOnMainSync(mContentsClient)
                        .getAwContents();
        final AwContents secondAwContents =
                mActivityTestRule.createAwTestContainerViewOnMainSync(mContentsClient)
                        .getAwContents();
        setBrowserContextSync(secondAwContents, otherProfile);
        Assert.assertSame(defaultProfile, firstAwContents.getBrowserContext());
        Assert.assertSame(otherProfile, secondAwContents.getBrowserContext());
    }

    @Test
    @SmallTest
    @OnlyRunIn(MULTI_PROCESS)
    @Feature({"AndroidWebView"})
    public void testGetBrowserContextThrowsExceptionIfWebViewDestroyed() throws Throwable {
        mActivityTestRule.startBrowserProcess();
        final AwBrowserContext myProfile = getContextSync("my-profile", true);
        final AwContents awContents =
                mActivityTestRule.createAwTestContainerViewOnMainSync(mContentsClient)
                        .getAwContents();
        setBrowserContextSync(awContents, myProfile);
        awContents.destroy();
        Assert.assertThrows("Cannot get profile for destroyed WebView.",
                IllegalStateException.class, awContents::getBrowserContext);
    }

    @Test
    @LargeTest
    @OnlyRunIn(MULTI_PROCESS)
    @Feature({"AndroidWebView"})
    public void testWebViewsRunningDifferentProfilesUseCorrectCookieManagers() throws Throwable {
        mActivityTestRule.startBrowserProcess();
        final AwBrowserContext defaultProfile = AwBrowserContext.getDefault();
        final AwBrowserContext otherProfile = getContextSync("my-profile", true);
        final AwContents firstAwContents =
                mActivityTestRule.createAwTestContainerViewOnMainSync(mContentsClient)
                        .getAwContents();
        final AwContents secondAwContents =
                mActivityTestRule.createAwTestContainerViewOnMainSync(mContentsClient)
                        .getAwContents();
        setBrowserContextSync(secondAwContents, otherProfile);

        AwCookieManager defaultCookieManager = defaultProfile.getCookieManager();
        Assert.assertSame(
                defaultCookieManager, firstAwContents.getBrowserContext().getCookieManager());
        defaultCookieManager.setAcceptCookie(true);

        AwCookieManager otherCookieManager = otherProfile.getCookieManager();
        Assert.assertSame(
                otherCookieManager, secondAwContents.getBrowserContext().getCookieManager());
        otherCookieManager.setAcceptCookie(true);

        Assert.assertFalse(defaultCookieManager.hasCookies());
        Assert.assertFalse(otherCookieManager.hasCookies());

        TestWebServer webServer = TestWebServer.start();

        String[] cookies = {"httponly=foo1; HttpOnly", "strictsamesite=foo2; SameSite=Strict",
                "laxsamesite=foo3; SameSite=Lax"};
        List<Pair<String, String>> responseHeaders = new ArrayList<>();
        for (String cookie : cookies) {
            responseHeaders.add(Pair.create("Set-Cookie", cookie));
        }
        String path = "/cookie_test.html";
        String responseStr = "<html><head><title>TEST!</title></head><body>HELLO!</body></html>";
        String url = webServer.setResponse(path, responseStr, responseHeaders);
        mActivityTestRule.loadUrlSync(
                secondAwContents, mContentsClient.getOnPageFinishedHelper(), url);
        AwActivityTestRule.pollInstrumentationThread(
                () -> otherCookieManager.getCookie(url) != null);
        Assert.assertTrue(otherCookieManager.hasCookies());
        Assert.assertNotNull(otherCookieManager.getCookie(url));
        validateCookies(otherCookieManager, url, "httponly", "strictsamesite", "laxsamesite");
        otherCookieManager.removeAllCookies();

        // Check that the default cookie manager still does not have cookies.
        Assert.assertFalse(defaultCookieManager.hasCookies());
        webServer.shutdown();
    }

    @Test
    @MediumTest
    @OnlyRunIn(MULTI_PROCESS)
    @Feature({"AndroidWebView"})
    public void testSeparateProfilesHaveSeparateRenderProcesses() throws Throwable {
        mActivityTestRule.startBrowserProcess();
        final AwBrowserContext profile = getContextSync("my-profile", true);
        final AwContents firstAwContents =
                mActivityTestRule.createAwTestContainerViewOnMainSync(mContentsClient)
                        .getAwContents();
        final AwContents secondAwContents =
                mActivityTestRule.createAwTestContainerViewOnMainSync(mContentsClient)
                        .getAwContents();
        setBrowserContextSync(secondAwContents, profile);

        TestWebServer webServer = TestWebServer.start();
        String path = "/cookie_test.html";
        String responseStr = "<html><head><title>TEST!</title></head><body>HELLO!</body></html>";
        String url = webServer.setResponse(path, responseStr, new ArrayList<>());

        mActivityTestRule.loadUrlSync(
                firstAwContents, mContentsClient.getOnPageFinishedHelper(), url);
        assertEquals(1, RenderProcessHostUtils.getCurrentRenderProcessCount());

        mActivityTestRule.loadUrlSync(
                secondAwContents, mContentsClient.getOnPageFinishedHelper(), url);
        assertEquals(2, RenderProcessHostUtils.getCurrentRenderProcessCount());
        webServer.shutdown();
    }

    @Test
    @MediumTest
    @OnlyRunIn(MULTI_PROCESS)
    @Feature({"AndroidWebView"})
    public void testAwContentsWithSameProfileShareRenderProcess() throws Throwable {
        mActivityTestRule.startBrowserProcess();
        final AwBrowserContext profile = getContextSync("my-profile", true);
        final AwContents firstAwContents =
                mActivityTestRule.createAwTestContainerViewOnMainSync(mContentsClient)
                        .getAwContents();
        final AwContents secondAwContents =
                mActivityTestRule.createAwTestContainerViewOnMainSync(mContentsClient)
                        .getAwContents();
        setBrowserContextSync(firstAwContents, profile);
        setBrowserContextSync(secondAwContents, profile);

        TestWebServer webServer = TestWebServer.start();
        String path = "/cookie_test.html";
        String responseStr = "<html><head><title>TEST!</title></head><body>HELLO!</body></html>";
        String url = webServer.setResponse(path, responseStr, new ArrayList<>());

        mActivityTestRule.loadUrlSync(
                firstAwContents, mContentsClient.getOnPageFinishedHelper(), url);
        assertEquals(1, RenderProcessHostUtils.getCurrentRenderProcessCount());

        mActivityTestRule.loadUrlSync(
                secondAwContents, mContentsClient.getOnPageFinishedHelper(), url);
        assertEquals(1, RenderProcessHostUtils.getCurrentRenderProcessCount());
        webServer.shutdown();
    }

    private void validateCookies(
            AwCookieManager cookieManager, String url, String... expectedCookieNames) {
        final String responseCookie = cookieManager.getCookie(url);
        String[] cookies = responseCookie.split(";");
        // Convert to sets, since Set#equals() hooks in nicely with assertEquals()
        Set<String> foundCookieNamesSet = new HashSet<String>();
        for (String cookie : cookies) {
            foundCookieNamesSet.add(cookie.substring(0, cookie.indexOf("=")).trim());
        }
        Set<String> expectedCookieNamesSet =
                new HashSet<String>(Arrays.asList(expectedCookieNames));
        assertEquals("Found cookies list differs from expected list", expectedCookieNamesSet,
                foundCookieNamesSet);
    }

    @Test
    @LargeTest
    @OnlyRunIn(MULTI_PROCESS)
    @Feature({"AndroidWebView"})
    public void testInjectedJavascriptIsTransferredWhenProfileChanges() throws Throwable {
        mActivityTestRule.startBrowserProcess();

        String listenerName = "injectedListener";
        String startupScript = listenerName + ".postMessage('success');";
        String[] injectDomains = {"*"};

        final AwContents webView =
                mActivityTestRule.createAwTestContainerViewOnMainSync(mContentsClient)
                        .getAwContents();
        AwActivityTestRule.enableJavaScriptOnUiThread(webView);

        CallbackHelper testDoneHelper = new CallbackHelper();

        final WebMessageListener injectedListener =
                (payload, sourceOrigin, isMainFrame, jsReplyProxy, ports) -> {
            Assert.assertEquals("success", payload.getAsString());
            testDoneHelper.notifyCalled();
        };

        // Setup a message listener and a startup script to post on to the listener.
        mActivityTestRule.runOnUiThread(() -> {
            webView.addWebMessageListener(listenerName, injectDomains, injectedListener);
            webView.addDocumentStartJavaScript(startupScript, injectDomains);
        });

        // Switch the profile after the JS objects have been injected, but before content is loaded.
        AwBrowserContext otherProfile = getContextSync("other-profile", true);
        setBrowserContextSync(webView, otherProfile);

        // Load content using the new Context.
        try (TestWebServer server = TestWebServer.start()) {
            server.setResponse("/", "hello, world", new ArrayList<>());
            mActivityTestRule.loadUrlSync(
                    webView, mContentsClient.getOnPageFinishedHelper(), server.getBaseUrl());
            Assert.assertEquals("Injected listener was missing", "true",
                    mActivityTestRule.executeJavaScriptAndWaitForResult(
                            webView, mContentsClient, listenerName + " != null"));
        }

        // Wait for the test to run (see injectedListener above).
        testDoneHelper.waitForFirst(
                "Did not receive post message triggered by injected javascript");
    }
}

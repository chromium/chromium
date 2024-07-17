// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;

import static org.chromium.android_webview.test.OnlyRunIn.ProcessMode.MULTI_PROCESS;

import android.util.Pair;

import androidx.test.filters.LargeTest;
import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.Parameterized;
import org.junit.runners.Parameterized.UseParametersRunnerFactory;

import org.chromium.android_webview.AwBrowserContext;
import org.chromium.android_webview.AwBrowserContextStore;
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

/** Tests the management of multiple AwBrowserContexts (profiles) */
@RunWith(Parameterized.class)
@UseParametersRunnerFactory(AwJUnit4ClassRunnerWithParameters.Factory.class)
@DoNotBatch(reason = "Tests focus on manipulation of global profile state")
public class MultiProfileTest extends AwParameterizedTest {
    @Rule public MultiProfileTestRule mRule;

    private TestAwContentsClient mContentsClient;

    public MultiProfileTest(AwSettingsMutation param) {
        this.mRule = new MultiProfileTestRule(param.getMutation());
        this.mContentsClient = mRule.getContentsClient();
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testCreateProfiles() {
        final AwBrowserContext nonDefaultProfile1 = mRule.getProfileSync("1", true);
        final AwBrowserContext alsoNonDefaultProfile1 = mRule.getProfileSync("1", true);
        final AwBrowserContext defaultProfile = AwBrowserContext.getDefault();
        final AwBrowserContext defaultProfileByName = mRule.getProfileSync("Default", true);
        final AwBrowserContext nonDefaultProfile2 = mRule.getProfileSync("2", true);

        assertNotNull(nonDefaultProfile1);
        assertNotNull(nonDefaultProfile2);
        assertNotNull(defaultProfile);
        Assert.assertSame(nonDefaultProfile1, alsoNonDefaultProfile1);
        Assert.assertSame(defaultProfile, defaultProfileByName);
        Assert.assertNotSame(nonDefaultProfile1, nonDefaultProfile2);
        Assert.assertNotSame(defaultProfile, nonDefaultProfile1);
        Assert.assertNotSame(nonDefaultProfile2, defaultProfile);

        final List<String> names =
                ThreadUtils.runOnUiThreadBlocking(AwBrowserContextStore::listAllContexts);
        Assert.assertTrue(names.contains("1"));
        Assert.assertTrue(names.contains("2"));
        Assert.assertTrue(names.contains("Default"));
        Assert.assertFalse(names.contains("3"));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testGetProfiles() {
        mRule.getProfileSync("Exists", true);
        final AwBrowserContext existsProfile1 = mRule.getProfileSync("Exists", false);
        final AwBrowserContext existsProfile2 = mRule.getProfileSync("Exists", false);
        final AwBrowserContext defaultProfile = AwBrowserContext.getDefault();
        final AwBrowserContext defaultProfileByName = mRule.getProfileSync("Default", false);
        final AwBrowserContext notExistsProfile = mRule.getProfileSync("NotExists", false);

        assertNotNull(existsProfile1);
        assertNotNull(defaultProfile);
        Assert.assertNull(notExistsProfile);

        Assert.assertSame(existsProfile1, existsProfile2);
        Assert.assertSame(defaultProfile, defaultProfileByName);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testCannotDeleteDefault() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertThrows(
                            IllegalArgumentException.class,
                            () -> {
                                AwBrowserContextStore.deleteNamedContext("Default");
                            });
                });
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testCannotDeleteProfileInUse() {
        mRule.getProfileSync("myProfile", true);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertThrows(
                            IllegalStateException.class,
                            () -> {
                                AwBrowserContextStore.deleteNamedContext("myProfile");
                            });
                });
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testCanDeleteNonExistent() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertFalse(AwBrowserContextStore.deleteNamedContext("DoesNotExist"));
                });
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testGetName() {
        final AwBrowserContext defaultProfile = AwBrowserContext.getDefault();
        final AwBrowserContext profile1 = mRule.getProfileSync("AwesomeProfile", true);
        assertEquals("Default", defaultProfile.getName());
        assertEquals("AwesomeProfile", profile1.getName());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testGetRelativePath() {
        final AwBrowserContext defaultProfile = AwBrowserContext.getDefault();
        final AwBrowserContext myCoolProfile = mRule.getProfileSync("MyCoolProfile", true);
        final AwBrowserContext myOtherCoolProfile =
                mRule.getProfileSync("MyOtherCoolProfile", true);
        assertEquals("Default", defaultProfile.getRelativePathForTesting());
        assertEquals("Profile 1", myCoolProfile.getRelativePathForTesting());
        assertEquals("Profile 2", myOtherCoolProfile.getRelativePathForTesting());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testSharedPrefsNamesAreCorrectAndUnique() {
        final String dataDirSuffix = "MyDataDirSuffix";

        AwBrowserProcess.setProcessDataDirSuffixForTesting(dataDirSuffix);
        final AwBrowserContext defaultProfile = AwBrowserContext.getDefault();
        final AwBrowserContext myCoolProfile = mRule.getProfileSync("MyCoolProfile", true);
        final AwBrowserContext myOtherCoolProfile =
                mRule.getProfileSync("MyOtherCoolProfile", true);
        final AwBrowserContext myCoolProfileCopy = mRule.getProfileSync("MyCoolProfile", true);
        assertEquals(
                "WebViewProfilePrefsDefault_MyDataDirSuffix",
                defaultProfile.getSharedPrefsNameForTesting());
        assertEquals(
                "WebViewProfilePrefsProfile 1_MyDataDirSuffix",
                myCoolProfile.getSharedPrefsNameForTesting());
        assertEquals(
                "WebViewProfilePrefsProfile 2_MyDataDirSuffix",
                myOtherCoolProfile.getSharedPrefsNameForTesting());
        assertEquals(
                myCoolProfile.getSharedPrefsNameForTesting(),
                myCoolProfileCopy.getSharedPrefsNameForTesting());

        AwBrowserProcess.setProcessDataDirSuffixForTesting(null);
        assertEquals("WebViewProfilePrefsDefault", defaultProfile.getSharedPrefsNameForTesting());
        assertEquals("WebViewProfilePrefsProfile 1", myCoolProfile.getSharedPrefsNameForTesting());
        assertEquals(
                "WebViewProfilePrefsProfile 2", myOtherCoolProfile.getSharedPrefsNameForTesting());
        assertEquals(
                myCoolProfile.getSharedPrefsNameForTesting(),
                myCoolProfileCopy.getSharedPrefsNameForTesting());
    }

    @Test
    @SmallTest
    @OnlyRunIn(MULTI_PROCESS)
    @Feature({"AndroidWebView"})
    public void testSetBrowserContextOnDestroyedWebViewThrowsException() {
        mRule.startBrowserProcess();
        final AwBrowserContext otherProfile = mRule.getProfileSync("other-profile", true);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    AwContents awContents =
                            mRule.createAwTestContainerView(mContentsClient).getAwContents();
                    awContents.destroy();
                    Assert.assertThrows(
                            "Cannot set new profile on a WebView that has been destroyed",
                            IllegalStateException.class,
                            () -> {
                                awContents.setBrowserContext(otherProfile);
                            });
                });
    }

    @Test
    @SmallTest
    @OnlyRunIn(MULTI_PROCESS)
    @Feature({"AndroidWebView"})
    public void testSetBrowserContextAfterGetBrowserContextThrowsException() {
        mRule.startBrowserProcess();
        final AwBrowserContext otherProfile = mRule.getProfileSync("other-profile", true);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    AwContents awContents =
                            mRule.createAwTestContainerView(mContentsClient).getAwContents();
                    awContents.getBrowserContext();
                    Assert.assertThrows(
                            "Cannot set new profile after the current one has been retrieved via. "
                                    + "WebViewCompat#getProfile",
                            IllegalStateException.class,
                            () -> {
                                awContents.setBrowserContext(otherProfile);
                            });
                });
    }

    @Test
    @SmallTest
    @OnlyRunIn(MULTI_PROCESS)
    @Feature({"AndroidWebView"})
    public void testSetBrowserContextAfterPreviouslySetThrowsException() {
        mRule.startBrowserProcess();
        final AwBrowserContext myCoolProfile = mRule.getProfileSync("my-profile", true);
        final AwBrowserContext myOtherCoolProfile = mRule.getProfileSync("my-other-profile", true);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    AwContents awContents =
                            mRule.createAwTestContainerView(mContentsClient).getAwContents();
                    awContents.setBrowserContext(myCoolProfile);
                    Assert.assertThrows(
                            "Cannot set new profile after one has already been set"
                                    + "via. WebViewCompat#setProfile",
                            IllegalStateException.class,
                            () -> {
                                awContents.setBrowserContext(myOtherCoolProfile);
                            });
                });
    }

    @Test
    @SmallTest
    @OnlyRunIn(MULTI_PROCESS)
    @Feature({"AndroidWebView"})
    public void testSetBrowserContextAfterEvaluateJavascriptThrowsException() {
        mRule.startBrowserProcess();
        AwContents awContents = mRule.createAwContents();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    awContents.evaluateJavaScript("", null);
                    return null;
                });
        final AwBrowserContext myCoolProfile = mRule.getProfileSync("my-profile", true);
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        Assert.assertThrows(
                                "Cannot set new profile after call to"
                                        + " WebView#evaluateJavascript",
                                IllegalStateException.class,
                                () -> awContents.setBrowserContext(myCoolProfile)));
    }

    @Test
    @MediumTest
    @OnlyRunIn(MULTI_PROCESS)
    @Feature({"AndroidWebView"})
    public void testSetBrowserContextAfterWebViewNavigatedThrowsException() throws Throwable {
        mRule.startBrowserProcess();
        final AwBrowserContext myCoolProfile = mRule.getProfileSync("my-profile", true);
        AwContents awContents = mRule.createAwContents();
        TestWebServer webServer = TestWebServer.start();
        String url = webServer.setResponse("/URL.html", "", null);
        mRule.loadUrlSync(awContents, mContentsClient.getOnPageFinishedHelper(), url);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertThrows(
                            "Cannot set new profile on a WebView that has been previously"
                                    + " navigated.",
                            IllegalStateException.class,
                            () -> {
                                awContents.setBrowserContext(myCoolProfile);
                            });
                    webServer.shutdown();
                });
    }

    @Test
    @SmallTest
    @OnlyRunIn(MULTI_PROCESS)
    @Feature({"AndroidWebView"})
    public void testSetBrowserContextSetsTheCorrectProfileOnAwContents() {
        mRule.startBrowserProcess();
        final AwBrowserContext defaultProfile = AwBrowserContext.getDefault();
        final AwBrowserContext otherProfile = mRule.getProfileSync("my-profile", true);
        final AwContents firstAwContents = mRule.createAwContents();
        final AwContents secondAwContents = mRule.createAwContents(otherProfile);

        Assert.assertSame(defaultProfile, firstAwContents.getBrowserContext());
        Assert.assertSame(otherProfile, secondAwContents.getBrowserContext());
    }

    @Test
    @SmallTest
    @OnlyRunIn(MULTI_PROCESS)
    @Feature({"AndroidWebView"})
    public void testGetBrowserContextThrowsExceptionIfWebViewDestroyed() {
        mRule.startBrowserProcess();
        final AwBrowserContext myProfile = mRule.getProfileSync("my-profile", true);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    final AwContents awContents = mRule.createAwContents(myProfile);
                    awContents.destroy();
                    Assert.assertThrows(
                            "Cannot get profile for destroyed WebView.",
                            IllegalStateException.class,
                            awContents::getBrowserContext);
                });
    }

    @Test
    @LargeTest
    @OnlyRunIn(MULTI_PROCESS)
    @Feature({"AndroidWebView"})
    public void testWebViewsRunningDifferentProfilesUseCorrectCookieManagers() throws Throwable {
        mRule.startBrowserProcess();
        final AwBrowserContext defaultProfile = AwBrowserContext.getDefault();
        final AwBrowserContext otherProfile = mRule.getProfileSync("my-profile", true);
        final AwContents firstAwContents = mRule.createAwContents();
        final AwContents secondAwContents = mRule.createAwContents(otherProfile);

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

        String[] cookies = {
            "httponly=foo1; HttpOnly",
            "strictsamesite=foo2; SameSite=Strict",
            "laxsamesite=foo3; SameSite=Lax"
        };
        List<Pair<String, String>> responseHeaders = new ArrayList<>();
        for (String cookie : cookies) {
            responseHeaders.add(Pair.create("Set-Cookie", cookie));
        }
        String path = "/cookie_test.html";
        String responseStr = "<html><head><title>TEST!</title></head><body>HELLO!</body></html>";
        String url = webServer.setResponse(path, responseStr, responseHeaders);
        mRule.loadUrlSync(secondAwContents, mContentsClient.getOnPageFinishedHelper(), url);
        AwActivityTestRule.pollInstrumentationThread(
                () -> otherCookieManager.getCookie(url) != null);
        Assert.assertTrue(otherCookieManager.hasCookies());
        assertNotNull(otherCookieManager.getCookie(url));
        validateCookies(otherCookieManager, url, "httponly", "strictsamesite", "laxsamesite");
        otherCookieManager.removeAllCookies();

        // Check that the default cookie manager still does not have cookies.
        Assert.assertFalse(defaultCookieManager.hasCookies());
        webServer.shutdown();

        // Check that the cookie managers have different accept cookie settings.
        defaultCookieManager.setAcceptCookie(true);
        otherCookieManager.setAcceptCookie(false);

        Assert.assertTrue(defaultCookieManager.acceptCookie());
        Assert.assertFalse(otherCookieManager.acceptCookie());

        defaultCookieManager.setAcceptCookie(false);
        otherCookieManager.setAcceptCookie(true);

        Assert.assertFalse(defaultCookieManager.acceptCookie());
        Assert.assertTrue(otherCookieManager.acceptCookie());
    }

    @Test
    @MediumTest
    @OnlyRunIn(MULTI_PROCESS)
    @Feature({"AndroidWebView"})
    public void testSeparateProfilesHaveSeparateRenderProcesses() throws Throwable {
        mRule.startBrowserProcess();
        final AwBrowserContext profile = mRule.getProfileSync("my-profile", true);
        final AwContents firstAwContents = mRule.createAwContents();
        final AwContents secondAwContents = mRule.createAwContents(profile);

        TestWebServer webServer = TestWebServer.start();
        String path = "/test.html";
        String responseStr = "<html><head><title>TEST!</title></head><body>HELLO!</body></html>";
        String url = webServer.setResponse(path, responseStr, new ArrayList<>());

        mRule.loadUrlSync(firstAwContents, mContentsClient.getOnPageFinishedHelper(), url);
        assertEquals(1, RenderProcessHostUtils.getCurrentRenderProcessCount());

        mRule.loadUrlSync(secondAwContents, mContentsClient.getOnPageFinishedHelper(), url);
        assertEquals(2, RenderProcessHostUtils.getCurrentRenderProcessCount());
        webServer.shutdown();
    }

    @Test
    @MediumTest
    @OnlyRunIn(MULTI_PROCESS)
    @Feature({"AndroidWebView"})
    public void testAwContentsWithSameProfileShareRenderProcess() throws Throwable {
        mRule.startBrowserProcess();
        final AwBrowserContext profile = mRule.getProfileSync("my-profile", true);
        final AwContents firstAwContents = mRule.createAwContents(profile);
        final AwContents secondAwContents = mRule.createAwContents(profile);

        TestWebServer webServer = TestWebServer.start();
        String path = "/test.html";
        String responseStr = "<html><head><title>TEST!</title></head><body>HELLO!</body></html>";
        String url = webServer.setResponse(path, responseStr, new ArrayList<>());

        mRule.loadUrlSync(firstAwContents, mContentsClient.getOnPageFinishedHelper(), url);
        assertEquals(1, RenderProcessHostUtils.getCurrentRenderProcessCount());

        mRule.loadUrlSync(secondAwContents, mContentsClient.getOnPageFinishedHelper(), url);
        assertEquals(1, RenderProcessHostUtils.getCurrentRenderProcessCount());
        webServer.shutdown();
    }

    private void validateCookies(
            AwCookieManager cookieManager, String url, String... expectedCookieNames) {
        final String responseCookie = cookieManager.getCookie(url);
        assertNotNull(responseCookie);
        String[] cookies = responseCookie.split(";");
        // Convert to sets, since Set#equals() hooks in nicely with assertEquals()
        Set<String> foundCookieNamesSet = new HashSet<String>();
        for (String cookie : cookies) {
            foundCookieNamesSet.add(cookie.substring(0, cookie.indexOf("=")).trim());
        }
        Set<String> expectedCookieNamesSet =
                new HashSet<String>(Arrays.asList(expectedCookieNames));
        assertEquals(
                "Found cookies list differs from expected list",
                expectedCookieNamesSet,
                foundCookieNamesSet);
    }

    @Test
    @LargeTest
    @OnlyRunIn(MULTI_PROCESS)
    @Feature({"AndroidWebView"})
    public void testInjectedJavascriptIsTransferredWhenProfileChanges() throws Throwable {
        mRule.startBrowserProcess();

        String listenerName = "injectedListener";
        String startupScript = listenerName + ".postMessage('success');";
        String[] injectDomains = {"*"};

        final AwContents webView = mRule.createAwContents();
        AwActivityTestRule.enableJavaScriptOnUiThread(webView);

        CallbackHelper testDoneHelper = new CallbackHelper();

        final WebMessageListener injectedListener =
                (payload, topLevelOrigin, sourceOrigin, isMainFrame, jsReplyProxy, ports) -> {
                    Assert.assertEquals("success", payload.getAsString());
                    testDoneHelper.notifyCalled();
                };

        // Setup a message listener and a startup script to post on to the listener.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    webView.addWebMessageListener(listenerName, injectDomains, injectedListener);
                    webView.addDocumentStartJavaScript(startupScript, injectDomains);
                });

        // Switch the profile after the JS objects have been injected, but before content is loaded.
        AwBrowserContext otherProfile = mRule.getProfileSync("other-profile", true);
        mRule.setBrowserContextSync(webView, otherProfile);

        // Load content using the new Context.
        try (TestWebServer server = TestWebServer.start()) {
            server.setResponse("/", "hello, world", new ArrayList<>());
            mRule.loadUrlSync(
                    webView, mContentsClient.getOnPageFinishedHelper(), server.getBaseUrl());
            Assert.assertEquals(
                    "Injected listener was missing",
                    "true",
                    mRule.executeJavaScriptAndWaitForResult(
                            webView, mContentsClient, listenerName + " != null"));
        }

        // Wait for the test to run (see injectedListener above).
        testDoneHelper.waitForOnly("Did not receive post message triggered by injected javascript");
    }
}

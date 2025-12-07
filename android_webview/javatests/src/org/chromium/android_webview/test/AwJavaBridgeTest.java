// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.webkit.JavascriptInterface;

import androidx.test.InstrumentationRegistry;
import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.Parameterized;
import org.junit.runners.Parameterized.UseParametersRunnerFactory;

import org.chromium.android_webview.AwContents;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.content_public.common.ContentSwitches;
import org.chromium.net.test.util.TestWebServer;

import java.util.Collections;
import java.util.List;

/** Test suite for the WebView specific JavaBridge features. */
@RunWith(Parameterized.class)
@UseParametersRunnerFactory(AwJUnit4ClassRunnerWithParameters.Factory.class)
public class AwJavaBridgeTest extends AwParameterizedTest {
    @Rule public AwActivityTestRule mActivityTestRule;

    private final TestAwContentsClient mContentsClient = new TestAwContentsClient();
    private AwTestContainerView mTestContainerView;

    // The system retains a strong ref to the last focused view (in InputMethodManager)
    // so allow for 1 'leaked' instance.
    private static final int MAX_IDLE_INSTANCES = 1;

    public AwJavaBridgeTest(AwSettingsMutation param) {
        this.mActivityTestRule = new AwActivityTestRule(param.getMutation());
    }

    @Before
    public void setUp() {
        mTestContainerView = mActivityTestRule.createAwTestContainerViewOnMainSync(mContentsClient);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    @CommandLineFlags.Add({ContentSwitches.HOST_RESOLVER_RULES + "=MAP * 127.0.0.1"})
    public void testAllowlist() throws Throwable {
        TestWebServer webServer = TestWebServer.start();
        String firstPartyOrigin = webServer.setServerHost("1porigin.test");
        String thirdPartyOrigin = firstPartyOrigin.replace("1porigin.test", "3porigin.test");

        String path = "/cookie_test.html";
        final String html = "<html>Hello World</html>";

        String url = webServer.setResponse(path, html, null);
        String thirdPartyUrl = url.replace(firstPartyOrigin, thirdPartyOrigin);

        final AwContents awContents = mTestContainerView.getAwContents();
        AwActivityTestRule.enableJavaScriptOnUiThread(awContents);

        class Foo {
            Foo(int value) {
                mValue = value;
            }

            @JavascriptInterface
            public int getValue() {
                return mValue;
            }

            private final int mValue;
        }

        // addJavascriptInterfaceOnUiThread returns any bad rules when adding an interface.
        Assert.assertEquals(
                AwActivityTestRule.addJavascriptInterfaceOnUiThread(
                        awContents,
                        new Foo(1),
                        "testNotAllowed",
                        List.of("http://notallowed.test")),
                Collections.emptyList());
        Assert.assertEquals(
                AwActivityTestRule.addJavascriptInterfaceOnUiThread(
                        awContents, new Foo(2), "testAllowed", List.of(firstPartyOrigin)),
                Collections.emptyList());
        Assert.assertEquals(
                AwActivityTestRule.addJavascriptInterfaceOnUiThread(
                        awContents, new Foo(3), "testAllowed3p", List.of(thirdPartyOrigin)),
                Collections.emptyList());
        Assert.assertEquals(
                AwActivityTestRule.addJavascriptInterfaceOnUiThread(
                        awContents, new Foo(4), "testUniversal", List.of("*")),
                Collections.emptyList());
        Assert.assertEquals(
                AwActivityTestRule.addJavascriptInterfaceOnUiThread(
                        awContents,
                        new Foo(1),
                        "testIllformed",
                        List.of(firstPartyOrigin, thirdPartyOrigin, "://ill_formed.test")),
                List.of("://ill_formed.test"));

        mActivityTestRule.loadUrlSync(awContents, mContentsClient.getOnPageFinishedHelper(), url);
        Assert.assertEquals(
                "null",
                mActivityTestRule.executeJavaScriptAndWaitForResult(
                        awContents, mContentsClient, "testNotAllowed.getValue()"));
        Assert.assertEquals(
                "2",
                mActivityTestRule.executeJavaScriptAndWaitForResult(
                        awContents, mContentsClient, "testAllowed.getValue()"));
        Assert.assertEquals(
                "null",
                mActivityTestRule.executeJavaScriptAndWaitForResult(
                        awContents, mContentsClient, "testAllowed3p.getValue()"));
        Assert.assertEquals(
                "4",
                mActivityTestRule.executeJavaScriptAndWaitForResult(
                        awContents, mContentsClient, "testUniversal.getValue()"));
        Assert.assertEquals(
                "null",
                mActivityTestRule.executeJavaScriptAndWaitForResult(
                        awContents, mContentsClient, "testIllformed.getValue()"));

        // Then navigated to a new url to confirm the allowlists are still applied appropriately
        mActivityTestRule.loadUrlSync(
                awContents, mContentsClient.getOnPageFinishedHelper(), thirdPartyUrl);
        Assert.assertEquals(
                "null",
                mActivityTestRule.executeJavaScriptAndWaitForResult(
                        awContents, mContentsClient, "testNotAllowed.getValue()"));
        Assert.assertEquals(
                "null",
                mActivityTestRule.executeJavaScriptAndWaitForResult(
                        awContents, mContentsClient, "testAllowed.getValue()"));
        Assert.assertEquals(
                "3",
                mActivityTestRule.executeJavaScriptAndWaitForResult(
                        awContents, mContentsClient, "testAllowed3p.getValue()"));
        Assert.assertEquals(
                "4",
                mActivityTestRule.executeJavaScriptAndWaitForResult(
                        awContents, mContentsClient, "testUniversal.getValue()"));
        Assert.assertEquals(
                "null",
                mActivityTestRule.executeJavaScriptAndWaitForResult(
                        awContents, mContentsClient, "testIllformed.getValue()"));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    public void testDestroyFromJavaObject() throws Throwable {
        final String html = "<html>Hello World</html>";
        final TestAwContentsClient client2 = new TestAwContentsClient();
        final AwTestContainerView view2 =
                mActivityTestRule.createAwTestContainerViewOnMainSync(client2);
        final AwContents awContents = mTestContainerView.getAwContents();

        class Foo {
            @JavascriptInterface
            public void destroy() {
                try {
                    InstrumentationRegistry.getInstrumentation()
                            .runOnMainSync(() -> awContents.destroy());
                    // Destroying one AwContents from within the JS callback should still
                    // leave others functioning. Note that we must do this asynchronously,
                    // as Blink thread is currently blocked waiting for this method to finish.
                    mActivityTestRule.loadDataAsync(
                            view2.getAwContents(), html, "text/html", false);
                } catch (Throwable t) {
                    throw new RuntimeException(t);
                }
            }
        }

        AwActivityTestRule.enableJavaScriptOnUiThread(awContents);
        AwActivityTestRule.addJavascriptInterfaceOnUiThread(awContents, new Foo(), "test");

        mActivityTestRule.loadDataSync(
                awContents, mContentsClient.getOnPageFinishedHelper(), html, "text/html", false);

        // Ensure the JS interface object is there, and invoke the test method.
        Assert.assertEquals(
                "\"function\"",
                mActivityTestRule.executeJavaScriptAndWaitForResult(
                        awContents, mContentsClient, "typeof test.destroy"));
        int currentCallCount = client2.getOnPageFinishedHelper().getCallCount();
        ThreadUtils.runOnUiThreadBlocking(
                () -> awContents.evaluateJavaScriptForTests("test.destroy()", null));

        client2.getOnPageFinishedHelper().waitForCallback(currentCallCount);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    public void testTwoWebViewsCreatedSimultaneously() throws Throwable {
        final AwContents awContents1 = mTestContainerView.getAwContents();
        final TestAwContentsClient client2 = new TestAwContentsClient();
        final AwTestContainerView view2 =
                mActivityTestRule.createAwTestContainerViewOnMainSync(client2);
        final AwContents awContents2 = view2.getAwContents();

        AwActivityTestRule.enableJavaScriptOnUiThread(awContents1);
        AwActivityTestRule.enableJavaScriptOnUiThread(awContents2);

        class Foo {
            Foo(int value) {
                mValue = value;
            }

            @JavascriptInterface
            public int getValue() {
                return mValue;
            }

            private final int mValue;
        }

        AwActivityTestRule.addJavascriptInterfaceOnUiThread(awContents1, new Foo(1), "test");
        AwActivityTestRule.addJavascriptInterfaceOnUiThread(awContents2, new Foo(2), "test");
        final String html = "<html>Hello World</html>";
        mActivityTestRule.loadDataSync(
                awContents1, mContentsClient.getOnPageFinishedHelper(), html, "text/html", false);
        mActivityTestRule.loadDataSync(
                awContents2, client2.getOnPageFinishedHelper(), html, "text/html", false);

        Assert.assertEquals(
                "1",
                mActivityTestRule.executeJavaScriptAndWaitForResult(
                        awContents1, mContentsClient, "test.getValue()"));
        Assert.assertEquals(
                "2",
                mActivityTestRule.executeJavaScriptAndWaitForResult(
                        awContents2, client2, "test.getValue()"));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    public void testTwoWebViewsSecondCreatedAfterLoadingInFirst() throws Throwable {
        final AwContents awContents1 = mTestContainerView.getAwContents();
        AwActivityTestRule.enableJavaScriptOnUiThread(awContents1);

        class Foo {
            Foo(int value) {
                mValue = value;
            }

            @JavascriptInterface
            public int getValue() {
                return mValue;
            }

            private final int mValue;
        }

        AwActivityTestRule.addJavascriptInterfaceOnUiThread(awContents1, new Foo(1), "test");
        final String html = "<html>Hello World</html>";
        mActivityTestRule.loadDataSync(
                awContents1, mContentsClient.getOnPageFinishedHelper(), html, "text/html", false);
        Assert.assertEquals(
                "1",
                mActivityTestRule.executeJavaScriptAndWaitForResult(
                        awContents1, mContentsClient, "test.getValue()"));

        final TestAwContentsClient client2 = new TestAwContentsClient();
        final AwTestContainerView view2 =
                mActivityTestRule.createAwTestContainerViewOnMainSync(client2);
        final AwContents awContents2 = view2.getAwContents();
        AwActivityTestRule.enableJavaScriptOnUiThread(awContents2);

        AwActivityTestRule.addJavascriptInterfaceOnUiThread(awContents2, new Foo(2), "test");
        mActivityTestRule.loadDataSync(
                awContents2, client2.getOnPageFinishedHelper(), html, "text/html", false);

        Assert.assertEquals(
                "1",
                mActivityTestRule.executeJavaScriptAndWaitForResult(
                        awContents1, mContentsClient, "test.getValue()"));
        Assert.assertEquals(
                "2",
                mActivityTestRule.executeJavaScriptAndWaitForResult(
                        awContents2, client2, "test.getValue()"));
    }
}

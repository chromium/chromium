// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import static org.chromium.android_webview.test.AwActivityTestRule.CHECK_INTERVAL;

import android.content.Context;
import android.content.ContextWrapper;
import android.os.ResultReceiver;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.LargeTest;
import android.support.test.filters.SmallTest;
import android.webkit.JavascriptInterface;

import org.junit.After;
import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.gfx.AwGLFunctor;
import org.chromium.android_webview.test.AwActivityTestRule.TestDependencyFactory;
import org.chromium.base.test.util.Feature;
import org.chromium.content_public.browser.ImeAdapter;
import org.chromium.content_public.browser.WebContentsAccessibility;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.content_public.common.ContentUrlConstants;

/**
 * AwContents garbage collection tests. Most apps relies on WebView being
 * garbage collected to release memory. These tests ensure that nothing
 * accidentally prevents AwContents from garbage collected, leading to leaks.
 * See crbug.com/544098 for why @DisableHardwareAccelerationForTest is needed.
 */
@RunWith(AwJUnit4ClassRunner.class)
public class AwContentsGarbageCollectionTest {
    @Rule
    public AwActivityTestRule mActivityTestRule = new AwActivityTestRule() {
        @Override
        public TestDependencyFactory createTestDependencyFactory() {
            if (mOverridenFactory == null) {
                return new TestDependencyFactory();
            } else {
                return mOverridenFactory;
            }
        }
    };

    // The system retains a strong ref to the last focused view (in InputMethodManager)
    // so allow for 1 'leaked' instance.
    private static final int MAX_IDLE_INSTANCES = 1;

    private TestDependencyFactory mOverridenFactory;

    @After
    public void tearDown() {
        mOverridenFactory = null;
    }

    private static class StrongRefTestContext extends ContextWrapper {
        private AwContents mAwContents;
        public void setAwContentsStrongRef(AwContents awContents) {
            mAwContents = awContents;
        }

        public StrongRefTestContext(Context context) {
            super(context);
        }
    }

    private static class GcTestDependencyFactory extends TestDependencyFactory {
        private final StrongRefTestContext mContext;

        public GcTestDependencyFactory(StrongRefTestContext context) {
            mContext = context;
        }

        @Override
        public AwTestContainerView createAwTestContainerView(
                AwTestRunnerActivity activity, boolean allowHardwareAcceleration) {
            if (activity != mContext.getBaseContext()) Assert.fail();
            return new AwTestContainerView(mContext, allowHardwareAcceleration);
        }
    }

    private static class StrongRefTestAwContentsClient extends TestAwContentsClient {
        private AwContents mAwContentsStrongRef;
        public void setAwContentsStrongRef(AwContents awContents) {
            mAwContentsStrongRef = awContents;
        }
    }

    @Test
    @DisableHardwareAccelerationForTest
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testCreateAndGcOneTime() {
        gcAndCheckAllAwContentsDestroyed();

        TestAwContentsClient client = new TestAwContentsClient();
        AwTestContainerView containerViews[] = new AwTestContainerView[MAX_IDLE_INSTANCES + 1];
        for (int i = 0; i < containerViews.length; i++) {
            containerViews[i] = mActivityTestRule.createAwTestContainerViewOnMainSync(client);
            mActivityTestRule.loadUrlAsync(
                    containerViews[i].getAwContents(), ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL);
        }

        for (int i = 0; i < containerViews.length; i++) {
            containerViews[i] = null;
        }
        containerViews = null;
        removeAllViews();
        gcAndCheckAllAwContentsDestroyed();
    }

    @Test
    @DisableHardwareAccelerationForTest
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testHoldKeyboardResultReceiver() throws Throwable {
        gcAndCheckAllAwContentsDestroyed();

        TestAwContentsClient client = new TestAwContentsClient();
        AwTestContainerView containerViews[] = new AwTestContainerView[MAX_IDLE_INSTANCES + 1];
        ResultReceiver resultReceivers[] = new ResultReceiver[MAX_IDLE_INSTANCES + 1];
        for (int i = 0; i < containerViews.length; i++) {
            final AwTestContainerView containerView =
                    mActivityTestRule.createAwTestContainerViewOnMainSync(client);
            containerViews[i] = containerView;
            mActivityTestRule.loadUrlAsync(
                    containerView.getAwContents(), ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL);
            // When we call showSoftInput(), we pass a ResultReceiver object as a parameter.
            // Android framework will hold the object reference until the matching
            // ResultReceiver in InputMethodService (IME app) gets garbage-collected.
            // WebView object wouldn't get gc'ed once OSK shows up because of this.
            // It is difficult to show keyboard and wait until input method window shows up.
            // Instead, we simply emulate Android's behavior by keeping strong references.
            // See crbug.com/595613 for details.
            resultReceivers[i] = TestThreadUtils.runOnUiThreadBlocking(
                    () -> ImeAdapter.fromWebContents(containerView.getWebContents())
                                       .getNewShowKeyboardReceiver());
        }

        for (int i = 0; i < containerViews.length; i++) {
            containerViews[i] = null;
        }
        containerViews = null;
        removeAllViews();
        gcAndCheckAllAwContentsDestroyed();
    }

    @Test
    @DisableHardwareAccelerationForTest
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testAccessibility() {
        gcAndCheckAllAwContentsDestroyed();

        TestAwContentsClient client = new TestAwContentsClient();
        AwTestContainerView containerViews[] = new AwTestContainerView[MAX_IDLE_INSTANCES + 1];
        for (int i = 0; i < containerViews.length; i++) {
            final AwTestContainerView containerView =
                    mActivityTestRule.createAwTestContainerViewOnMainSync(client);
            containerViews[i] = containerView;
            mActivityTestRule.loadUrlAsync(
                    containerViews[i].getAwContents(), ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL);
            TestThreadUtils.runOnUiThreadBlocking(() -> {
                WebContentsAccessibility webContentsA11y =
                        WebContentsAccessibility.fromWebContents(containerView.getWebContents());
                webContentsA11y.setState(true);
                // Enable a11y for testing.
                webContentsA11y.setAccessibilityEnabledForTesting();
                // Initialize native object.
                containerView.getAccessibilityNodeProvider();
                Assert.assertTrue(webContentsA11y.isAccessibilityEnabled());
            });
        }

        for (int i = 0; i < containerViews.length; i++) {
            containerViews[i] = null;
        }
        containerViews = null;
        removeAllViews();
        gcAndCheckAllAwContentsDestroyed();
    }

    @Test
    @DisableHardwareAccelerationForTest
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testReferenceFromClient() {
        gcAndCheckAllAwContentsDestroyed();

        AwTestContainerView containerViews[] = new AwTestContainerView[MAX_IDLE_INSTANCES + 1];
        for (int i = 0; i < containerViews.length; i++) {
            StrongRefTestAwContentsClient client = new StrongRefTestAwContentsClient();
            containerViews[i] = mActivityTestRule.createAwTestContainerViewOnMainSync(client);
            mActivityTestRule.loadUrlAsync(
                    containerViews[i].getAwContents(), ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL);
        }

        for (int i = 0; i < containerViews.length; i++) {
            containerViews[i] = null;
        }
        containerViews = null;
        removeAllViews();
        gcAndCheckAllAwContentsDestroyed();
    }

    @Test
    @DisableHardwareAccelerationForTest
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testReferenceFromContext() {
        gcAndCheckAllAwContentsDestroyed();

        TestAwContentsClient client = new TestAwContentsClient();
        AwTestContainerView containerViews[] = new AwTestContainerView[MAX_IDLE_INSTANCES + 1];
        for (int i = 0; i < containerViews.length; i++) {
            StrongRefTestContext context =
                    new StrongRefTestContext(mActivityTestRule.getActivity());
            mOverridenFactory = new GcTestDependencyFactory(context);
            containerViews[i] = mActivityTestRule.createAwTestContainerViewOnMainSync(client);
            mOverridenFactory = null;
            mActivityTestRule.loadUrlAsync(
                    containerViews[i].getAwContents(), ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL);
        }

        for (int i = 0; i < containerViews.length; i++) {
            containerViews[i] = null;
        }
        containerViews = null;
        removeAllViews();
        gcAndCheckAllAwContentsDestroyed();
    }

    @Test
    @DisableHardwareAccelerationForTest
    @LargeTest
    @Feature({"AndroidWebView"})
    public void testCreateAndGcManyTimes() {
        gcAndCheckAllAwContentsDestroyed();

        final int concurrentInstances = 4;
        final int repetitions = 16;

        for (int i = 0; i < repetitions; ++i) {
            for (int j = 0; j < concurrentInstances; ++j) {
                StrongRefTestAwContentsClient client = new StrongRefTestAwContentsClient();
                StrongRefTestContext context =
                        new StrongRefTestContext(mActivityTestRule.getActivity());
                mOverridenFactory = new GcTestDependencyFactory(context);
                AwTestContainerView view =
                        mActivityTestRule.createAwTestContainerViewOnMainSync(client);
                mOverridenFactory = null;
                // Embedding app can hold onto a strong ref to the WebView from either
                // WebViewClient or WebChromeClient. That should not prevent WebView from
                // gc-ed. We simulate that behavior by making the equivalent change here,
                // have AwContentsClient hold a strong ref to the AwContents object.
                client.setAwContentsStrongRef(view.getAwContents());
                context.setAwContentsStrongRef(view.getAwContents());
                mActivityTestRule.loadUrlAsync(
                        view.getAwContents(), ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL);
            }
            Assert.assertTrue(AwContents.getNativeInstanceCount() >= concurrentInstances);
            Assert.assertTrue(AwContents.getNativeInstanceCount() <= (i + 1) * concurrentInstances);
            removeAllViews();
        }

        gcAndCheckAllAwContentsDestroyed();
    }

    @Test
    @DisableHardwareAccelerationForTest
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testGcAfterUsingJavascriptObject() throws Throwable {
        // Javascript object with a reference to WebView.
        class Test {
            Test(int value, AwContents awContents) {
                mValue = value;
                mAwContents = awContents;
            }
            @JavascriptInterface
            public int getValue() {
                return mValue;
            }
            public AwContents getAwContents() {
                return mAwContents;
            }
            private int mValue;
            private AwContents mAwContents;
        }
        String html = "<html>Hello World</html>";
        AwTestContainerView[] containerViews = new AwTestContainerView[MAX_IDLE_INSTANCES + 1];

        TestAwContentsClient contentsClient = new TestAwContentsClient();
        for (int i = 0; i < MAX_IDLE_INSTANCES + 1; ++i) {
            containerViews[i] =
                    mActivityTestRule.createAwTestContainerViewOnMainSync(contentsClient);
            AwActivityTestRule.enableJavaScriptOnUiThread(containerViews[i].getAwContents());
            final AwContents awContents = containerViews[i].getAwContents();
            final Test jsObject = new Test(i, awContents);
            AwActivityTestRule.addJavascriptInterfaceOnUiThread(awContents, jsObject, "test");
            mActivityTestRule.loadDataSync(
                    awContents, contentsClient.getOnPageFinishedHelper(), html, "text/html", false);
            Assert.assertEquals(String.valueOf(i),
                    mActivityTestRule.executeJavaScriptAndWaitForResult(
                            awContents, contentsClient, "test.getValue()"));
        }

        containerViews[0] = null;
        containerViews[1] = null;
        containerViews = null;
        removeAllViews();
        gcAndCheckAllAwContentsDestroyed();
    }

    private void removeAllViews() {
        InstrumentationRegistry.getInstrumentation().runOnMainSync(
                () -> mActivityTestRule.getActivity().removeAllViews());
    }

    private void gcAndCheckAllAwContentsDestroyed() {
        Runtime.getRuntime().gc();

        Criteria criteria = new Criteria() {
            @Override
            public boolean isSatisfied() {
                try {
                    return TestThreadUtils.runOnUiThreadBlocking(() -> {
                        int count_aw_contents = AwContents.getNativeInstanceCount();
                        int count_aw_functor = AwGLFunctor.getNativeInstanceCount();
                        return count_aw_contents <= MAX_IDLE_INSTANCES
                                && count_aw_functor <= MAX_IDLE_INSTANCES;
                    });
                } catch (Exception e) {
                    return false;
                }
            }
        };

        // Depending on a single gc call can make this test flaky. It's possible
        // that the WebView still has transient references during load so it does not get
        // gc-ed in the one gc-call above. Instead call gc again if exit criteria fails to
        // catch this case.
        final long timeoutBetweenGcMs = 1000L;
        for (int i = 0; i < 15; ++i) {
            try {
                CriteriaHelper.pollInstrumentationThread(
                        criteria, timeoutBetweenGcMs, CHECK_INTERVAL);
                break;
            } catch (AssertionError e) {
                Runtime.getRuntime().gc();
            }
        }

        Assert.assertTrue(criteria.isSatisfied());
    }
}

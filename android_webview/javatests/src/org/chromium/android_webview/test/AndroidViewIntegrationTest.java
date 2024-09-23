// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.view.View;
import android.view.ViewGroup.LayoutParams;
import android.widget.LinearLayout;

import androidx.test.InstrumentationRegistry;
import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.Parameterized;
import org.junit.runners.Parameterized.UseParametersRunnerFactory;

import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.AwContentsClient;
import org.chromium.android_webview.AwLayoutSizer;
import org.chromium.android_webview.test.util.CommonResources;
import org.chromium.android_webview.test.util.GraphicsTestUtils;
import org.chromium.base.Log;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.Feature;

import java.util.concurrent.atomic.AtomicReference;

import javax.annotation.concurrent.GuardedBy;

/** Tests for certain edge cases related to integrating with the Android view system. */
@RunWith(Parameterized.class)
@UseParametersRunnerFactory(AwJUnit4ClassRunnerWithParameters.Factory.class)
public class AndroidViewIntegrationTest extends AwParameterizedTest {
    @Rule public AwActivityTestRule mActivityTestRule;

    public AndroidViewIntegrationTest(AwSettingsMutation param) {
        mActivityTestRule =
                new AwActivityTestRule(param.getMutation()) {
                    @Override
                    public TestDependencyFactory createTestDependencyFactory() {
                        return new TestDependencyFactory() {
                            @Override
                            public AwLayoutSizer createLayoutSizer() {
                                return new TestAwLayoutSizer();
                            }
                        };
                    }
                };
    }

    private static final String TAG = "AndroidViewTest"; // 20 max characters
    // TODO(crbug.com/41451075): turn this off once we can get some details about flakes.
    private static final boolean DEBUG = true;
    private static final int CONTENT_SIZE_CHANGE_STABILITY_TIMEOUT_MS = 1000;

    private static class OnContentSizeChangedHelper extends CallbackHelper {
        private final Object mLock = new Object();

        @GuardedBy("mLock")
        private int mWidth;

        @GuardedBy("mLock")
        private int mHeight;

        public int getWidth() {
            assert getCallCount() > 0;
            synchronized (mLock) {
                return mWidth;
            }
        }

        public int getHeight() {
            assert getCallCount() > 0;
            synchronized (mLock) {
                return mHeight;
            }
        }

        public void onContentSizeChanged(int widthCss, int heightCss) {
            synchronized (mLock) {
                mWidth = widthCss;
                mHeight = heightCss;
            }
            notifyCalled();
        }
    }

    private OnContentSizeChangedHelper mOnContentSizeChangedHelper =
            new OnContentSizeChangedHelper();
    private CallbackHelper mOnPageScaleChangedHelper = new CallbackHelper();
    private AwTestContainerView mTestContainerView;

    private class TestAwLayoutSizer extends AwLayoutSizer {
        @Override
        public void onContentSizeChanged(int widthCss, int heightCss) {
            super.onContentSizeChanged(widthCss, heightCss);
            if (mOnContentSizeChangedHelper != null) {
                mOnContentSizeChangedHelper.onContentSizeChanged(widthCss, heightCss);
            }
        }

        @Override
        public void onPageScaleChanged(float pageScaleFactor) {
            super.onPageScaleChanged(pageScaleFactor);
            if (mOnPageScaleChangedHelper != null) {
                mOnPageScaleChangedHelper.notifyCalled();
            }
        }
    }

    final LinearLayout.LayoutParams mWrapContentLayoutParams =
            new LinearLayout.LayoutParams(LayoutParams.WRAP_CONTENT, LayoutParams.WRAP_CONTENT);

    private AwTestContainerView createCustomTestContainerViewOnMainSync(
            final AwContentsClient awContentsClient, final int visibility) {
        final AtomicReference<AwTestContainerView> testContainerView = new AtomicReference<>();
        InstrumentationRegistry.getInstrumentation()
                .runOnMainSync(
                        () -> {
                            testContainerView.set(
                                    mActivityTestRule.createAwTestContainerView(awContentsClient));
                            testContainerView.get().setLayoutParams(mWrapContentLayoutParams);
                            testContainerView.get().setVisibility(visibility);
                        });
        return testContainerView.get();
    }

    private AwTestContainerView createDetachedTestContainerViewOnMainSync(
            final AwContentsClient awContentsClient) {
        final AtomicReference<AwTestContainerView> testContainerView = new AtomicReference<>();
        InstrumentationRegistry.getInstrumentation()
                .runOnMainSync(
                        () ->
                                testContainerView.set(
                                        mActivityTestRule.createDetachedAwTestContainerView(
                                                awContentsClient)));
        return testContainerView.get();
    }

    private void assertZeroHeight(final AwTestContainerView testContainerView) {
        // Make sure the test isn't broken by the view having a non-zero height.
        InstrumentationRegistry.getInstrumentation()
                .runOnMainSync(() -> Assert.assertEquals(0, testContainerView.getHeight()));
    }

    private int getRootLayoutWidthOnMainThread() {
        final AtomicReference<Integer> width = new AtomicReference<>();
        InstrumentationRegistry.getInstrumentation()
                .runOnMainSync(
                        () ->
                                width.set(
                                        Integer.valueOf(
                                                mActivityTestRule
                                                        .getActivity()
                                                        .getRootLayoutWidth())));
        return width.get();
    }

    /**
     * This checks for issues related to loading content into a 0x0 view.
     *
     * A 0x0 sized view is common if the WebView is set to wrap_content and newly created. The
     * expected behavior is for the WebView to expand after some content is loaded.
     * In Chromium it would be valid to not load or render content into a WebContents with a 0x0
     * view (since the user can't see it anyway) and only do so after the view's size is non-zero.
     * Such behavior is unacceptable for the WebView and this test is to ensure that such behavior
     * is not re-introduced.
     */
    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testZeroByZeroViewLoadsContent() throws Throwable {
        final TestAwContentsClient contentsClient = new TestAwContentsClient();
        mTestContainerView = createCustomTestContainerViewOnMainSync(contentsClient, View.VISIBLE);
        assertZeroHeight(mTestContainerView);

        final int contentSizeChangeCallCount = mOnContentSizeChangedHelper.getCallCount();
        mActivityTestRule.loadDataAsync(
                mTestContainerView.getAwContents(), CommonResources.ABOUT_HTML, "text/html", false);
        mOnContentSizeChangedHelper.waitForCallback(contentSizeChangeCallCount);
        Assert.assertTrue(mOnContentSizeChangedHelper.getHeight() > 0);
    }

    /**
     * Check that a content size change notification is issued when the view is invisible.
     *
     * This makes sure that any optimizations related to the view's visibility don't inhibit
     * the ability to load pages. Many applications keep the WebView hidden when it's loading.
     */
    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testInvisibleViewLoadsContent() throws Throwable {
        final TestAwContentsClient contentsClient = new TestAwContentsClient();
        mTestContainerView =
                createCustomTestContainerViewOnMainSync(contentsClient, View.INVISIBLE);
        assertZeroHeight(mTestContainerView);

        final int contentSizeChangeCallCount = mOnContentSizeChangedHelper.getCallCount();
        mActivityTestRule.loadDataAsync(
                mTestContainerView.getAwContents(), CommonResources.ABOUT_HTML, "text/html", false);
        mOnContentSizeChangedHelper.waitForCallback(contentSizeChangeCallCount);
        Assert.assertTrue(mOnContentSizeChangedHelper.getHeight() > 0);

        InstrumentationRegistry.getInstrumentation()
                .runOnMainSync(
                        () ->
                                Assert.assertEquals(
                                        View.INVISIBLE, mTestContainerView.getVisibility()));
    }

    /** Check that a content size change notification is sent even if the WebView is off screen. */
    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testDisconnectedViewLoadsContent() throws Throwable {
        final TestAwContentsClient contentsClient = new TestAwContentsClient();
        mTestContainerView = createDetachedTestContainerViewOnMainSync(contentsClient);
        assertZeroHeight(mTestContainerView);

        final int contentSizeChangeCallCount = mOnContentSizeChangedHelper.getCallCount();
        final int pageScaleChangeCallCount = mOnPageScaleChangedHelper.getCallCount();
        mActivityTestRule.loadDataAsync(
                mTestContainerView.getAwContents(), CommonResources.ABOUT_HTML, "text/html", false);
        mOnPageScaleChangedHelper.waitForCallback(pageScaleChangeCallCount);
        mOnContentSizeChangedHelper.waitForCallback(contentSizeChangeCallCount);
        Assert.assertTrue(mOnContentSizeChangedHelper.getHeight() > 0);
    }

    private String makeHtmlPageOfSize(int widthCss, int heightCss, boolean heightPercent) {
        String content = "<div class=\"normal\">a</div>";
        if (heightPercent) content += "<div class=\"heightPercent\"></div>";
        return CommonResources.makeHtmlPageFrom(
                "<style type=\"text/css\">"
                        + "  body { margin:0px; padding:0px; } "
                        + "  .normal { "
                        + "    width:"
                        + widthCss
                        + "px; "
                        + "    height:"
                        + heightCss
                        + "px; "
                        + "    background-color: #227788; "
                        + "  } "
                        + "  .heightPercent { "
                        + "    height: 150%; "
                        + "    background-color: blue; "
                        + "  } "
                        + "</style>",
                content);
    }

    private void waitForContentSizeToChangeTo(
            OnContentSizeChangedHelper helper, int callCount, int widthCss, int heightCss)
            throws Exception {
        final int maxSizeChangeNotificationsToWaitFor = 5;
        for (int i = 0; i < maxSizeChangeNotificationsToWaitFor; i++) {
            helper.waitForCallback(callCount + i);
            if (DEBUG) {
                Log.i(
                        TAG,
                        "i: "
                                + i
                                + ", height: "
                                + helper.getHeight()
                                + ", width: "
                                + helper.getWidth());
            }
            if ((heightCss == -1 || helper.getHeight() == heightCss)
                    && (widthCss == -1 || helper.getWidth() == widthCss)) {
                return;
            }
        }
        Assert.fail("The expected contents size was not reached in max # of trials.");
    }

    private void loadPageOfSizeAndWaitForSizeChange(
            AwContents awContents,
            OnContentSizeChangedHelper helper,
            int widthCss,
            int heightCss,
            boolean heightPercent)
            throws Exception {
        // loadDataAsync loads HTML as a data URI, which requires encoding '#' characters as '%23'.
        final String htmlData =
                makeHtmlPageOfSize(widthCss, heightCss, heightPercent).replace("#", "%23");
        final int contentSizeChangeCallCount = helper.getCallCount();
        mActivityTestRule.loadDataAsync(awContents, htmlData, "text/html", false);

        waitForContentSizeToChangeTo(helper, contentSizeChangeCallCount, widthCss, heightCss);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testSizeUpdateWhenDetached() throws Throwable {
        final TestAwContentsClient contentsClient = new TestAwContentsClient();
        mTestContainerView = createDetachedTestContainerViewOnMainSync(contentsClient);
        assertZeroHeight(mTestContainerView);

        final int contentWidthCss = 142;
        final int contentHeightCss = 180;

        loadPageOfSizeAndWaitForSizeChange(
                mTestContainerView.getAwContents(),
                mOnContentSizeChangedHelper,
                contentWidthCss,
                contentHeightCss,
                false);
    }

    public void waitForNoLayoutsPending() throws InterruptedException {
        // This is to make sure that there are no more pending size change notifications. Ideally
        // we'd assert that the renderer is idle (has no pending layout passes) but that would
        // require quite a bit of plumbing, so we just wait a bit and make sure the size hadn't
        // changed.
        Thread.sleep(CONTENT_SIZE_CHANGE_STABILITY_TIMEOUT_MS);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testAbsolutePositionContributesToContentSize() throws Throwable {
        final TestAwContentsClient contentsClient = new TestAwContentsClient();
        mTestContainerView = createDetachedTestContainerViewOnMainSync(contentsClient);
        assertZeroHeight(mTestContainerView);

        final int widthCss = 142;
        final int heightCss = 180;

        final String htmlData =
                CommonResources.makeHtmlPageFrom(
                        "<style type=\"text/css\">"
                                + "  body { margin:0px; padding:0px; } "
                                + "  div { "
                                + "    position: absolute; "
                                + "    width:"
                                + widthCss
                                + "px; "
                                + "    height:"
                                + heightCss
                                + "px; "
                                + "    background-color: red; "
                                + "  } "
                                + "</style>",
                        "<div>a</div>");

        final int contentSizeChangeCallCount = mOnContentSizeChangedHelper.getCallCount();
        Assert.assertEquals(0, contentSizeChangeCallCount);
        mActivityTestRule.loadDataAsync(
                mTestContainerView.getAwContents(), htmlData, "text/html", false);

        waitForContentSizeToChangeTo(
                mOnContentSizeChangedHelper, contentSizeChangeCallCount, widthCss, heightCss);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testViewIsNotBlankInWrapContentsMode() throws Throwable {
        final TestAwContentsClient contentsClient = new TestAwContentsClient();
        mTestContainerView = createCustomTestContainerViewOnMainSync(contentsClient, View.VISIBLE);
        assertZeroHeight(mTestContainerView);

        final double deviceDIPScale =
                GraphicsTestUtils.dipScaleForContext(mTestContainerView.getContext());
        final int contentHeightCss = 180;

        // In wrap-content mode the AwLayoutSizer will size the view to be as wide as the parent
        // view.
        final int expectedWidthCss =
                (int) Math.ceil(getRootLayoutWidthOnMainThread() / deviceDIPScale);
        final int expectedHeightCss = contentHeightCss;

        loadPageOfSizeAndWaitForSizeChange(
                mTestContainerView.getAwContents(),
                mOnContentSizeChangedHelper,
                expectedWidthCss,
                expectedHeightCss,
                false);

        GraphicsTestUtils.pollForBackgroundColor(mTestContainerView.getAwContents(), 0xFF227788);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @SkipMutations(reason = "This test depends on AwSettings.setUseWideViewPort(false)")
    public void testViewSizedCorrectlyInWrapContentMode() throws Throwable {
        final TestAwContentsClient contentsClient = new TestAwContentsClient();
        mTestContainerView = createCustomTestContainerViewOnMainSync(contentsClient, View.VISIBLE);
        assertZeroHeight(mTestContainerView);

        final double deviceDIPScale =
                GraphicsTestUtils.dipScaleForContext(mTestContainerView.getContext());
        final int contentHeightCss = 180;

        // In wrap-content mode the AwLayoutSizer will size the view to be as wide as the parent
        // view.
        final int expectedWidthCss =
                (int) Math.ceil(getRootLayoutWidthOnMainThread() / deviceDIPScale);
        final int expectedHeightCss = contentHeightCss;

        loadPageOfSizeAndWaitForSizeChange(
                mTestContainerView.getAwContents(),
                mOnContentSizeChangedHelper,
                expectedWidthCss,
                expectedHeightCss,
                false);

        waitForNoLayoutsPending();
        Assert.assertEquals(expectedWidthCss, mOnContentSizeChangedHelper.getWidth());
        Assert.assertEquals(expectedHeightCss, mOnContentSizeChangedHelper.getHeight());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @SkipMutations(reason = "This test depends on AwSettings.setUseWideViewPort(false)")
    public void testViewSizedCorrectlyInWrapContentModeWithDynamicContents() throws Throwable {
        final TestAwContentsClient contentsClient = new TestAwContentsClient();
        mTestContainerView = createCustomTestContainerViewOnMainSync(contentsClient, View.VISIBLE);
        assertZeroHeight(mTestContainerView);

        final double deviceDIPScale =
                GraphicsTestUtils.dipScaleForContext(mTestContainerView.getContext());
        final int contentHeightCss = 180;

        final int expectedWidthCss =
                (int) Math.ceil(getRootLayoutWidthOnMainThread() / deviceDIPScale);
        final int expectedHeightCss = contentHeightCss;

        loadPageOfSizeAndWaitForSizeChange(
                mTestContainerView.getAwContents(),
                mOnContentSizeChangedHelper,
                expectedWidthCss,
                contentHeightCss,
                true);

        waitForNoLayoutsPending();
        Assert.assertEquals(expectedWidthCss, mOnContentSizeChangedHelper.getWidth());
        Assert.assertEquals(expectedHeightCss, mOnContentSizeChangedHelper.getHeight());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @SkipMutations(reason = "This test depends on AwSettings.setUseWideViewPort(false)")
    public void testReceivingSizeAfterLoadUpdatesLayout() throws Throwable {
        final TestAwContentsClient contentsClient = new TestAwContentsClient();
        mTestContainerView = createDetachedTestContainerViewOnMainSync(contentsClient);
        final AwContents awContents = mTestContainerView.getAwContents();

        final double deviceDIPScale =
                GraphicsTestUtils.dipScaleForContext(mTestContainerView.getContext());
        final int physicalWidth = 600;
        final int spanWidth = 42;
        final int expectedWidthCss = (int) Math.ceil(physicalWidth / deviceDIPScale);

        StringBuilder htmlBuilder = new StringBuilder("<html><body style='margin:0px;'>");
        final String spanBlock =
                "<span style='width: " + spanWidth + "px; display: inline-block;'>a</span>";
        for (int i = 0; i < 10; ++i) {
            htmlBuilder.append(spanBlock);
        }
        htmlBuilder.append("</body></html>");

        int contentSizeChangeCallCount = mOnContentSizeChangedHelper.getCallCount();
        mActivityTestRule.loadDataAsync(awContents, htmlBuilder.toString(), "text/html", false);
        // Because we're loading the contents into a detached WebView its layout size is 0x0 and as
        // a result of that the paragraph will be formated such that each word is on a separate
        // line.
        waitForContentSizeToChangeTo(
                mOnContentSizeChangedHelper, contentSizeChangeCallCount, spanWidth, -1);

        final int narrowLayoutHeight = mOnContentSizeChangedHelper.getHeight();

        contentSizeChangeCallCount = mOnContentSizeChangedHelper.getCallCount();
        InstrumentationRegistry.getInstrumentation()
                .runOnMainSync(() -> mTestContainerView.onSizeChanged(physicalWidth, 0, 0, 0));
        mOnContentSizeChangedHelper.waitForCallback(contentSizeChangeCallCount);

        // As a result of calling the onSizeChanged method the layout size should be updated to
        // match the width of the webview and the text we previously loaded should reflow making the
        // contents width match the WebView width.
        Assert.assertEquals(expectedWidthCss, mOnContentSizeChangedHelper.getWidth());
        Assert.assertTrue(mOnContentSizeChangedHelper.getHeight() < narrowLayoutHeight);
        Assert.assertTrue(mOnContentSizeChangedHelper.getHeight() > 0);
    }
}

// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test.services;

import static org.chromium.android_webview.test.OnlyRunIn.ProcessMode.MULTI_PROCESS;

import android.content.Context;
import android.view.ViewGroup;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.Parameterized;
import org.junit.runners.Parameterized.UseParametersRunnerFactory;

import org.chromium.android_webview.AwBrowserContext;
import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.AwContents.DependencyFactory;
import org.chromium.android_webview.AwContents.InternalAccessDelegate;
import org.chromium.android_webview.AwContents.NativeDrawFunctorFactory;
import org.chromium.android_webview.AwContentsClient;
import org.chromium.android_webview.AwRenderProcessGoneDetail;
import org.chromium.android_webview.AwSettings;
import org.chromium.android_webview.test.AwActivityTestRule;
import org.chromium.android_webview.test.AwJUnit4ClassRunnerWithParameters;
import org.chromium.android_webview.test.AwParameterizedTest;
import org.chromium.android_webview.test.AwSettingsMutation;
import org.chromium.android_webview.test.AwTestContainerView;
import org.chromium.android_webview.test.OnlyRunIn;
import org.chromium.android_webview.test.RenderProcessGoneHelper;
import org.chromium.android_webview.test.TestAwContents;
import org.chromium.android_webview.test.TestAwContentsClient;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.Feature;

/**
 * Test VisualStateCallback when render process is gone. Test is not batched because it tests
 * behaviour in multiprocesses.
 */
@RunWith(Parameterized.class)
@UseParametersRunnerFactory(AwJUnit4ClassRunnerWithParameters.Factory.class)
public class VisualStateCallbackTest extends AwParameterizedTest {
    @Rule public AwActivityTestRule mActivityTestRule;

    private static class VisualStateCallbackHelper extends CallbackHelper {
        // Indicates VisualStateCallback has been received by AwContents, but
        // not forwarded to app's callback class.
        private boolean mVisualStateCallbackArrived;

        public void onVisualStateCallbackArrived() {
            mVisualStateCallbackArrived = true;
            notifyCalled();
        }

        public boolean visualStateCallbackArrived() {
            return mVisualStateCallbackArrived;
        }
    }

    private static class RenderProcessGoneTestAwContentsClient extends TestAwContentsClient {
        @Override
        public boolean onRenderProcessGone(AwRenderProcessGoneDetail detail) {
            return true;
        }
    }

    private static class VisualStateCallbackTestAwContents extends TestAwContents {
        private VisualStateCallbackHelper mVisualStateCallbackHelper;

        private VisualStateCallback mCallback;
        private long mRequestId;

        public VisualStateCallbackTestAwContents(
                AwBrowserContext browserContext,
                ViewGroup containerView,
                Context context,
                InternalAccessDelegate internalAccessAdapter,
                NativeDrawFunctorFactory nativeDrawFunctorFactory,
                AwContentsClient contentsClient,
                AwSettings settings,
                DependencyFactory dependencyFactory) {
            super(
                    browserContext,
                    containerView,
                    context,
                    internalAccessAdapter,
                    nativeDrawFunctorFactory,
                    contentsClient,
                    settings,
                    dependencyFactory);
            mVisualStateCallbackHelper = new VisualStateCallbackHelper();
        }

        public VisualStateCallbackHelper getVisualStateCallbackHelper() {
            return mVisualStateCallbackHelper;
        }

        @Override
        public void invokeVisualStateCallback(
                final VisualStateCallback callback, final long requestId) {
            mCallback = callback;
            mRequestId = requestId;
            mVisualStateCallbackHelper.onVisualStateCallbackArrived();
        }

        public void doInvokeVisualStateCallbackOnUiThread() {
            final VisualStateCallbackTestAwContents awContents = this;
            PostTask.runOrPostTask(
                    TaskTraits.UI_DEFAULT, () -> awContents.doInvokeVisualStateCallback());
        }

        private void doInvokeVisualStateCallback() {
            super.invokeVisualStateCallback(mCallback, mRequestId);
        }
    }

    private static class CrashTestDependencyFactory
            extends AwActivityTestRule.TestDependencyFactory {
        @Override
        public AwContents createAwContents(
                AwBrowserContext browserContext,
                ViewGroup containerView,
                Context context,
                InternalAccessDelegate internalAccessAdapter,
                NativeDrawFunctorFactory nativeDrawFunctorFactory,
                AwContentsClient contentsClient,
                AwSettings settings,
                DependencyFactory dependencyFactory) {
            return new VisualStateCallbackTestAwContents(
                    browserContext,
                    containerView,
                    context,
                    internalAccessAdapter,
                    nativeDrawFunctorFactory,
                    contentsClient,
                    settings,
                    dependencyFactory);
        }
    }

    private static class VisualStateCallbackImpl extends AwContents.VisualStateCallback {
        private int mRequestId;
        private boolean mCalled;

        @Override
        public void onComplete(long requestId) {
            mCalled = true;
        }

        public int requestId() {
            return mRequestId;
        }

        public boolean called() {
            return mCalled;
        }
    }

    private VisualStateCallbackTestAwContents mAwContents;
    private RenderProcessGoneHelper mHelper;

    public VisualStateCallbackTest(AwSettingsMutation param) {
        this.mActivityTestRule = new AwActivityTestRule(param.getMutation());
    }

    @Before
    public void setUp() {
        RenderProcessGoneTestAwContentsClient contentsClient =
                new RenderProcessGoneTestAwContentsClient();
        AwTestContainerView testView =
                mActivityTestRule.createAwTestContainerViewOnMainSync(
                        contentsClient, false, new CrashTestDependencyFactory());
        mAwContents = (VisualStateCallbackTestAwContents) testView.getAwContents();
        mHelper = mAwContents.getRenderProcessGoneHelper();
    }

    // Tests the callback isn't invoked if insertVisualStateCallback() is called after render
    // process gone, but before the AwContentsClient knows about it.
    @Test
    @Feature({"AndroidWebView"})
    @SmallTest
    @OnlyRunIn(MULTI_PROCESS)
    public void testAddVisualStateCallbackAfterRendererGone() throws Throwable {
        final VisualStateCallbackImpl vsImpl = new VisualStateCallbackImpl();
        mHelper.setOnRenderProcessGoneTask(
                () -> mAwContents.insertVisualStateCallback(vsImpl.requestId(), vsImpl));
        mActivityTestRule.loadUrlAsync(mAwContents, "chrome://kill");

        mHelper.waitForRenderProcessGoneNotifiedToAwContentsClient();

        mActivityTestRule.destroyAwContentsOnMainSync(mAwContents);

        mHelper.waitForAwContentsDestroyed();
        Assert.assertFalse(vsImpl.called());
    }
}

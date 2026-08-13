// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import static com.google.common.truth.Truth.assertThat;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import androidx.test.filters.SmallTest;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.Parameterized;
import org.junit.runners.Parameterized.UseParametersRunnerFactory;

import org.chromium.android_webview.AppState;
import org.chromium.android_webview.AwContentsLifecycleNotifier;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Feature;

import java.util.ArrayList;
import java.util.List;

/** AwContentsLifecycleNotifier tests. */
@RunWith(Parameterized.class)
@UseParametersRunnerFactory(AwJUnit4ClassRunnerWithParameters.Factory.class)
public class AwContentsLifecycleNotifierTest extends AwParameterizedTest {
    @Rule public AwActivityTestRule mActivityTestRule;

    private final TestAwContentsClient mContentsClient = new TestAwContentsClient();

    private static class LifecycleObserver implements AwContentsLifecycleNotifier.Observer {
        public final CallbackHelper mFirstWebViewCreatedCallback = new CallbackHelper();
        public final CallbackHelper mLastWebViewDestroyedCallback = new CallbackHelper();
        public final List<Integer> mAppStatesSeen = new ArrayList<Integer>();

        @Override
        public void onFirstWebViewCreated() {
            mFirstWebViewCreatedCallback.notifyCalled();
        }

        @Override
        public void onLastWebViewDestroyed() {
            mLastWebViewDestroyedCallback.notifyCalled();
        }

        @Override
        public void onAppStateChanged(@AppState int appState) {
            mAppStatesSeen.add(appState);
        }
    }

    public AwContentsLifecycleNotifierTest(AwSettingsMutation param) {
        this.mActivityTestRule = new AwActivityTestRule(param.getMutation());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testNotifierCreate() throws Throwable {
        LifecycleObserver observer = new LifecycleObserver();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    AwContentsLifecycleNotifier.getInstance().addObserver(observer);
                    assertFalse(AwContentsLifecycleNotifier.getInstance().hasWebViewInstances());
                });

        AwTestContainerView awTestContainerView =
                mActivityTestRule.createAwTestContainerViewOnMainSync(mContentsClient);
        observer.mFirstWebViewCreatedCallback.waitForCallback(0, 1);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertTrue(AwContentsLifecycleNotifier.getInstance().hasWebViewInstances());
                    mActivityTestRule.getActivity().removeAllViews();
                });
        mActivityTestRule.destroyAwContentsOnMainSync(awTestContainerView.getAwContents());
        observer.mLastWebViewDestroyedCallback.waitForCallback(0, 1);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertFalse(AwContentsLifecycleNotifier.getInstance().hasWebViewInstances());
                });
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testAppState() throws Throwable {
        LifecycleObserver observer = new LifecycleObserver();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    AwContentsLifecycleNotifier.getInstance().addObserver(observer);
                    assertFalse(AwContentsLifecycleNotifier.getInstance().hasWebViewInstances());
                    assertEquals(
                            AppState.DESTROYED,
                            AwContentsLifecycleNotifier.getInstance().getAppState());
                });

        AwTestContainerView awTestContainerView =
                mActivityTestRule.createAwTestContainerViewOnMainSync(mContentsClient);

        CriteriaHelper.pollUiThread(
                () -> {
                    return AwContentsLifecycleNotifier.getInstance().getAppState()
                            == AppState.FOREGROUND;
                });

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertEquals(
                            observer.mAppStatesSeen.get(observer.mAppStatesSeen.size() - 1),
                            Integer.valueOf(AppState.FOREGROUND));
                    observer.mAppStatesSeen.clear();

                    mActivityTestRule.getActivity().removeAllViews();
                });
        mActivityTestRule.destroyAwContentsOnMainSync(awTestContainerView.getAwContents());

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertFalse(AwContentsLifecycleNotifier.getInstance().hasWebViewInstances());
                    assertEquals(
                            AppState.DESTROYED,
                            AwContentsLifecycleNotifier.getInstance().getAppState());
                });
        assertThat(observer.mAppStatesSeen)
                .containsAtLeast(AppState.BACKGROUND, AppState.DESTROYED)
                .inOrder();
    }
}

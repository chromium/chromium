// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.firstrun;

import static org.robolectric.Shadows.shadowOf;

import android.content.Context;
import android.os.Bundle;
import android.os.UserManager;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.LooperMode;
import org.robolectric.shadows.ShadowUserManager;

import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.task.TaskTraits;
import org.chromium.base.task.test.ShadowPostTask;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.PayloadCallbackHelper;
import org.chromium.components.policy.PolicySwitches;

/** Unit test for {@link FirstRunAppRestrictionInfo}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(
        manifest = Config.NONE,
        shadows = {ShadowPostTask.class, ShadowUserManager.class})
@LooperMode(LooperMode.Mode.LEGACY)
public class FirstRunAppRestrictionInfoTest {
    @Mock private Bundle mMockBundle;

    private boolean mPauseDuringPostTask;
    private Runnable mPendingPostTask;

    @Before
    public void setup() {
        MockitoAnnotations.initMocks(this);
        ShadowPostTask.setTestImpl(
                new ShadowPostTask.TestImpl() {
                    @Override
                    public void postDelayedTask(
                            @TaskTraits int taskTraits, Runnable task, long delay) {
                        if (!mPauseDuringPostTask) {
                            task.run();
                        } else {
                            mPendingPostTask = task;
                        }
                    }
                });

        Context context = ContextUtils.getApplicationContext();
        UserManager userManager = (UserManager) context.getSystemService(Context.USER_SERVICE);
        ShadowUserManager shadowUserManager = shadowOf(userManager);
        shadowUserManager.setApplicationRestrictions(context.getPackageName(), mMockBundle);
    }

    @Test
    @SmallTest
    public void testInitWithRestriction() {
        testInitImpl(true);
    }

    @Test
    @SmallTest
    public void testInitWithoutRestriction() {
        testInitImpl(false);
    }

    private void testInitImpl(boolean withRestriction) {
        Mockito.when(mMockBundle.isEmpty()).thenReturn(!withRestriction);
        final PayloadCallbackHelper<Boolean> appResCallbackHelper = new PayloadCallbackHelper<>();
        final CallbackHelper completionCallbackHelper = new CallbackHelper();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    FirstRunAppRestrictionInfo info =
                            FirstRunAppRestrictionInfo.takeMaybeInitialized();
                    info.getHasAppRestriction(appResCallbackHelper::notifyCalled);
                    info.getCompletionElapsedRealtimeMs(
                            (ignored) -> completionCallbackHelper.notifyCalled());
                });

        Assert.assertEquals(withRestriction, appResCallbackHelper.getOnlyPayloadBlocking());
        Assert.assertEquals(1, completionCallbackHelper.getCallCount());
    }

    @Test
    @SmallTest
    public void testQueuedCallback() {
        Mockito.when(mMockBundle.isEmpty()).thenReturn(false);

        final PayloadCallbackHelper<Boolean> appResCallbackHelper1 = new PayloadCallbackHelper<>();
        final PayloadCallbackHelper<Boolean> appResCallbackHelper2 = new PayloadCallbackHelper<>();
        final PayloadCallbackHelper<Boolean> appResCallbackHelper3 = new PayloadCallbackHelper<>();
        final CallbackHelper completionCallbackHelper1 = new CallbackHelper();
        final CallbackHelper completionCallbackHelper2 = new CallbackHelper();
        final CallbackHelper completionCallbackHelper3 = new CallbackHelper();

        mPauseDuringPostTask = true;
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    FirstRunAppRestrictionInfo info =
                            FirstRunAppRestrictionInfo.takeMaybeInitialized();
                    info.getHasAppRestriction(appResCallbackHelper1::notifyCalled);
                    info.getHasAppRestriction(appResCallbackHelper2::notifyCalled);
                    info.getHasAppRestriction(appResCallbackHelper3::notifyCalled);
                    info.getCompletionElapsedRealtimeMs(
                            (ignored) -> completionCallbackHelper1.notifyCalled());
                    info.getCompletionElapsedRealtimeMs(
                            (ignored) -> completionCallbackHelper2.notifyCalled());
                    info.getCompletionElapsedRealtimeMs(
                            (ignored) -> completionCallbackHelper3.notifyCalled());
                });

        Assert.assertEquals(
                "CallbackHelper should not triggered yet.",
                0,
                appResCallbackHelper1.getCallCount());
        Assert.assertEquals(
                "CallbackHelper should not triggered yet.",
                0,
                appResCallbackHelper2.getCallCount());
        Assert.assertEquals(
                "CallbackHelper should not triggered yet.",
                0,
                appResCallbackHelper3.getCallCount());
        Assert.assertEquals(
                "CallbackHelper should not triggered yet.",
                0,
                completionCallbackHelper1.getCallCount());
        Assert.assertEquals(
                "CallbackHelper should not triggered yet.",
                0,
                completionCallbackHelper2.getCallCount());
        Assert.assertEquals(
                "CallbackHelper should not triggered yet.",
                0,
                completionCallbackHelper3.getCallCount());

        mPauseDuringPostTask = false;
        // Initialized the AppRestrictionInfo and wait until initialized.
        ThreadUtils.runOnUiThreadBlocking(() -> mPendingPostTask.run());

        Assert.assertTrue(appResCallbackHelper1.getOnlyPayloadBlocking());
        Assert.assertTrue(appResCallbackHelper2.getOnlyPayloadBlocking());
        Assert.assertTrue(appResCallbackHelper3.getOnlyPayloadBlocking());
        Assert.assertEquals(1, completionCallbackHelper1.getCallCount());
        Assert.assertEquals(1, completionCallbackHelper2.getCallCount());
        Assert.assertEquals(1, completionCallbackHelper3.getCallCount());
    }

    @Test
    @SmallTest
    public void testDestroy() {
        final PayloadCallbackHelper<Boolean> appResCallbackHelper = new PayloadCallbackHelper<>();
        final CallbackHelper completionCallbackHelper = new CallbackHelper();
        mPauseDuringPostTask = true;

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    FirstRunAppRestrictionInfo info =
                            FirstRunAppRestrictionInfo.takeMaybeInitialized();
                    info.getHasAppRestriction(appResCallbackHelper::notifyCalled);
                    info.getCompletionElapsedRealtimeMs(
                            (ignored) -> completionCallbackHelper.notifyCalled());

                    // Destroy the object before the async task completes.
                    info.destroy();

                    mPendingPostTask.run();
                });

        Assert.assertEquals(
                "CallbackHelper should not triggered yet.", 0, appResCallbackHelper.getCallCount());
        Assert.assertEquals(
                "CallbackHelper should not triggered yet.",
                0,
                completionCallbackHelper.getCallCount());
    }

    @Test
    @SmallTest
    @CommandLineFlags.Add({PolicySwitches.CHROME_POLICY})
    public void testCommandLine() {
        final PayloadCallbackHelper<Boolean> appResCallbackHelper = new PayloadCallbackHelper<>();
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        FirstRunAppRestrictionInfo.takeMaybeInitialized()
                                .getHasAppRestriction(appResCallbackHelper::notifyCalled));
        Assert.assertTrue(appResCallbackHelper.getOnlyPayloadBlocking());
    }
}

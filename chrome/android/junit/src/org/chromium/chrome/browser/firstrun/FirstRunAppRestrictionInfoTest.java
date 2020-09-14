// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.firstrun;

import static org.robolectric.Shadows.shadowOf;

import android.content.Context;
import android.os.Bundle;
import android.os.UserManager;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.CommandLine;
import org.chromium.base.ContextUtils;
import org.chromium.base.metrics.test.ShadowRecordHistogram;
import org.chromium.base.task.TaskTraits;
import org.chromium.base.task.test.ShadowPostTask;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.components.policy.PolicySwitches;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.testing.local.CustomShadowUserManager;

import java.util.Arrays;
import java.util.List;
import java.util.concurrent.TimeoutException;

/**
 * Unit test for {@link FirstRunAppRestrictionInfo}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE,
        shadows = {ShadowRecordHistogram.class, ShadowPostTask.class,
                CustomShadowUserManager.class})
public class FirstRunAppRestrictionInfoTest {
    private static final List<String> HISTOGRAM_NAMES =
            Arrays.asList("Enterprise.FirstRun.AppRestrictionLoadTime",
                    "Enterprise.FirstRun.AppRestrictionLoadTime.Medium");

    private static class BooleanInputCallbackHelper extends CallbackHelper {
        boolean mLastInput;

        void notifyCalledWithInput(boolean input) {
            mLastInput = input;
            notifyCalled();
        }

        void assertCallbackHelperCalledWithInput(boolean expected) throws TimeoutException {
            waitForFirst();
            Assert.assertEquals("Callback helper should be called once.", 1, getCallCount());
            Assert.assertEquals(expected, mLastInput);
        }
    }

    @Mock
    private Bundle mMockBundle;
    @Mock
    private CommandLine mCommandLine;

    private boolean mPauseDuringPostTask;
    private Runnable mPendingPostTask;

    @Before
    public void setup() {
        MockitoAnnotations.initMocks(this);
        ShadowRecordHistogram.reset();
        ShadowPostTask.setTestImpl(new ShadowPostTask.TestImpl() {
            @Override
            public void postDelayedTask(TaskTraits taskTraits, Runnable task, long delay) {
                if (!mPauseDuringPostTask) {
                    task.run();
                } else {
                    mPendingPostTask = task;
                }
            }
        });

        Context context = ContextUtils.getApplicationContext();
        UserManager userManager = (UserManager) context.getSystemService(Context.USER_SERVICE);
        CustomShadowUserManager shadowUserManager = (CustomShadowUserManager) shadowOf(userManager);
        shadowUserManager.setApplicationRestrictions(context.getPackageName(), mMockBundle);
    }

    @After
    public void tearDown() {
        FirstRunAppRestrictionInfo.setInitializedInstanceForTest(null);
        CommandLine.reset();
    }

    private void verifyHistograms(int expectedCallCount) {
        for (String name : HISTOGRAM_NAMES) {
            Assert.assertEquals("Histogram record count doesn't match.", expectedCallCount,
                    ShadowRecordHistogram.getHistogramTotalCountForTesting(name));
        }
    }

    @Test
    @SmallTest
    public void testInitWithRestriction() throws TimeoutException {
        testInitImpl(true);
    }

    @Test
    @SmallTest
    public void testInitWithoutRestriction() throws TimeoutException {
        testInitImpl(false);
    }

    private void testInitImpl(boolean withRestriction) throws TimeoutException {
        Mockito.when(mMockBundle.isEmpty()).thenReturn(!withRestriction);
        final BooleanInputCallbackHelper callbackHelper = new BooleanInputCallbackHelper();
        final CallbackHelper completionCallbackHelper = new CallbackHelper();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            FirstRunAppRestrictionInfo info = FirstRunAppRestrictionInfo.takeMaybeInitialized();
            info.getHasAppRestriction(callbackHelper::notifyCalledWithInput);
            info.getCompletionElapsedRealtimeMs(
                    (ignored) -> completionCallbackHelper.notifyCalled());
        });

        callbackHelper.assertCallbackHelperCalledWithInput(withRestriction);
        Assert.assertEquals(1, completionCallbackHelper.getCallCount());
        verifyHistograms(1);
    }

    @Test
    @SmallTest
    public void testQueuedCallback() throws TimeoutException {
        Mockito.when(mMockBundle.isEmpty()).thenReturn(false);

        final BooleanInputCallbackHelper callbackHelper1 = new BooleanInputCallbackHelper();
        final BooleanInputCallbackHelper callbackHelper2 = new BooleanInputCallbackHelper();
        final BooleanInputCallbackHelper callbackHelper3 = new BooleanInputCallbackHelper();
        final CallbackHelper completionCallbackHelper1 = new CallbackHelper();
        final CallbackHelper completionCallbackHelper2 = new CallbackHelper();
        final CallbackHelper completionCallbackHelper3 = new CallbackHelper();

        mPauseDuringPostTask = true;
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            FirstRunAppRestrictionInfo info = FirstRunAppRestrictionInfo.takeMaybeInitialized();
            info.getHasAppRestriction(callbackHelper1::notifyCalledWithInput);
            info.getHasAppRestriction(callbackHelper2::notifyCalledWithInput);
            info.getHasAppRestriction(callbackHelper3::notifyCalledWithInput);
            info.getCompletionElapsedRealtimeMs(
                    (ignored) -> completionCallbackHelper1.notifyCalled());
            info.getCompletionElapsedRealtimeMs(
                    (ignored) -> completionCallbackHelper2.notifyCalled());
            info.getCompletionElapsedRealtimeMs(
                    (ignored) -> completionCallbackHelper3.notifyCalled());
        });

        Assert.assertEquals(
                "CallbackHelper should not triggered yet.", 0, callbackHelper1.getCallCount());
        Assert.assertEquals(
                "CallbackHelper should not triggered yet.", 0, callbackHelper2.getCallCount());
        Assert.assertEquals(
                "CallbackHelper should not triggered yet.", 0, callbackHelper3.getCallCount());
        Assert.assertEquals("CallbackHelper should not triggered yet.", 0,
                completionCallbackHelper1.getCallCount());
        Assert.assertEquals("CallbackHelper should not triggered yet.", 0,
                completionCallbackHelper2.getCallCount());
        Assert.assertEquals("CallbackHelper should not triggered yet.", 0,
                completionCallbackHelper3.getCallCount());

        // Initialized the AppRestrictionInfo and wait until initialized.
        TestThreadUtils.runOnUiThreadBlocking(() -> mPendingPostTask.run());

        callbackHelper1.assertCallbackHelperCalledWithInput(true);
        callbackHelper2.assertCallbackHelperCalledWithInput(true);
        callbackHelper3.assertCallbackHelperCalledWithInput(true);
        Assert.assertEquals(1, completionCallbackHelper1.getCallCount());
        Assert.assertEquals(1, completionCallbackHelper2.getCallCount());
        Assert.assertEquals(1, completionCallbackHelper3.getCallCount());

        verifyHistograms(1);
    }

    @Test
    @SmallTest
    public void testDestroy() {
        final BooleanInputCallbackHelper callbackHelper = new BooleanInputCallbackHelper();
        final CallbackHelper completionCallbackHelper = new CallbackHelper();
        mPauseDuringPostTask = true;

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            FirstRunAppRestrictionInfo info = FirstRunAppRestrictionInfo.takeMaybeInitialized();
            info.getHasAppRestriction(callbackHelper::notifyCalledWithInput);
            info.getCompletionElapsedRealtimeMs(
                    (ignored) -> completionCallbackHelper.notifyCalled());

            // Destroy the object before the async task completes.
            info.destroy();

            mPendingPostTask.run();
        });

        Assert.assertEquals(
                "CallbackHelper should not triggered yet.", 0, callbackHelper.getCallCount());
        Assert.assertEquals("CallbackHelper should not triggered yet.", 0,
                completionCallbackHelper.getCallCount());
        verifyHistograms(0);
    }

    @Test
    @SmallTest
    public void testCommandLine() throws TimeoutException {
        // TODO(https://crbug.com/1119410): Switch to @CommandLineFlag once supported for junit.
        CommandLine.setInstanceForTesting(mCommandLine);
        Mockito.when(mCommandLine.hasSwitch(Mockito.eq(PolicySwitches.CHROME_POLICY)))
                .thenReturn(true);

        final BooleanInputCallbackHelper callbackHelper = new BooleanInputCallbackHelper();
        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> FirstRunAppRestrictionInfo.takeMaybeInitialized().getHasAppRestriction(
                                callbackHelper::notifyCalledWithInput));
        callbackHelper.assertCallbackHelperCalledWithInput(true);

        verifyHistograms(1);
    }
}

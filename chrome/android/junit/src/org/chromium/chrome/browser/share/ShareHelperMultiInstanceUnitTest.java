// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.content.ComponentName;
import android.content.Intent;
import android.content.IntentSender;
import android.content.IntentSender.SendIntentException;

import androidx.annotation.Nullable;
import androidx.lifecycle.Lifecycle.State;
import androidx.test.core.app.ActivityScenario;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Spy;
import org.robolectric.Shadows;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowActivity;
import org.robolectric.shadows.ShadowActivity.IntentForResult;
import org.robolectric.shadows.ShadowLooper;
import org.robolectric.shadows.ShadowPendingIntent;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.components.browser_ui.share.ShareParams;
import org.chromium.components.browser_ui.share.ShareParams.TargetChosenCallback;
import org.chromium.ui.base.ActivityWindowAndroid;
import org.chromium.ui.base.IntentRequestTracker;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.base.WindowAndroid;

/**
 * Robolectric test testing share functionality in multi-instance scenarios. Focusing testing
 * whether related broadcast receivers are registered / unregistered correctly in multi-instance
 * mode.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(shadows = {ShadowPendingIntent.class, ShadowActivity.class})
public class ShareHelperMultiInstanceUnitTest {
    private static final ComponentName COMPONENT_NAME_1 = new ComponentName("package", "one");
    private static final ComponentName COMPONENT_NAME_2 = new ComponentName("package", "two");

    private SingleWindowTestInstance mWindowFoo;
    private SingleWindowTestInstance mWindowBar;

    @Before
    public void setup() {
        mWindowFoo = new SingleWindowTestInstance(1);
        mWindowBar = new SingleWindowTestInstance(2);
    }

    @After
    public void tearDown() {
        mWindowBar.closeWindow();
        mWindowFoo.closeWindow();
        ChromeSharedPreferences.getInstance()
                .removeKey(ChromePreferenceKeys.SHARING_LAST_SHARED_COMPONENT_NAME);
    }

    @Test
    public void shareInTwoWindow_FinishInOrder() throws SendIntentException {
        mWindowFoo.startShare().verifyCallbackNotCalled();
        mWindowBar.startShare().verifyCallbackNotCalled();
        mWindowFoo.completeShareWithComponent(COMPONENT_NAME_1).verifyCallbackState();
        mWindowBar
                .verifyCallbackNotCalled()
                .completeShareWithComponent(COMPONENT_NAME_2)
                .verifyCallbackState();
        assertLastComponentRecorded(COMPONENT_NAME_2);
    }

    @Test
    public void shareInTwoWindow_FinishInReverseOrder() throws SendIntentException {
        mWindowFoo.startShare();
        mWindowBar
                .startShare()
                .verifyCallbackNotCalled()
                .completeShareWithComponent(COMPONENT_NAME_2)
                .verifyCallbackState();
        mWindowFoo
                .verifyCallbackNotCalled()
                .completeShareWithComponent(COMPONENT_NAME_1)
                .verifyCallbackState();
        assertLastComponentRecorded(COMPONENT_NAME_1);
    }

    @Test
    public void shareInTwoWindow_FinishFirstThenCancelSecond() throws SendIntentException {
        mWindowFoo.startShare();
        mWindowBar.startShare();
        mWindowFoo.completeShareWithComponent(COMPONENT_NAME_1).verifyCallbackState();
        mWindowBar.cancelShare().verifyCallbackState();
        assertLastComponentRecorded(COMPONENT_NAME_1);
    }

    @Test
    public void shareInTwoWindow_FinishSecondThenCancelFirst() throws SendIntentException {
        mWindowFoo.startShare();
        mWindowBar.startShare().completeShareWithComponent(COMPONENT_NAME_2).verifyCallbackState();
        mWindowFoo.cancelShare().verifyCallbackState();

        assertLastComponentRecorded(COMPONENT_NAME_2);
    }

    @Test
    public void shareInTwoWindow_CancelFirstFinishSecond() throws SendIntentException {
        mWindowFoo.startShare();
        mWindowBar.startShare();
        mWindowFoo.cancelShare().verifyCallbackState();
        mWindowBar.completeShareWithComponent(COMPONENT_NAME_2).verifyCallbackState();

        assertLastComponentRecorded(COMPONENT_NAME_2);
    }

    @Test
    public void shareInTwoWindow_KillFirstWindowThenCompleteSecond() throws SendIntentException {
        mWindowFoo.startShare();
        mWindowBar.startShare();
        mWindowFoo.closeWindow().verifyCleanerIntentDispatched();
        mWindowBar
                .verifyCallbackNotCalled()
                .completeShareWithComponent(COMPONENT_NAME_2)
                .verifyCallbackState()
                .closeWindow();
        assertLastComponentRecorded(COMPONENT_NAME_2);
    }

    @Test
    public void shareInTwoWindow_KillSecondWindowThenCompleteFirst() throws SendIntentException {
        mWindowFoo.startShare();
        mWindowBar.startShare().closeWindow().verifyCleanerIntentDispatched();
        mWindowFoo
                .verifyCallbackNotCalled()
                .completeShareWithComponent(COMPONENT_NAME_1)
                .verifyCallbackState()
                .closeWindow();
        assertLastComponentRecorded(COMPONENT_NAME_1);
    }

    private void assertLastComponentRecorded(ComponentName expected) {
        assertEquals(
                "Last saved component name is different.",
                expected,
                ShareHelper.getLastShareComponentName());
    }

    private static class TestTargetChosenCallback implements TargetChosenCallback {
        public boolean onTargetChosenCalled;
        public boolean onCancelCalled;

        @Override
        public void onTargetChosen(ComponentName chosenComponent) {
            onTargetChosenCalled = true;
        }

        @Override
        public void onCancel() {
            onCancelCalled = true;
        }

        public boolean isValid() {
            return (onTargetChosenCalled || onCancelCalled)
                    && (onTargetChosenCalled ^ onCancelCalled);
        }
    }

    /** Class that simulate the share journey. */
    private static class SingleWindowTestInstance {
        private final ActivityScenario<TestActivity> mActivityScenario;
        private final WindowAndroid mWindow;
        private final IntentRequestTracker mIntentRequestTracker;
        private final TestTargetChosenCallback mCallback = new TestTargetChosenCallback();

        @Spy private TestActivity mActivity;
        @Nullable private IntentForResult mShareIntent;
        private boolean mClosed;

        public SingleWindowTestInstance(int taskId) {
            mActivityScenario =
                    ActivityScenario.launch(TestActivity.class)
                            .onActivity(activity -> mActivity = spy(activity))
                            .moveToState(State.STARTED);
            doReturn(taskId).when(mActivity).getTaskId();
            mIntentRequestTracker = IntentRequestTracker.createFromActivity(mActivity);
            mWindow = new ActivityWindowAndroid(mActivity, false, mIntentRequestTracker);
        }

        public SingleWindowTestInstance startShare() {
            ShareHelper.shareWithSystemShareSheetUi(getTextParams(), null, true);
            ShadowLooper.idleMainLooper();

            mShareIntent = Shadows.shadowOf(mActivity).peekNextStartedActivityForResult();
            assertNotNull("Share activity is not launched.", mShareIntent);
            return this;
        }

        public SingleWindowTestInstance completeShareWithComponent(ComponentName componentName)
                throws SendIntentException {
            assert mShareIntent != null;
            Intent sendBackIntent =
                    new Intent().putExtra(Intent.EXTRA_CHOSEN_COMPONENT, componentName);
            IntentSender sender =
                    mShareIntent.intent.getParcelableExtra(
                            Intent.EXTRA_CHOSEN_COMPONENT_INTENT_SENDER);
            sender.sendIntent(
                    mActivity.getApplicationContext(),
                    Activity.RESULT_OK,
                    sendBackIntent,
                    null,
                    null);
            ShadowLooper.idleMainLooper();
            return this;
        }

        public SingleWindowTestInstance cancelShare() throws SendIntentException {
            assert mShareIntent != null;

            mIntentRequestTracker.onActivityResult(
                    mShareIntent.requestCode, Activity.RESULT_CANCELED, null);
            ShadowLooper.idleMainLooper();
            return this;
        }

        public SingleWindowTestInstance verifyCallbackNotCalled() {
            assertFalse(
                    "Callback should not be called.",
                    mCallback.onTargetChosenCalled || mCallback.onCancelCalled);
            return this;
        }

        public SingleWindowTestInstance verifyCallbackState() {
            assertTrue("Callback is not in a valid state when share ends.", mCallback.isValid());
            verify(mActivity).unregisterReceiver(any());
            return this;
        }

        public SingleWindowTestInstance verifyCleanerIntentDispatched() {
            Intent intent = Shadows.shadowOf(mActivity).peekNextStartedActivity();
            assertNotNull("Cleaner intent is not sent.", intent);
            assertEquals(
                    "Cleaner intent does not have the right class name.",
                    intent.getComponent().getClassName(),
                    mActivity.getClass().getName());
            assertTrue(
                    "FLAG_ACTIVITY_CLEAR_TOP is not set for cleaner intent.",
                    (intent.getFlags() & Intent.FLAG_ACTIVITY_CLEAR_TOP) != 0);
            return this;
        }

        public SingleWindowTestInstance closeWindow() {
            if (mClosed) return this;

            mClosed = true;
            mWindow.destroy();
            mActivity.finish();
            mActivityScenario.close();

            return this;
        }

        private ShareParams getTextParams() {
            return new ShareParams.Builder(mWindow, "title", "")
                    .setText("text")
                    .setCallback(mCallback)
                    .setBypassFixingDomDistillerUrl(true)
                    .build();
        }
    }
}

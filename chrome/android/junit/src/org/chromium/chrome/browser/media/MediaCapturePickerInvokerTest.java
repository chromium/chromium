// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNotSame;
import static org.junit.Assert.assertNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.robolectric.Shadows.shadowOf;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;

import androidx.activity.result.ActivityResult;
import androidx.fragment.app.FragmentActivity;
import androidx.lifecycle.Lifecycle;
import androidx.test.core.app.ActivityScenario;
import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.shadows.ShadowActivity.IntentForResult;

import org.chromium.base.CallbackUtils;
import org.chromium.base.ServiceLoaderUtil;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.media.MediaCapturePickerHeadlessFragment.CaptureAction;
import org.chromium.chrome.browser.media.MediaCapturePickerManager.Delegate;
import org.chromium.chrome.browser.media.MediaCapturePickerManager.Params;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.media.capture.ScreenCapture;
import org.chromium.ui.base.TestActivity;

/** Tests for MediaCapturePickerInvoker. */
@RunWith(BaseRobolectricTestRunner.class)
public class MediaCapturePickerInvokerTest {
    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Delegate mDelegate;
    @Mock private WebContents mTabWebContents;

    private Activity mActivity;
    private FakeMediaCapturePickerDelegate mPickerDelegate;

    private static class FakeMediaCapturePickerDelegate implements MediaCapturePickerDelegate {
        private Intent mIntent;
        private Tab mTab;
        private boolean mShouldShareAudio;
        private WebContents mStartedWebContents;
        private WebContents mStoppedWebContents;
        private WebContents mFinishedWebContents;

        @Override
        public Intent createScreenCaptureIntent(Context context, Params params, Delegate delegate) {
            return mIntent;
        }

        @Override
        public Tab getPickedTab() {
            return mTab;
        }

        @Override
        public boolean shouldShareAudio() {
            return mShouldShareAudio;
        }

        @Override
        public void startAppContentMediaProjection(WebContents webContents, ActivityResult result) {
            mStartedWebContents = webContents;
            // Mirror the real implementation: starting the projection consumes the pending
            // selection, after which getPickedTab()/shouldShareAudio() are no longer valid.
            mTab = null;
            mShouldShareAudio = false;
        }

        @Override
        public void stopAppContentMediaProjection(WebContents webContents) {
            mStoppedWebContents = webContents;
        }

        @Override
        public void onFinish(WebContents webContents) {
            mFinishedWebContents = webContents;
        }

        private WebContents getStartedWebContents() {
            return mStartedWebContents;
        }

        private WebContents getStoppedWebContents() {
            return mStoppedWebContents;
        }

        private WebContents getFinishedWebContents() {
            return mFinishedWebContents;
        }

        private void setIntent(Intent intent) {
            mIntent = intent;
        }

        private void setPickedTab(Tab tab) {
            mTab = tab;
        }

        public void setShouldShareAudio(boolean shouldShareAudio) {
            mShouldShareAudio = shouldShareAudio;
        }
    }

    private static MediaCapturePickerManager.Params mediaCaptureParams() {
        return new MediaCapturePickerManager.Params(
                mock(WebContents.class),
                "",
                "",
                /* requestAudio= */ false,
                /* excludeSystemAudio= */ false,
                /* windowAudioPreference= */ 0,
                /* preferredDisplaySurface= */ 0,
                true,
                /* excludeSelfBrowserSurface= */ false,
                /* excludeMonitorTypeSurfaces= */ false,
                AllowedScreenCaptureLevel.DESKTOP);
    }

    @Before
    public void setUp() {
        mActivityScenarioRule.getScenario().onActivity(activity -> mActivity = activity);
        mPickerDelegate = new FakeMediaCapturePickerDelegate();
        ServiceLoaderUtil.setInstanceForTesting(MediaCapturePickerDelegate.class, mPickerDelegate);
        MediaCapturePickerManager.setBringTabToFrontCallbackForTesting(
                CallbackUtils.emptyCallback());
    }

    @After
    public void tearDown() {
        ScreenCapture.resetStaticStateForTesting();
        MediaCapturePickerHeadlessFragment.sNextDelegates.clear();
    }

    /** The delegate of the prompt currently showing in {@link #mActivity}'s window. */
    private MediaCapturePickerHeadlessFragment.Delegate pendingDelegate() {
        return MediaCapturePickerHeadlessFragment.sNextDelegates.get(mActivity.getTaskId());
    }

    @Test
    public void testShow_cancel() {
        mPickerDelegate.setIntent(new Intent());
        MediaCapturePickerInvoker.show(mActivity, mediaCaptureParams(), mDelegate);
        pendingDelegate()
                .onPicked(
                        CaptureAction.CAPTURE_CANCELLED,
                        new ActivityResult(Activity.RESULT_CANCELED, null));
        verify(mDelegate).onCancel();
    }

    @Test
    public void testShow_tabWithAudio() {
        Tab tab = mock(Tab.class);
        doReturn(mTabWebContents).when(tab).getWebContents();

        mPickerDelegate.setIntent(new Intent());
        mPickerDelegate.setPickedTab(tab);
        mPickerDelegate.setShouldShareAudio(true);
        Params params = mediaCaptureParams();
        MediaCapturePickerInvoker.show(mActivity, params, mDelegate);
        pendingDelegate()
                .onPicked(
                        CaptureAction.CAPTURE_WINDOW,
                        new ActivityResult(Activity.RESULT_OK, new Intent()));
        verify(tab).loadIfNeeded(/* forceBackingSize= */ true);
        assertEquals(params.webContents, mPickerDelegate.getStartedWebContents());
        verify(mDelegate).onPickTab(mTabWebContents, true);
    }

    @Test
    public void testShow_tabWithoutAudio() {
        Tab tab = mock(Tab.class);
        doReturn(mTabWebContents).when(tab).getWebContents();

        mPickerDelegate.setIntent(new Intent());
        mPickerDelegate.setPickedTab(tab);
        mPickerDelegate.setShouldShareAudio(false);
        Params params = mediaCaptureParams();
        MediaCapturePickerInvoker.show(mActivity, params, mDelegate);
        pendingDelegate()
                .onPicked(
                        CaptureAction.CAPTURE_WINDOW,
                        new ActivityResult(Activity.RESULT_OK, new Intent()));
        verify(tab).loadIfNeeded(/* forceBackingSize= */ true);
        assertEquals(params.webContents, mPickerDelegate.getStartedWebContents());
        verify(mDelegate).onPickTab(mTabWebContents, false);
    }

    /** A picked tab that this request's filter excludes is not shared. */
    @Test
    public void testShow_pickedTabExcludedByFilter() {
        var resultWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Media.MediaCapture.UI.Android.Picker.Result",
                        MediaCapturePickerManager.Result.CANCELLED);
        Tab tab = mock(Tab.class);
        doReturn(mTabWebContents).when(tab).getWebContents();
        doReturn(true).when(mDelegate).shouldFilterWebContents(mTabWebContents);

        mPickerDelegate.setIntent(new Intent());
        mPickerDelegate.setPickedTab(tab);
        Params params = mediaCaptureParams();
        MediaCapturePickerInvoker.show(mActivity, params, mDelegate);
        pendingDelegate()
                .onPicked(
                        CaptureAction.CAPTURE_WINDOW,
                        new ActivityResult(Activity.RESULT_OK, new Intent()));

        verify(mDelegate).onCancel();
        verify(mDelegate, never()).onPickTab(any(), anyBoolean());
        verify(mDelegate, never()).onPickWindow();
        assertNull(mPickerDelegate.getStartedWebContents());
        assertEquals(params.webContents, mPickerDelegate.getFinishedWebContents());
        resultWatcher.assertExpected();
    }

    /** A picked tab with no WebContents, e.g. closed after it was picked, is not shared. */
    @Test
    public void testShow_pickedTabWithoutWebContents() {
        Tab tab = mock(Tab.class);
        mPickerDelegate.setIntent(new Intent());
        mPickerDelegate.setPickedTab(tab);
        Params params = mediaCaptureParams();
        MediaCapturePickerInvoker.show(mActivity, params, mDelegate);
        pendingDelegate()
                .onPicked(
                        CaptureAction.CAPTURE_WINDOW,
                        new ActivityResult(Activity.RESULT_OK, new Intent()));

        verify(mDelegate).onCancel();
        verify(mDelegate, never()).onPickTab(any(), anyBoolean());
        assertNull(mPickerDelegate.getStartedWebContents());
        assertEquals(params.webContents, mPickerDelegate.getFinishedWebContents());
    }

    @Test
    public void testShow_window() {
        mPickerDelegate.setIntent(new Intent());
        Params params = mediaCaptureParams();
        MediaCapturePickerInvoker.show(mActivity, params, mDelegate);
        pendingDelegate()
                .onPicked(
                        CaptureAction.CAPTURE_WINDOW,
                        new ActivityResult(Activity.RESULT_OK, new Intent()));
        verify(mDelegate).onPickWindow();
        assertEquals(params.webContents, mPickerDelegate.getStoppedWebContents());
    }

    @Test
    public void testShow_screen() {
        mPickerDelegate.setIntent(new Intent());
        Params params = mediaCaptureParams();
        MediaCapturePickerInvoker.show(mActivity, params, mDelegate);
        pendingDelegate()
                .onPicked(
                        CaptureAction.CAPTURE_SCREEN,
                        new ActivityResult(Activity.RESULT_OK, new Intent()));
        verify(mDelegate).onPickScreen();
        assertEquals(params.webContents, mPickerDelegate.getStoppedWebContents());
    }

    /**
     * Regression test: the Activity is recreated (e.g. ChromeActivity#recreate() on a tablet-mode
     * or night-mode change) while the system prompt is showing. The restored fragment is a new
     * instance; the result must still reach the delegate of the request that launched the prompt.
     */
    @Test
    public void testShow_activityRecreatedWhilePromptShowing() {
        mPickerDelegate.setIntent(new Intent());
        Params params = mediaCaptureParams();
        MediaCapturePickerInvoker.show(mActivity, params, mDelegate);
        MediaCapturePickerHeadlessFragment fragmentBefore =
                MediaCapturePickerHeadlessFragment.getInstance((FragmentActivity) mActivity);
        IntentForResult launched = shadowOf(mActivity).getNextStartedActivityForResult();
        assertNotNull(launched);

        ActivityScenario<TestActivity> scenario = mActivityScenarioRule.getScenario();
        scenario.recreate();
        scenario.onActivity(activity -> mActivity = activity);
        assertNotSame(
                fragmentBefore,
                MediaCapturePickerHeadlessFragment.getInstance((FragmentActivity) mActivity));
        verify(mDelegate, never()).onCancel();
        assertNull(mPickerDelegate.getFinishedWebContents());

        // Same path as Activity#onActivityResult -> ActivityResultRegistry#dispatchResult.
        ((FragmentActivity) mActivity)
                .getActivityResultRegistry()
                .dispatchResult(launched.requestCode, Activity.RESULT_OK, new Intent());

        verify(mDelegate).onPickWindow();
        assertEquals(params.webContents, mPickerDelegate.getFinishedWebContents());
    }

    /**
     * Closing the window while its prompt is showing cancels the request and finishes the picker.
     */
    @Test
    public void testShow_windowClosedWhilePromptShowing() {
        var resultWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Media.MediaCapture.UI.Android.Picker.Result",
                        MediaCapturePickerManager.Result.CANCELLED);
        mPickerDelegate.setIntent(new Intent());
        Params params = mediaCaptureParams();
        MediaCapturePickerInvoker.show(mActivity, params, mDelegate);
        assertNotNull(pendingDelegate());

        // finish() then destroy: onDestroy() runs with isFinishing() == true.
        mActivity.finish();
        mActivityScenarioRule.getScenario().moveToState(Lifecycle.State.DESTROYED);

        assertNull(pendingDelegate());
        verify(mDelegate).onCancel();
        assertEquals(params.webContents, mPickerDelegate.getFinishedWebContents());
        resultWatcher.assertExpected();
    }

    /**
     * An Activity destroyed for a reason other than recreation (no relaunch pending) also cancels
     * the request and finishes the picker, even if it is not finishing.
     */
    @Test
    public void testShow_activityDestroyedWhilePromptShowing() {
        mPickerDelegate.setIntent(new Intent());
        Params params = mediaCaptureParams();
        MediaCapturePickerInvoker.show(mActivity, params, mDelegate);
        assertNotNull(pendingDelegate());

        // Destroy without finish() and without recreate(): isChangingConfigurations() is false.
        mActivityScenarioRule.getScenario().moveToState(Lifecycle.State.DESTROYED);

        assertNull(pendingDelegate());
        verify(mDelegate).onCancel();
        assertEquals(params.webContents, mPickerDelegate.getFinishedWebContents());
    }

    /**
     * If the requesting tab closes while the system prompt is open, a positive picker result
     * cancels cleanly and finishes the picker instead of starting a projection with no requester.
     */
    @Test
    public void testShow_requestingWebContentsDestroyedBeforeResult() {
        var resultWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Media.MediaCapture.UI.Android.Picker.Result",
                        MediaCapturePickerManager.Result.CANCELLED);
        Tab tab = mock(Tab.class);
        doReturn(mTabWebContents).when(tab).getWebContents();
        mPickerDelegate.setIntent(new Intent());
        mPickerDelegate.setPickedTab(tab);
        Params params = mediaCaptureParams();
        MediaCapturePickerInvoker.show(mActivity, params, mDelegate);

        doReturn(true).when(params.webContents).isDestroyed();
        pendingDelegate()
                .onPicked(
                        CaptureAction.CAPTURE_WINDOW,
                        new ActivityResult(Activity.RESULT_OK, new Intent()));

        verify(mDelegate).onCancel();
        verify(mDelegate, never()).onPickTab(any(), anyBoolean());
        verify(mDelegate, never()).onPickWindow();
        assertNull(mPickerDelegate.getStartedWebContents());
        assertEquals(params.webContents, mPickerDelegate.getFinishedWebContents());
        resultWatcher.assertExpected();
    }

    /**
     * If the capture request is torn down while its WebContents stays alive (e.g. frame navigation
     * or iframe detach), a positive picker result cancels cleanly instead of arming ScreenCapture
     * or starting an orphaned projection.
     */
    @Test
    public void testShow_requestDestroyedBeforeResult() {
        var resultWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Media.MediaCapture.UI.Android.Picker.Result",
                        MediaCapturePickerManager.Result.CANCELLED);
        mPickerDelegate.setIntent(new Intent());
        Params params = mediaCaptureParams();
        MediaCapturePickerInvoker.show(mActivity, params, mDelegate);
        IntentForResult launched = shadowOf(mActivity).getNextStartedActivityForResult();
        assertNotNull(launched);

        doReturn(true).when(mDelegate).isRequestDestroyed();
        ((FragmentActivity) mActivity)
                .getActivityResultRegistry()
                .dispatchResult(launched.requestCode, Activity.RESULT_OK, new Intent());

        verify(mDelegate).onCancel();
        verify(mDelegate, never()).onPickWindow();
        assertNull(mPickerDelegate.getStartedWebContents());
        assertNull(mPickerDelegate.getStoppedWebContents());
        assertEquals(params.webContents, mPickerDelegate.getFinishedWebContents());
        resultWatcher.assertExpected();

        // A subsequent window capture must succeed without hitting stale ScreenCapture pick state.
        Delegate nextDelegate = mock(Delegate.class);
        MediaCapturePickerInvoker.show(mActivity, mediaCaptureParams(), nextDelegate);
        pendingDelegate()
                .onPicked(
                        CaptureAction.CAPTURE_WINDOW,
                        new ActivityResult(Activity.RESULT_OK, new Intent()));
        verify(nextDelegate).onPickWindow();
    }

    /** A second request from the same window while its prompt is showing is cancelled. */
    @Test
    public void testShow_secondRequestWhilePromptShowingIsCancelled() {
        mPickerDelegate.setIntent(new Intent());
        Delegate secondDelegate = mock(Delegate.class);

        MediaCapturePickerInvoker.show(mActivity, mediaCaptureParams(), mDelegate);
        MediaCapturePickerInvoker.show(mActivity, mediaCaptureParams(), secondDelegate);

        // The second request is cancelled right away and no second prompt is launched.
        verify(secondDelegate).onCancel();
        verify(mDelegate, never()).onCancel();
        assertNotNull(shadowOf(mActivity).getNextStartedActivityForResult());
        assertNull(shadowOf(mActivity).getNextStartedActivityForResult());

        // The first request still completes normally.
        pendingDelegate()
                .onPicked(
                        CaptureAction.CAPTURE_WINDOW,
                        new ActivityResult(Activity.RESULT_OK, new Intent()));
        verify(mDelegate).onPickWindow();
    }
}

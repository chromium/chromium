// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;

import androidx.activity.result.ActivityResult;
import androidx.fragment.app.FragmentActivity;
import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.CallbackUtils;
import org.chromium.base.ServiceLoaderUtil;
import org.chromium.base.test.BaseRobolectricTestRunner;
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
        private WebContents mStoppedWebContents;

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
        public void stopAppContentMediaProjection(WebContents webContents) {
            mStoppedWebContents = webContents;
        }

        private WebContents getStoppedWebContents() {
            return mStoppedWebContents;
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
    }

    @Test
    public void testShow_cancel() {
        mPickerDelegate.setIntent(new Intent());
        MediaCapturePickerInvoker.show(mActivity, mediaCaptureParams(), mDelegate);
        MediaCapturePickerHeadlessFragment fragment =
                MediaCapturePickerHeadlessFragment.getInstance((FragmentActivity) mActivity);
        fragment.mNextDelegate.onPicked(
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
        MediaCapturePickerInvoker.show(mActivity, mediaCaptureParams(), mDelegate);
        MediaCapturePickerHeadlessFragment fragment =
                MediaCapturePickerHeadlessFragment.getInstance((FragmentActivity) mActivity);
        fragment.mNextDelegate.onPicked(
                CaptureAction.CAPTURE_WINDOW, new ActivityResult(Activity.RESULT_OK, new Intent()));
        verify(tab).loadIfNeeded(/* forceBackingSize= */ true);
        verify(mDelegate).onPickTab(mTabWebContents, true);
    }

    @Test
    public void testShow_tabWithoutAudio() {
        Tab tab = mock(Tab.class);
        doReturn(mTabWebContents).when(tab).getWebContents();

        mPickerDelegate.setIntent(new Intent());
        mPickerDelegate.setPickedTab(tab);
        mPickerDelegate.setShouldShareAudio(false);
        MediaCapturePickerInvoker.show(mActivity, mediaCaptureParams(), mDelegate);
        MediaCapturePickerHeadlessFragment fragment =
                MediaCapturePickerHeadlessFragment.getInstance((FragmentActivity) mActivity);
        fragment.mNextDelegate.onPicked(
                CaptureAction.CAPTURE_WINDOW, new ActivityResult(Activity.RESULT_OK, new Intent()));
        verify(tab).loadIfNeeded(/* forceBackingSize= */ true);
        verify(mDelegate).onPickTab(mTabWebContents, false);
    }

    @Test
    public void testShow_window() {
        mPickerDelegate.setIntent(new Intent());
        Params params = mediaCaptureParams();
        MediaCapturePickerInvoker.show(mActivity, params, mDelegate);
        MediaCapturePickerHeadlessFragment fragment =
                MediaCapturePickerHeadlessFragment.getInstance((FragmentActivity) mActivity);
        fragment.mNextDelegate.onPicked(
                CaptureAction.CAPTURE_WINDOW, new ActivityResult(Activity.RESULT_OK, new Intent()));
        verify(mDelegate).onPickWindow();
        assertEquals(params.webContents, mPickerDelegate.getStoppedWebContents());
    }

    @Test
    public void testShow_screen() {
        mPickerDelegate.setIntent(new Intent());
        Params params = mediaCaptureParams();
        MediaCapturePickerInvoker.show(mActivity, params, mDelegate);
        MediaCapturePickerHeadlessFragment fragment =
                MediaCapturePickerHeadlessFragment.getInstance((FragmentActivity) mActivity);
        fragment.mNextDelegate.onPicked(
                CaptureAction.CAPTURE_SCREEN, new ActivityResult(Activity.RESULT_OK, new Intent()));
        verify(mDelegate).onPickScreen();
        assertEquals(params.webContents, mPickerDelegate.getStoppedWebContents());
    }
}

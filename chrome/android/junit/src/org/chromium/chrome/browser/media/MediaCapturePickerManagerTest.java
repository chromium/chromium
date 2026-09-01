// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media;

import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.verify;

import android.content.Context;

import androidx.test.ext.junit.rules.ActivityScenarioRule;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.base.WindowAndroid;

import java.lang.ref.WeakReference;

/** Tests for {@link MediaCapturePickerManager}. */
@RunWith(BaseRobolectricTestRunner.class)
public class MediaCapturePickerManagerTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Mock private WebContents mWebContents;
    @Mock private WindowAndroid mWindowAndroid;
    @Mock private MediaCapturePickerManager.Delegate mDelegate;
    @Mock private Tab mTab;
    @Mock private Callback<Tab> mTestingCallback;

    private Context mContext;

    @Before
    public void setUp() {
        mActivityScenarioRule.getScenario().onActivity(activity -> mContext = activity);
    }

    @After
    public void tearDown() {
        MediaCapturePickerManager.setBringTabToFrontCallbackForTesting(null);
    }

    private MediaCapturePickerManager.Params createParams() {
        return new MediaCapturePickerManager.Params(
                mWebContents,
                "App",
                "Target",
                /* requestAudio= */ true,
                /* excludeSystemAudio= */ false,
                /* windowAudioPreference= */ 0,
                /* preferredDisplaySurface= */ 0,
                /* captureThisTab= */ false,
                /* excludeSelfBrowserSurface= */ true,
                /* excludeMonitorTypeSurfaces= */ false,
                /* allowedCaptureLevel= */ 0);
    }

    @Test
    @SmallTest
    public void testShowDialog_NullWindowAndroid() {
        doReturn(null).when(mWebContents).getTopLevelNativeWindow();

        var histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Media.MediaCapture.UI.Android.Picker.PreShowFailure",
                        MediaCapturePickerManager.PreShowFailure.CONTEXT_NULL_ERROR);

        MediaCapturePickerManager.showDialog(createParams(), mDelegate);

        verify(mDelegate).onCancel();
        histogramWatcher.assertExpected();
    }

    @Test
    @SmallTest
    public void testShowDialog_NullContext() {
        doReturn(mWindowAndroid).when(mWebContents).getTopLevelNativeWindow();
        doReturn(new WeakReference<Context>(null)).when(mWindowAndroid).getActivity();
        doReturn(new WeakReference<Context>(null)).when(mWindowAndroid).getContext();

        var histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Media.MediaCapture.UI.Android.Picker.PreShowFailure",
                        MediaCapturePickerManager.PreShowFailure.CONTEXT_NULL_ERROR);

        MediaCapturePickerManager.showDialog(createParams(), mDelegate);

        verify(mDelegate).onCancel();
        histogramWatcher.assertExpected();
    }

    @Test
    @SmallTest
    public void testShowDialog() {
        doReturn(mWindowAndroid).when(mWebContents).getTopLevelNativeWindow();
        doReturn(new WeakReference<Context>(mContext)).when(mWindowAndroid).getContext();

        MediaCapturePickerManager.showDialog(createParams(), mDelegate);
    }

    @Test
    @SmallTest
    public void testRecordResultAndPreShowFailure() {
        var resultWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Media.MediaCapture.UI.Android.Picker.Result",
                        MediaCapturePickerManager.Result.TAB_SELECTED);
        MediaCapturePickerManager.recordResult(MediaCapturePickerManager.Result.TAB_SELECTED);
        resultWatcher.assertExpected();

        var failureWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Media.MediaCapture.UI.Android.Picker.PreShowFailure",
                        MediaCapturePickerManager.PreShowFailure.PICKER_DELEGATE_NULL_ERROR);
        MediaCapturePickerManager.recordPreShowFailure(
                MediaCapturePickerManager.PreShowFailure.PICKER_DELEGATE_NULL_ERROR);
        failureWatcher.assertExpected();
    }

    @Test
    @SmallTest
    public void testBringTabToFront() {
        MediaCapturePickerManager.setBringTabToFrontCallbackForTesting(mTestingCallback);
        MediaCapturePickerManager.bringTabToFront(mContext, mTab);
        verify(mTestingCallback).onResult(mTab);
    }
}

// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.screenshot;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.content.Context;
import android.graphics.Bitmap;
import android.net.Uri;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.UmaRecorderHolder;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.share.share_sheet.ChromeOptionShareCallback;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.JUnitTestGURLs;

/**
 * Tests for {@link ScreenshotShareSheetMediator}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
// clang-format off
public class ScreenshotShareSheetMediatorUnitTest {
    // clang-format on
    @Rule
    public TestRule mProcessor = new Features.JUnitProcessor();

    @Mock
    Runnable mDeleteRunnable;

    @Mock
    Runnable mSaveRunnable;

    @Mock
    Callback<Runnable> mInstallRunnable;

    @Mock
    Activity mContext;

    @Mock
    WindowAndroid mWindowAndroid;

    @Mock
    ChromeOptionShareCallback mShareCallback;

    private PropertyModel mModel;

    private static class MockScreenshotShareSheetMediator extends ScreenshotShareSheetMediator {
        private boolean mGenerateTemporaryUriFromBitmapCalled;

        MockScreenshotShareSheetMediator(Context context, PropertyModel propertyModel,
                Runnable deleteRunnable, Runnable saveRunnable, WindowAndroid windowAndroid,
                ChromeOptionShareCallback chromeOptionShareCallback,
                Callback<Runnable> installCallback) {
            super(context, propertyModel, deleteRunnable, saveRunnable, windowAndroid,
                    JUnitTestGURLs.EXAMPLE_URL, chromeOptionShareCallback, installCallback);
        }
        @Override
        protected void generateTemporaryUriFromBitmap(
                String fileName, Bitmap bitmap, Callback<Uri> callback) {
            mGenerateTemporaryUriFromBitmapCalled = true;
        }

        public boolean generateTemporaryUriFromBitmapCalled() {
            return mGenerateTemporaryUriFromBitmapCalled;
        }
    };

    private MockScreenshotShareSheetMediator mMediator;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        UmaRecorderHolder.resetForTesting();

        doNothing().when(mDeleteRunnable).run();

        doNothing().when(mSaveRunnable).run();

        doNothing().when(mShareCallback).showThirdPartyShareSheet(any(), any(), anyLong());

        mModel = new PropertyModel(ScreenshotShareSheetViewProperties.ALL_KEYS);

        mMediator = new MockScreenshotShareSheetMediator(mContext, mModel, mDeleteRunnable,
                mSaveRunnable, mWindowAndroid, mShareCallback, mInstallRunnable);
    }

    @Test
    public void onClickDelete() {
        Callback<Integer> callback =
                mModel.get(ScreenshotShareSheetViewProperties.NO_ARG_OPERATION_LISTENER);
        callback.onResult(ScreenshotShareSheetViewProperties.NoArgOperation.DELETE);

        verify(mDeleteRunnable).run();
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Sharing.ScreenshotFallback.Action",
                        ScreenshotShareSheetMetrics.ScreenshotShareSheetAction.DELETE));
    }

    @Test
    public void onClickSave() {
        Callback<Integer> callback =
                mModel.get(ScreenshotShareSheetViewProperties.NO_ARG_OPERATION_LISTENER);
        callback.onResult(ScreenshotShareSheetViewProperties.NoArgOperation.SAVE);

        verify(mSaveRunnable).run();
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Sharing.ScreenshotFallback.Action",
                        ScreenshotShareSheetMetrics.ScreenshotShareSheetAction.SAVE));
    }

    @Test
    public void onClickShare() {
        Callback<Integer> callback =
                mModel.get(ScreenshotShareSheetViewProperties.NO_ARG_OPERATION_LISTENER);
        callback.onResult(ScreenshotShareSheetViewProperties.NoArgOperation.SHARE);

        Assert.assertTrue(mMediator.generateTemporaryUriFromBitmapCalled());
        verify(mDeleteRunnable).run();
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Sharing.ScreenshotFallback.Action",
                        ScreenshotShareSheetMetrics.ScreenshotShareSheetAction.SHARE));
    }

    @Test
    public void onClickInstall() {
        Callback<Integer> callback =
                mModel.get(ScreenshotShareSheetViewProperties.NO_ARG_OPERATION_LISTENER);
        callback.onResult(ScreenshotShareSheetViewProperties.NoArgOperation.INSTALL);

        verify(mInstallRunnable).onResult(any());
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Sharing.ScreenshotFallback.Action",
                        ScreenshotShareSheetMetrics.ScreenshotShareSheetAction.EDIT));
    }

    @After
    public void tearDown() {}
}

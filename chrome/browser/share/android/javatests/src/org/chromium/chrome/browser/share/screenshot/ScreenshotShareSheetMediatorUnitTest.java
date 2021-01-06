// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.screenshot;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.doReturn;
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
import org.chromium.base.metrics.test.ShadowRecordHistogram;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.share.share_sheet.ChromeOptionShareCallback;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Tests for {@link ScreenshotShareSheetMediator}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE, shadows = {ShadowRecordHistogram.class})
// clang-format off
@Features.EnableFeatures(ChromeFeatureList.CHROME_SHARE_SCREENSHOT)
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
    Tab mTab;

    @Mock
    ChromeOptionShareCallback mShareCallback;

    private PropertyModel mModel;

    private class MockScreenshotShareSheetMediator extends ScreenshotShareSheetMediator {
        private boolean mGenerateTemporaryUriFromBitmapCalled;

        MockScreenshotShareSheetMediator(Context context, PropertyModel propertyModel,
                Runnable deleteRunnable, Runnable saveRunnable, Tab tab,
                ChromeOptionShareCallback chromeOptionShareCallback,
                Callback<Runnable> installCallback) {
            super(context, propertyModel, deleteRunnable, saveRunnable, tab,
                    chromeOptionShareCallback, installCallback);
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

        ShadowRecordHistogram.reset();

        doNothing().when(mDeleteRunnable).run();

        doNothing().when(mSaveRunnable).run();

        doNothing().when(mShareCallback).showThirdPartyShareSheet(any(), any(), anyLong());

        doReturn(true).when(mTab).isInitialized();

        mModel = new PropertyModel(ScreenshotShareSheetViewProperties.ALL_KEYS);

        mMediator = new MockScreenshotShareSheetMediator(mContext, mModel, mDeleteRunnable,
                mSaveRunnable, mTab, mShareCallback, mInstallRunnable);
    }

    @Test
    public void onClickDelete() {
        Callback<Integer> callback =
                mModel.get(ScreenshotShareSheetViewProperties.NO_ARG_OPERATION_LISTENER);
        callback.onResult(ScreenshotShareSheetViewProperties.NoArgOperation.DELETE);

        verify(mDeleteRunnable).run();
        Assert.assertEquals(1,
                ShadowRecordHistogram.getHistogramValueCountForTesting(
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
                ShadowRecordHistogram.getHistogramValueCountForTesting(
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
                ShadowRecordHistogram.getHistogramValueCountForTesting(
                        "Sharing.ScreenshotFallback.Action",
                        ScreenshotShareSheetMetrics.ScreenshotShareSheetAction.SHARE));
    }

    @Test
    public void onClickShareUninitialized() {
        doReturn(false).when(mTab).isInitialized();
        Callback<Integer> callback =
                mModel.get(ScreenshotShareSheetViewProperties.NO_ARG_OPERATION_LISTENER);
        callback.onResult(ScreenshotShareSheetViewProperties.NoArgOperation.SHARE);

        Assert.assertFalse(mMediator.generateTemporaryUriFromBitmapCalled());
    }

    @Test
    public void onClickInstall() {
        Callback<Integer> callback =
                mModel.get(ScreenshotShareSheetViewProperties.NO_ARG_OPERATION_LISTENER);
        callback.onResult(ScreenshotShareSheetViewProperties.NoArgOperation.INSTALL);

        verify(mInstallRunnable).onResult(any());
        Assert.assertEquals(1,
                ShadowRecordHistogram.getHistogramValueCountForTesting(
                        "Sharing.ScreenshotFallback.Action",
                        ScreenshotShareSheetMetrics.ScreenshotShareSheetAction.EDIT));
    }

    @After
    public void tearDown() {}
}

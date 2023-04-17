// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.long_screenshots;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.graphics.Bitmap;
import android.net.Uri;

import androidx.fragment.app.FragmentActivity;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.share.ChromeShareExtras;
import org.chromium.chrome.browser.share.long_screenshots.bitmap_generation.EntryManager;
import org.chromium.chrome.browser.share.share_sheet.ChromeOptionShareCallback;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.modules.image_editor.ImageEditorModuleProvider;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.share.ShareImageFileUtils;
import org.chromium.components.browser_ui.share.ShareParams;
import org.chromium.ui.test.util.MockitoHelper;
import org.chromium.url.JUnitTestGURLs;
import org.chromium.url.ShadowGURL;

/** Tests for the LongScreenshotsCoordinator. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class LongScreenshotsCoordinatorTest {
    private static final Uri TEST_IMAGE_URI = Uri.parse("content://path/to-image");
    private LongScreenshotsCoordinator mCoordinator;

    @Mock
    private FragmentActivity mActivity;

    @Mock
    private ChromeOptionShareCallback mChromeOptionShareCallback;

    @Mock
    private ImageEditorModuleProvider mImageEditorModuleProviderMock;

    @Mock
    private BottomSheetController mBottomSheetControllerMock;

    @Mock
    private Tab mTab;

    @Mock
    private EntryManager mManager;

    @Mock
    private LongScreenshotsMediator mMediator;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        doReturn(ContextUtils.getApplicationContext()).when(mActivity).getApplicationContext();
    }

    @Test
    public void testCaptureScreenshotInitialEntrySuccess() {
        // Instantiate the object under test.
        mCoordinator = LongScreenshotsCoordinator.createForTests(mActivity, mTab,
                JUnitTestGURLs.EXAMPLE_URL, mChromeOptionShareCallback, mBottomSheetControllerMock,
                mImageEditorModuleProviderMock, mManager, mMediator, true);
        mCoordinator.captureScreenshot();
        verify(mMediator, times(1)).capture(Mockito.isA(Runnable.class));
    }

    @Test
    @Config(shadows = {ShadowShareImageFileUtils.class, ShadowGURL.class})
    public void bypassPreviewDialog() {
        // Instantiate the object under test.
        mCoordinator = LongScreenshotsCoordinator.createForTests(mActivity, mTab,
                JUnitTestGURLs.EXAMPLE_URL, mChromeOptionShareCallback, mBottomSheetControllerMock,
                null, mManager, mMediator, true);
        doReturn(Bitmap.createBitmap(1, 1, Bitmap.Config.ARGB_8888))
                .when(mMediator)
                .getScreenshot();
        MockitoHelper.doCallback(Runnable::run).when(mMediator).capture(any());

        mCoordinator.captureScreenshot();

        ArgumentCaptor<ShareParams> paramsCaptor = ArgumentCaptor.forClass(ShareParams.class);
        ArgumentCaptor<ChromeShareExtras> shareExtraCaptor =
                ArgumentCaptor.forClass(ChromeShareExtras.class);
        verify(mChromeOptionShareCallback)
                .showShareSheet(paramsCaptor.capture(), shareExtraCaptor.capture(), anyLong());

        Assert.assertEquals("ScreenshotUri not found.", TEST_IMAGE_URI,
                paramsCaptor.getValue().getImageUriToShare());
        Assert.assertEquals("Share URL is different.", JUnitTestGURLs.EXAMPLE_URL,
                shareExtraCaptor.getValue().getContentUrl().getSpec());
        Assert.assertEquals("Screenshot type is not used correctly.",
                ChromeShareExtras.DetailedContentType.SCREENSHOT,
                shareExtraCaptor.getValue().getDetailedContentType());
    }

    @Implements(ShareImageFileUtils.class)
    static class ShadowShareImageFileUtils {
        @Implementation
        public static void generateTemporaryUriFromBitmap(
                String fileName, Bitmap bitmap, Callback<Uri> callback) {
            callback.onResult(TEST_IMAGE_URI);
        }
    }
}

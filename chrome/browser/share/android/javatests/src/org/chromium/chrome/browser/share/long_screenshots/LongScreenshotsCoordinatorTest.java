// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.long_screenshots;

import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.graphics.Bitmap;

import androidx.fragment.app.FragmentActivity;
import androidx.fragment.app.FragmentManager;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.image_editor.ImageEditorDialogCoordinator;
import org.chromium.chrome.browser.share.long_screenshots.bitmap_generation.EntryManager;
import org.chromium.chrome.browser.share.long_screenshots.bitmap_generation.LongScreenshotsEntry;
import org.chromium.chrome.browser.share.screenshot.ScreenshotShareSheetDialog;
import org.chromium.chrome.browser.share.share_sheet.ChromeOptionShareCallback;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.modules.image_editor.ImageEditorModuleProvider;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;

/** Tests for the LongScreenshotsCoordinator. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@Features.EnableFeatures(ChromeFeatureList.CHROME_SHARE_LONG_SCREENSHOT)
public class LongScreenshotsCoordinatorTest {
    private LongScreenshotsCoordinator mCoordinator;
    private Bitmap mBitmap;

    @Mock
    private FragmentActivity mActivity;

    @Mock
    private FragmentManager mFragmentManagerMock;

    @Mock
    private ChromeOptionShareCallback mChromeOptionShareCallback;

    @Mock
    private ImageEditorDialogCoordinator mImageEditorDialogCoordinatorMock;

    @Mock
    private ImageEditorModuleProvider mImageEditorModuleProviderMock;

    @Mock
    private ScreenshotShareSheetDialog mScreenshotShareSheetDialogMock;

    @Mock
    private BottomSheetController mBottomSheetControllerMock;

    @Mock
    private Tab mTab;

    @Mock
    private EntryManager mManager;

    @Mock
    private LongScreenshotsEntry mEntry;

    @Mock
    private LongScreenshotsMediator mMediator;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        // Instantiate the object under test.
        mCoordinator = LongScreenshotsCoordinator.createForTests(mActivity, mTab,
                mChromeOptionShareCallback, mBottomSheetControllerMock,
                mImageEditorModuleProviderMock, mManager, mMediator);
    }

    @Test
    public void testCaptureScreenshotInitialEntrySuccess() {
        mCoordinator.captureScreenshot();
        verify(mMediator, times(1)).capture(Mockito.isA(Runnable.class));
    }
}

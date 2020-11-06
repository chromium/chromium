// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.screenshot;

import static org.mockito.AdditionalAnswers.answerVoid;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.graphics.Bitmap;

import androidx.annotation.Nullable;
import androidx.fragment.app.FragmentActivity;
import androidx.fragment.app.FragmentManager;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.image_editor.ImageEditorDialogCoordinator;
import org.chromium.chrome.browser.share.share_sheet.ChromeOptionShareCallback;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.modules.image_editor.ImageEditorModuleProvider;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.module_installer.engine.InstallListener;

// clang-format off
/**
 * Tests for {@link ScreenshotCoordinator}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Features.EnableFeatures({ChromeFeatureList.CHROME_SHARE_SCREENSHOT,
        ChromeFeatureList.CHROME_SHARING_HUB})
public class ScreenshotCoordinatorTest {
    // clang-format on
    @Mock
    private FragmentActivity mActivity;

    @Mock
    private FragmentManager mFragmentManagerMock;

    @Mock
    private ChromeOptionShareCallback mChromeOptionShareCallback;

    // FakeEditorScreenshotTask abstracts taking a screenshot; it always succeeds and
    // returns mBitmap from the test class.
    private final class FakeEditorScreenshotTask implements EditorScreenshotSource {
        public FakeEditorScreenshotTask() {}

        @Override
        public void capture(@Nullable Runnable callback) {
            Assert.assertNotNull(callback);
            callback.run();
        }

        @Override
        public boolean isReady() {
            return true;
        }

        @Override
        public Bitmap getScreenshot() {
            return mBitmap;
        }
    }

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

    // Bitmap used for successful screenshot capture requests.
    private Bitmap mBitmap;

    // Object under test.
    private ScreenshotCoordinator mScreenshotCoordinator;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        when(mActivity.getSupportFragmentManager()).thenReturn(mFragmentManagerMock);

        when(mImageEditorModuleProviderMock.getImageEditorDialogCoordinator())
                .thenReturn(mImageEditorDialogCoordinatorMock);
        doNothing()
                .when(mImageEditorDialogCoordinatorMock)
                .launchEditor(mActivity, mBitmap, mTab, mChromeOptionShareCallback);

        mBitmap = Bitmap.createBitmap(800, 600, Bitmap.Config.ARGB_8888);

        // Instantiate the object under test.
        mScreenshotCoordinator =
                new ScreenshotCoordinator(mActivity, mTab, new FakeEditorScreenshotTask(),
                        mScreenshotShareSheetDialogMock, mChromeOptionShareCallback,
                        mBottomSheetControllerMock, mImageEditorModuleProviderMock);
    }

    @Test
    public void editorLaunchesWhenModuleInstalled() {
        when(mImageEditorModuleProviderMock.isModuleInstalled()).thenReturn(true);
        // Simulate a successful install.
        doAnswer(answerVoid((InstallListener listener) -> listener.onComplete(/*success=*/true)))
                .when(mImageEditorModuleProviderMock)
                .maybeInstallModule(any());

        // The first request for the feature will ask for module install, which succeeds,
        // and the callback will launch the editor.
        mScreenshotCoordinator.captureScreenshot();

        // Ensure the editor launches.
        verify(mImageEditorDialogCoordinatorMock)
                .launchEditor(mActivity, mBitmap, mTab, mChromeOptionShareCallback);
    }

    @Test
    public void retryInstallSuccess() {
        // First request does not have the module available, and install fails.
        when(mImageEditorModuleProviderMock.isModuleInstalled()).thenReturn(false);
        doAnswer(answerVoid((InstallListener listener) -> listener.onComplete(/*success=*/false)))
                .when(mImageEditorModuleProviderMock)
                .maybeInstallModule(any());

        doNothing().when(mScreenshotShareSheetDialogMock).show(any(FragmentManager.class), any());

        mScreenshotCoordinator.captureScreenshot();

        // Failed install loads the share sheet.
        verify(mScreenshotShareSheetDialogMock).show(any(FragmentManager.class), any());
        // The editor is not launched.
        verify(mImageEditorDialogCoordinatorMock, never()).launchEditor(any(), any(), any(), any());

        // A second install is attempted and succeeds.
        when(mImageEditorModuleProviderMock.isModuleInstalled()).thenReturn(true);
        doAnswer(answerVoid((InstallListener listener) -> listener.onComplete(/*success=*/true)))
                .when(mImageEditorModuleProviderMock)
                .maybeInstallModule(any());

        mScreenshotCoordinator.captureScreenshot();
        // The editor should launch without requiring a discrete user action.
        verify(mImageEditorDialogCoordinatorMock)
                .launchEditor(mActivity, mBitmap, mTab, mChromeOptionShareCallback);
    }

    @Test
    public void maxInstallTriesExceeded() {
        // Attempt several installs which persistently fail, up to the limit.
        when(mImageEditorModuleProviderMock.isModuleInstalled()).thenReturn(false);
        doAnswer(answerVoid((InstallListener listener) -> listener.onComplete(/*success=*/false)))
                .when(mImageEditorModuleProviderMock)
                .maybeInstallModule(any());
        for (int attempt = 0; attempt < ScreenshotCoordinator.MAX_INSTALL_ATTEMPTS; attempt++) {
            mScreenshotCoordinator.captureScreenshot();
        }

        // Ensure we tried to install N times.
        verify(mImageEditorModuleProviderMock, times(ScreenshotCoordinator.MAX_INSTALL_ATTEMPTS))
                .maybeInstallModule(any());
        // Failed install loads the share sheet.
        verify(mScreenshotShareSheetDialogMock, times(ScreenshotCoordinator.MAX_INSTALL_ATTEMPTS))
                .show(any(FragmentManager.class), any());
        // Ensure the editor was never loaded.
        verify(mImageEditorDialogCoordinatorMock, never()).launchEditor(any(), any(), any(), any());

        // Subsequent attempts will not invoke installation.
        mScreenshotCoordinator.captureScreenshot();
        // maybeInstallModule will not be called again; count will remain the same, at the limit.
        verify(mImageEditorModuleProviderMock, times(ScreenshotCoordinator.MAX_INSTALL_ATTEMPTS))
                .maybeInstallModule(any());
        // The share sheet will be loaded directly in all cases plus the failure case.
        verify(mScreenshotShareSheetDialogMock,
                times(ScreenshotCoordinator.MAX_INSTALL_ATTEMPTS + 1))
                .show(any(FragmentManager.class), any());
        // The editor should not attempt to be loaded.
        verify(mImageEditorDialogCoordinatorMock, never()).launchEditor(any(), any(), any(), any());
    }
}

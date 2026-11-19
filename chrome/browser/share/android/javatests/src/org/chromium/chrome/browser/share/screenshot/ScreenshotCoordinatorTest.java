// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.screenshot;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.graphics.Bitmap;
import android.os.Looper;

import androidx.annotation.Nullable;
import androidx.fragment.app.FragmentActivity;
import androidx.fragment.app.FragmentManager;
import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Shadows;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.share.share_sheet.ChromeOptionShareCallback;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.JUnitTestGURLs;

import java.lang.ref.WeakReference;

/** Tests for {@link ScreenshotCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class ScreenshotCoordinatorTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public ActivityScenarioRule<FragmentActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(FragmentActivity.class);

    @Mock private ChromeOptionShareCallback mChromeOptionShareCallback;

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

    @Mock private ScreenshotShareSheetDialog mScreenshotShareSheetDialogMock;

    @Mock private BottomSheetController mBottomSheetControllerMock;

    @Mock private WindowAndroid mWindowAndroid;

    @Mock private Profile mProfile;

    private FragmentActivity mActivity;

    // Bitmap used for successful screenshot capture requests.
    private Bitmap mBitmap;

    // Object under test.
    private ScreenshotCoordinator mScreenshotCoordinator;

    @Before
    public void setUp() {
        mActivityScenarioRule.getScenario().onActivity((activity) -> mActivity = activity);

        when(mWindowAndroid.getActivity()).thenReturn(new WeakReference<>(mActivity));

        mBitmap = Bitmap.createBitmap(800, 600, Bitmap.Config.ARGB_8888);

        // Instantiate the object under test.
        mScreenshotCoordinator =
                new ScreenshotCoordinator(
                        mActivity,
                        mWindowAndroid,
                        JUnitTestGURLs.EXAMPLE_URL.getSpec(),
                        new FakeEditorScreenshotTask(),
                        mScreenshotShareSheetDialogMock,
                        mChromeOptionShareCallback,
                        mBottomSheetControllerMock,
                        mProfile);
    }

    @Test
    public void captureScreenshotLaunchDialog() {
        // The first request for the feature will ask for module install, which succeeds,
        // and the callback will launch the editor.
        mScreenshotCoordinator.captureScreenshot();
        verify(mScreenshotShareSheetDialogMock)
                .init(
                        eq(mBitmap),
                        eq(mWindowAndroid),
                        eq(JUnitTestGURLs.EXAMPLE_URL.getSpec()),
                        eq(mChromeOptionShareCallback),
                        eq(mProfile));
        verify(mScreenshotShareSheetDialogMock).show(any(FragmentManager.class), any());
    }

    @Test
    public void screenshotDialogDismissesWhenProfileNull() {
        ScreenshotShareSheetDialog dialog = new ScreenshotShareSheetDialog();
        dialog.init(
                mBitmap,
                mWindowAndroid,
                JUnitTestGURLs.EXAMPLE_URL.getSpec(),
                mChromeOptionShareCallback,
                null);
        dialog.show(mActivity.getSupportFragmentManager(), "screenshot");
        mActivity.getSupportFragmentManager().executePendingTransactions();
        Shadows.shadowOf(Looper.getMainLooper()).idle();
        Assert.assertFalse(dialog.isAdded());
        Assert.assertNull(dialog.getDialog());
    }

    @Test
    public void screenshotDialogDismissesWhenProfileShutdownStarted() {
        when(mProfile.shutdownStarted()).thenReturn(true);
        ScreenshotShareSheetDialog dialog = new ScreenshotShareSheetDialog();
        dialog.init(
                mBitmap,
                mWindowAndroid,
                JUnitTestGURLs.EXAMPLE_URL.getSpec(),
                mChromeOptionShareCallback,
                mProfile);
        dialog.show(mActivity.getSupportFragmentManager(), "screenshot");
        mActivity.getSupportFragmentManager().executePendingTransactions();
        Shadows.shadowOf(Looper.getMainLooper()).idle();
        Assert.assertFalse(dialog.isAdded());
        Assert.assertNull(dialog.getDialog());
    }
}

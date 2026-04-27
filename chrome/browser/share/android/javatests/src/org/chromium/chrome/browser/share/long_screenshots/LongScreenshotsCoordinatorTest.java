// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.long_screenshots;

import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import androidx.fragment.app.FragmentActivity;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLooper;
import org.robolectric.shadows.ShadowToast;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.share.long_screenshots.bitmap_generation.EntryManager;
import org.chromium.chrome.browser.share.share_sheet.ChromeOptionShareCallback;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.url.JUnitTestGURLs;

/** Tests for the LongScreenshotsCoordinator. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(
        manifest = Config.NONE,
        shadows = {ShadowToast.class})
public class LongScreenshotsCoordinatorTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();
    private LongScreenshotsCoordinator mCoordinator;

    private FragmentActivity mActivity;

    @Mock private ChromeOptionShareCallback mChromeOptionShareCallback;

    @Mock private BottomSheetController mBottomSheetControllerMock;

    @Mock private Tab mTab;

    @Mock private EntryManager mManager;

    @Mock private LongScreenshotsMediator mMediator;

    @Captor private ArgumentCaptor<Runnable> mCallbackCaptor;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(FragmentActivity.class).setup().get();
        // Instantiate the object under test.
        mCoordinator =
                LongScreenshotsCoordinator.createForTests(
                        mActivity,
                        mTab,
                        JUnitTestGURLs.EXAMPLE_URL.getSpec(),
                        mChromeOptionShareCallback,
                        mBottomSheetControllerMock,
                        mManager,
                        mMediator);
    }

    @Test
    public void testCaptureScreenshotInitialEntrySuccess() {
        mCoordinator.captureScreenshot();
        verify(mMediator, times(1)).capture(Mockito.isA(Runnable.class));
    }

    @Test
    public void testHandleScreenshot_NullScreenshotShowsError() {
        mCoordinator.captureScreenshot();
        verify(mMediator).capture(mCallbackCaptor.capture());

        when(mMediator.getScreenshot()).thenReturn(null);
        mCallbackCaptor.getValue().run();

        ShadowLooper.runUiThreadTasks();
        assertTrue(
                ShadowToast.showedCustomToast(
                        mActivity.getString(R.string.sharing_long_screenshot_unknown_error),
                        R.id.toast_text));
    }
}

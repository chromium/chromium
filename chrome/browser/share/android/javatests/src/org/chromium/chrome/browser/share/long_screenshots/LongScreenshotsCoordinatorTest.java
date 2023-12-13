// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.long_screenshots;

import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import androidx.fragment.app.FragmentActivity;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.share.long_screenshots.bitmap_generation.EntryManager;
import org.chromium.chrome.browser.share.share_sheet.ChromeOptionShareCallback;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.url.JUnitTestGURLs;

/** Tests for the LongScreenshotsCoordinator. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class LongScreenshotsCoordinatorTest {
    private LongScreenshotsCoordinator mCoordinator;

    @Mock private FragmentActivity mActivity;

    @Mock private ChromeOptionShareCallback mChromeOptionShareCallback;

    @Mock private BottomSheetController mBottomSheetControllerMock;

    @Mock private Tab mTab;

    @Mock private EntryManager mManager;

    @Mock private LongScreenshotsMediator mMediator;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

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
}

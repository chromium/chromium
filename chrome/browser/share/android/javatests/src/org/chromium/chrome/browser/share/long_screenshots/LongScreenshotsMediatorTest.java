// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.long_screenshots;

import android.app.Activity;
import android.graphics.Bitmap;
import android.os.Looper;
import android.view.View;

import androidx.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.FeatureList;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.share.long_screenshots.bitmap_generation.EntryManager;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.ui.test.util.BlankUiTestActivity;

/** Tests for the LongScreenshotsMediator. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class LongScreenshotsMediatorTest {
    private Activity mActivity;
    private Bitmap mBitmap;
    private FeatureList.TestValues mTestValues;
    private LongScreenshotsMediator mMediator;

    @Rule
    public BaseActivityTestRule<BlankUiTestActivity> mActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    @Mock
    private View mView;

    @Mock
    private EntryManager mManager;

    @Before
    public void setUp() {
        Looper.prepare();

        mActivityTestRule.launchActivity(null);
        mActivity = mActivityTestRule.getActivity();

        MockitoAnnotations.initMocks(this);

        mBitmap = Bitmap.createBitmap(800, 600, Bitmap.Config.ARGB_8888);

        // Instantiate the object under test.
        mMediator = new LongScreenshotsMediator(mActivity, mManager);
    }

    @Test
    @MediumTest
    public void testAreaSelectionDone() {
        mMediator.showAreaSelectionDialog(mBitmap);
        Assert.assertTrue(mMediator.getDialog().isShowing());

        mMediator.areaSelectionDone(mView);
        Assert.assertFalse(mMediator.getDialog().isShowing());
    }

    @Test
    @MediumTest
    public void testAreaSelectionClose() {
        mMediator.showAreaSelectionDialog(mBitmap);
        Assert.assertTrue(mMediator.getDialog().isShowing());

        mMediator.areaSelectionClose(mView);
        Assert.assertFalse(mMediator.getDialog().isShowing());
    }
}

// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.long_screenshots;

import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.graphics.Bitmap;
import android.graphics.Point;
import android.os.Looper;
import android.util.Size;
import android.view.View;

import androidx.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.share.long_screenshots.bitmap_generation.EntryManager;
import org.chromium.chrome.browser.share.long_screenshots.bitmap_generation.EntryManager.BitmapGeneratorObserver;
import org.chromium.chrome.browser.share.long_screenshots.bitmap_generation.LongScreenshotsEntry;
import org.chromium.chrome.browser.share.long_screenshots.bitmap_generation.LongScreenshotsEntry.EntryListener;
import org.chromium.chrome.browser.share.long_screenshots.bitmap_generation.LongScreenshotsEntry.EntryStatus;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.ui.test.util.BlankUiTestActivity;

/** Tests for the LongScreenshotsMediator. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class LongScreenshotsMediatorTest {
    /** Some nominal representative screen dimension */
    private static final int NOMINAL_SCREEN_DIMENTION = 1000;
    private Activity mActivity;
    private Bitmap mBitmap;
    private LongScreenshotsMediator mMediator;

    @Rule
    public BaseActivityTestRule<BlankUiTestActivity> mActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    @Mock
    private View mView;

    @Mock
    private EntryManager mManager;

    @Mock
    private LongScreenshotsEntry mLongScreenshotsEntry;

    @Captor
    private ArgumentCaptor<BitmapGeneratorObserver> mBitmapGeneratorObserverCaptor;

    @Captor
    private ArgumentCaptor<LongScreenshotsEntry.EntryListener> mEntryListenerCaptor;

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

    @Test
    @MediumTest
    public void testOnStatusChange() {
        mMediator.displayInitialScreenshot();
        verify(mManager).addBitmapGeneratorObserver(mBitmapGeneratorObserverCaptor.capture());
        BitmapGeneratorObserver generatorObserver = mBitmapGeneratorObserverCaptor.getValue();
        // Now we can call the onStatusChange that's within displayInitialScreenShot.
        generatorObserver.onStatusChange(EntryStatus.CAPTURE_IN_PROGRESS);
        generatorObserver.onStatusChange(EntryStatus.BITMAP_GENERATION_IN_PROGRESS);
    }

    @Test
    @MediumTest
    public void testOnCompositorReady() {
        Bitmap someBitmap = Bitmap.createBitmap(
                NOMINAL_SCREEN_DIMENTION, NOMINAL_SCREEN_DIMENTION, Bitmap.Config.ARGB_8888);
        mMediator.displayInitialScreenshot();
        verify(mManager).addBitmapGeneratorObserver(mBitmapGeneratorObserverCaptor.capture());
        BitmapGeneratorObserver generatorObserver = mBitmapGeneratorObserverCaptor.getValue();

        // Now we can call the onCompositorReady and capture it's EntryListener.
        when(mManager.generateFullpageEntry()).thenReturn(mLongScreenshotsEntry);
        when(mLongScreenshotsEntry.getBitmap()).thenReturn(someBitmap);
        generatorObserver.onCompositorReady(
                new Size(NOMINAL_SCREEN_DIMENTION, NOMINAL_SCREEN_DIMENTION), new Point(0, 0));
        verify(mLongScreenshotsEntry).setListener(mEntryListenerCaptor.capture());
        EntryListener entryListener = mEntryListenerCaptor.getValue();

        // Now we can call the onResult method of the EntryListener.
        // In-progress should be ignored.
        entryListener.onResult(EntryStatus.BITMAP_GENERATION_IN_PROGRESS);
        // Should already be captured, so this should put up a Toast with the error.
        entryListener.onResult(EntryStatus.CAPTURE_IN_PROGRESS);
        // This should trigger showAreaSelectionDialog to show the generated bitmap!
        entryListener.onResult(EntryStatus.BITMAP_GENERATED);
        Assert.assertTrue(mMediator.getDialog().isShowing());
    }
}

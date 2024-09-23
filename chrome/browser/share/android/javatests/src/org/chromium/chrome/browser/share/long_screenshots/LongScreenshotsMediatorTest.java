// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.long_screenshots;

import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.graphics.Bitmap;
import android.graphics.Point;
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

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Manual;
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
@Batch(Batch.PER_CLASS)
public class LongScreenshotsMediatorTest {
    /** Some screenshot dimension that's supposed to be reasonable. */
    /**
     * The largest screen dimension that will be accepted by Android in a View. This is evidently
     * due to an Android total bytes limit of 100M bytes. Anything the size of a 5000x5000 will
     * scale.
     */
    private static final int MAX_ALLOWABLE_SCREENSHOT_DIMENSION = 4999;

    private Activity mActivity;
    private Bitmap mBitmap;
    private LongScreenshotsMediator mMediator;

    @Rule
    public BaseActivityTestRule<BlankUiTestActivity> mActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    @Mock private View mView;

    @Mock private EntryManager mManager;

    @Mock private LongScreenshotsEntry mLongScreenshotsEntry;

    @Captor private ArgumentCaptor<BitmapGeneratorObserver> mBitmapGeneratorObserverCaptor;
    @Captor private ArgumentCaptor<Bitmap> mBitmapCaptor;

    @Captor private ArgumentCaptor<LongScreenshotsEntry.EntryListener> mEntryListenerCaptor;

    @Before
    public void setUp() {
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
        // Boilerplate
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mMediator.showAreaSelectionDialog(mBitmap);
                    Assert.assertTrue(mMediator.getDialog().isShowing());

                    // Test Done function
                    mMediator.areaSelectionDone(mView);
                });

        Assert.assertFalse(mMediator.getDialog().isShowing());
    }

    @Test
    @MediumTest
    public void testAreaSelectionClose() {
        // Boilerplate
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mMediator.showAreaSelectionDialog(mBitmap);
                    Assert.assertTrue(mMediator.getDialog().isShowing());

                    // Test Close function
                    mMediator.areaSelectionClose(mView);
                });

        Assert.assertFalse(mMediator.getDialog().isShowing());
    }

    @Test
    @MediumTest
    public void testOnStatusChange() {
        mMediator.displayInitialScreenshot();
        verify(mManager).addBitmapGeneratorObserver(mBitmapGeneratorObserverCaptor.capture());
        BitmapGeneratorObserver generatorObserver = mBitmapGeneratorObserverCaptor.getValue();
        // Now we can exercise BitmapGeneratorObserver#onStatusChange using the captured generator
        // that was constructed inside of displayInitialScreenShot.
        generatorObserver.onStatusChange(EntryStatus.CAPTURE_IN_PROGRESS);
        generatorObserver.onStatusChange(EntryStatus.BITMAP_GENERATION_IN_PROGRESS);
    }

    @Test
    @MediumTest
    public void testOnCompositorReady_LargeBitmap() {
        // Max value that does not trigger scaling.
        int bitmapDimension = MAX_ALLOWABLE_SCREENSHOT_DIMENSION;

        // Boilerplate - check that we can construct a large bitmap of the given size and show it in
        // the dialog.
        Bitmap someBitmap =
                Bitmap.createBitmap(bitmapDimension, bitmapDimension, Bitmap.Config.ARGB_8888);
        mMediator.displayInitialScreenshot();
        verify(mManager).addBitmapGeneratorObserver(mBitmapGeneratorObserverCaptor.capture());
        BitmapGeneratorObserver generatorObserver = mBitmapGeneratorObserverCaptor.getValue();
        // Now we can call the onCompositorReady and capture it's EntryListener.
        when(mManager.generateFullpageEntry()).thenReturn(mLongScreenshotsEntry);
        when(mLongScreenshotsEntry.getBitmap()).thenReturn(someBitmap);
        generatorObserver.onCompositorReady(
                new Size(bitmapDimension, bitmapDimension), new Point(0, 0));
        verify(mLongScreenshotsEntry).setListener(mEntryListenerCaptor.capture());
        EntryListener entryListener = mEntryListenerCaptor.getValue();
        // Now we can call the onResult method of the EntryListener.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    // This should trigger showAreaSelectionDialog to show the generated bitmap.
                    entryListener.onResult(EntryStatus.BITMAP_GENERATED);
                    Assert.assertTrue(mMediator.getDialog().isShowing());
                });

        Assert.assertFalse(mMediator.getDidScaleForTesting());
    }

    @Test
    @MediumTest
    @DisabledTest(message = "https://crbug.com/1275758")
    public void testOnCompositorReady_VeryLargeBitmap_Scales() {
        // Very large size should trigger scaling.
        int bitmapDimension = 1 + MAX_ALLOWABLE_SCREENSHOT_DIMENSION;

        // Boilerplate - check that we can construct a bitmap of the given size and show it in the
        // dialog.
        Bitmap someBitmap =
                Bitmap.createBitmap(bitmapDimension, bitmapDimension, Bitmap.Config.ARGB_8888);
        mMediator.displayInitialScreenshot();
        verify(mManager).addBitmapGeneratorObserver(mBitmapGeneratorObserverCaptor.capture());
        BitmapGeneratorObserver generatorObserver = mBitmapGeneratorObserverCaptor.getValue();
        // Now we can call the onCompositorReady and capture it's EntryListener.
        when(mManager.generateFullpageEntry()).thenReturn(mLongScreenshotsEntry);
        when(mLongScreenshotsEntry.getBitmap()).thenReturn(someBitmap);
        generatorObserver.onCompositorReady(
                new Size(bitmapDimension, bitmapDimension), new Point(0, 0));
        verify(mLongScreenshotsEntry).setListener(mEntryListenerCaptor.capture());
        EntryListener entryListener = mEntryListenerCaptor.getValue();
        // Now we can call the onResult method of the EntryListener.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    // This should trigger showAreaSelectionDialog to show the scaled bitmap.
                    entryListener.onResult(EntryStatus.BITMAP_GENERATED);
                    Assert.assertTrue(mMediator.getDialog().isShowing());
                });

        Assert.assertTrue(mMediator.getDidScaleForTesting());
    }

    /**
     * Use this manual test to investigate Android limit changes. A better option is to look at
     * Android Android RecordingCanvas#getPanelFrameSize if possible.
     */
    @Test
    @MediumTest
    @Manual(message = "Works with the screen unlocked on a phone, maybe not on bots.")
    public void testOnCompositorReady_VeryLargeBitmap_Throws_Manual() {
        // Usage: Manually disable the scaling in the mediator to validate Android's Canvas limit.
        // Run this test on a phone with the screen unlocked so the View will actually be drawn.
        // If the test passes that means Android did throw with the given bitmapDimension.
        // 5200 has been empirically verified using this test to be beyond the Android size limit
        // w/o scaling, so it will throw an exception on U (and probably mose earlier releases).
        int bitmapDimension = 200 + MAX_ALLOWABLE_SCREENSHOT_DIMENSION;

        // Boilerplate - check that we can construct a bitmap of the given size and show it in the
        // dialog.
        Bitmap someBitmap =
                Bitmap.createBitmap(bitmapDimension, bitmapDimension, Bitmap.Config.ARGB_8888);
        mMediator.displayInitialScreenshot();
        verify(mManager).addBitmapGeneratorObserver(mBitmapGeneratorObserverCaptor.capture());
        BitmapGeneratorObserver generatorObserver = mBitmapGeneratorObserverCaptor.getValue();
        // Now we can call the onCompositorReady and capture it's EntryListener.
        when(mManager.generateFullpageEntry()).thenReturn(mLongScreenshotsEntry);
        when(mLongScreenshotsEntry.getBitmap()).thenReturn(someBitmap);
        generatorObserver.onCompositorReady(
                new Size(bitmapDimension, bitmapDimension), new Point(0, 0));
        verify(mLongScreenshotsEntry).setListener(mEntryListenerCaptor.capture());
        EntryListener entryListener = mEntryListenerCaptor.getValue();

        // Now we can call the onResult method of the EntryListener.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    // This should trigger showAreaSelectionDialog to show the scaled bitmap.
                    entryListener.onResult(EntryStatus.BITMAP_GENERATED);
                });

        // When the dialog is shown an exception should be thrown due to Canvas size limits.
        Exception exception =
                Assert.assertThrows(
                        RuntimeException.class,
                        () -> {
                            ThreadUtils.runOnUiThreadBlocking(
                                    () -> {
                                        Assert.assertTrue(mMediator.getDialog().isShowing());
                                    });
                        });
        String expectedMessage = "trying to draw too large";
        String actualMessage = exception.getMessage();
        Assert.assertTrue(
                "Check that the screen is on and not locked when running the test! "
                        + "Expected Canvas error: Android limits may have changed "
                        + "on this platform.",
                actualMessage.contains(expectedMessage));
    }
}

// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.photo_picker;

import android.content.Context;
import android.graphics.Bitmap;
import android.net.Uri;
import android.os.Build;
import android.support.test.filters.LargeTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;

import java.io.File;
import java.util.List;
import java.util.concurrent.TimeUnit;

/**
 * Tests for the DecoderServiceHost.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class DecoderServiceHostTest implements DecoderServiceHost.ServiceReadyCallback,
                                               DecoderServiceHost.ImagesDecodedCallback {
    // The timeout (in seconds) to wait for the decoding.
    private static final long WAIT_TIMEOUT_SECONDS = 5L;

    @Rule
    public ChromeActivityTestRule<ChromeActivity> mActivityTestRule =
            new ChromeActivityTestRule<>(ChromeActivity.class);

    private Context mContext;

    // A callback that fires when the decoder is ready.
    public final CallbackHelper onDecoderReadyCallback = new CallbackHelper();

    // A callback that fires when something is finished decoding in the dialog.
    public final CallbackHelper onDecodedCallback = new CallbackHelper();

    private String mLastDecodedPath;
    private boolean mLastIsVideo;
    private int mLastFrameCount;
    private String mLastVideoDuration;

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.startMainActivityOnBlankPage();
        mContext = mActivityTestRule.getActivity();

        DecoderServiceHost.setReadyCallback(this);
    }

    // DecoderServiceHost.ServiceReadyCallback:

    @Override
    public void serviceReady() {
        onDecoderReadyCallback.notifyCalled();
    }

    // DecoderServiceHost.ImagesDecodedCallback:

    @Override
    public void imagesDecodedCallback(
            String filePath, boolean isVideo, List<Bitmap> bitmaps, String videoDuration) {
        mLastDecodedPath = filePath;
        mLastIsVideo = isVideo;
        mLastFrameCount = bitmaps != null ? bitmaps.size() : -1;
        mLastVideoDuration = videoDuration;

        onDecodedCallback.notifyCalled();
    }

    private void waitForDecoder() throws Exception {
        int callCount = onDecoderReadyCallback.getCallCount();
        onDecoderReadyCallback.waitForCallback(
                callCount, 1, WAIT_TIMEOUT_SECONDS, TimeUnit.SECONDS);
    }

    private void waitForThumbnailDecode() throws Exception {
        int callCount = onDecodedCallback.getCallCount();
        onDecodedCallback.waitForCallback(callCount, 1, WAIT_TIMEOUT_SECONDS, TimeUnit.SECONDS);
    }

    @Test
    @DisableIf.Build(sdk_is_less_than = Build.VERSION_CODES.N)
    @LargeTest
    public void testDecodingOrder() throws Throwable {
        DecoderServiceHost host = new DecoderServiceHost(this, mContext);
        host.bind(mContext);
        waitForDecoder();

        String fileName1 = "noogler.mp4";
        String fileName2 = "noogler2.mp4";
        String fileName3 = "blue100x100.jpg";
        String filePath = "chrome/test/data/android/photo_picker/";
        File file1 = new File(UrlUtils.getIsolatedTestFilePath(filePath + fileName1));
        File file2 = new File(UrlUtils.getIsolatedTestFilePath(filePath + fileName2));
        File file3 = new File(UrlUtils.getIsolatedTestFilePath(filePath + fileName3));

        host.decodeImage(Uri.fromFile(file1), PickerBitmap.TileTypes.VIDEO, 10, this);
        host.decodeImage(Uri.fromFile(file2), PickerBitmap.TileTypes.VIDEO, 10, this);
        host.decodeImage(Uri.fromFile(file3), PickerBitmap.TileTypes.PICTURE, 10, this);

        // First decoding result should be first frame only of video 1.
        waitForThumbnailDecode();
        Assert.assertTrue(mLastDecodedPath.contains(fileName1));
        Assert.assertEquals(true, mLastIsVideo);
        Assert.assertEquals("00:00", mLastVideoDuration);
        Assert.assertEquals(1, mLastFrameCount);

        // Second decoding result is first frame of video 2, because that's higher priority than the
        // rest of video 1.
        waitForThumbnailDecode();
        Assert.assertTrue(mLastDecodedPath.contains(fileName2));
        Assert.assertEquals(true, mLastIsVideo);
        Assert.assertEquals("00:00", mLastVideoDuration);
        Assert.assertEquals(1, mLastFrameCount);

        // Third in line should be the jpg file.
        waitForThumbnailDecode();
        Assert.assertTrue(mLastDecodedPath.contains(fileName3));
        Assert.assertEquals(false, mLastIsVideo);
        Assert.assertEquals(null, mLastVideoDuration);
        Assert.assertEquals(1, mLastFrameCount);

        // Remaining frames of video 1.
        waitForThumbnailDecode();
        Assert.assertTrue(mLastDecodedPath.contains(fileName1));
        Assert.assertEquals(true, mLastIsVideo);
        Assert.assertEquals("00:00", mLastVideoDuration);
        Assert.assertEquals(10, mLastFrameCount);

        // Remaining frames of video 2.
        waitForThumbnailDecode();
        Assert.assertTrue(mLastDecodedPath.contains(fileName2));
        Assert.assertEquals(true, mLastIsVideo);
        Assert.assertEquals("00:00", mLastVideoDuration);
        Assert.assertEquals(10, mLastFrameCount);

        host.unbind(mContext);
    }
}

// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.photo_picker;

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.ServiceConnection;
import android.graphics.Bitmap;
import android.os.Build;
import android.os.Bundle;
import android.os.IBinder;
import android.os.ParcelFileDescriptor;
import android.support.test.filters.LargeTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;

import java.io.File;
import java.io.FileDescriptor;
import java.io.FileInputStream;

/**
 * Tests for the out-of-process DecoderService.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class DecoderServiceTest {
    @Rule
    public ChromeActivityTestRule<ChromeActivity> mActivityTestRule =
            new ChromeActivityTestRule<>(ChromeActivity.class);

    Context mContext;

    // Flag indicating whether we are bound to the service.
    private boolean mBound;

    private class DecoderServiceCallback extends IDecoderServiceCallback.Stub {
        // The returned bundle from the decoder.
        private Bundle mDecodedBundle;

        public boolean resolved() {
            return mDecodedBundle != null;
        }

        public Bundle getBundle() {
            return mDecodedBundle;
        }

        @Override
        public void onDecodeImageDone(final Bundle payload) {
            mDecodedBundle = payload;
        }
    }

    IDecoderService mIRemoteService;
    private ServiceConnection mConnection = new ServiceConnection() {
        @Override
        public void onServiceConnected(ComponentName className, IBinder service) {
            mIRemoteService = IDecoderService.Stub.asInterface(service);
            mBound = true;
        }

        @Override
        public void onServiceDisconnected(ComponentName className) {
            mIRemoteService = null;
            mBound = false;
        }
    };

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.startMainActivityOnBlankPage();
        mContext = mActivityTestRule.getActivity();
    }

    private void startDecoderService() {
        Intent intent = new Intent(mContext, DecoderService.class);
        intent.setAction(IDecoderService.class.getName());
        mContext.bindService(intent, mConnection, Context.BIND_AUTO_CREATE);

        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return mBound;
            }
        });
    }

    private void decode(String filePath, FileDescriptor fd, int size,
            final DecoderServiceCallback callback) throws Exception {
        Bundle bundle = new Bundle();
        bundle.putString(DecoderService.KEY_FILE_PATH, filePath);
        ParcelFileDescriptor pfd = null;
        if (fd != null) {
            pfd = ParcelFileDescriptor.dup(fd);
            Assert.assertTrue(pfd != null);
        }
        bundle.putParcelable(DecoderService.KEY_FILE_DESCRIPTOR, pfd);
        bundle.putInt(DecoderService.KEY_SIZE, size);

        mIRemoteService.decodeImage(bundle, callback);
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return callback.resolved();
            }
        });
    }

    @Test
    @DisableIf.Build(sdk_is_less_than = Build.VERSION_CODES.LOLLIPOP, message = "crbug.com/888931")
    @LargeTest
    public void testServiceDecodeNullFileDescriptor() throws Throwable {
        startDecoderService();

        // Attempt to decode to a 50x50 thumbnail without a valid FileDescriptor (null).
        DecoderServiceCallback callback = new DecoderServiceCallback();
        decode("path", null, 50, callback);

        Bundle bundle = callback.getBundle();
        Assert.assertFalse(
                "Expected decode to fail", bundle.getBoolean(DecoderService.KEY_SUCCESS));
        Assert.assertEquals("path", bundle.getString(DecoderService.KEY_FILE_PATH));
        Assert.assertEquals(null, bundle.getParcelable(DecoderService.KEY_IMAGE_BITMAP));
        Assert.assertEquals(0, bundle.getLong(DecoderService.KEY_DECODE_TIME));
    }

    @Test
    @DisableIf.Build(sdk_is_less_than = Build.VERSION_CODES.LOLLIPOP, message = "crbug.com/888931")
    @LargeTest
    public void testServiceDecodeSimple() throws Exception {
        startDecoderService();

        File file = new File(UrlUtils.getIsolatedTestFilePath(
                "chrome/test/data/android/photo_picker/blue100x100.jpg"));
        FileInputStream inStream = new FileInputStream(file);

        // Attempt to decode a valid 100x100 image file to a 50x50 thumbnail.
        DecoderServiceCallback callback = new DecoderServiceCallback();
        decode(file.getPath(), inStream.getFD(), 50, callback);

        Bundle bundle = callback.getBundle();
        Assert.assertTrue(
                "Expecting success being returned", bundle.getBoolean(DecoderService.KEY_SUCCESS));
        Assert.assertEquals(file.getPath(), bundle.getString(DecoderService.KEY_FILE_PATH));
        Assert.assertFalse("Decoding should take a non-zero amount of time",
                0 == bundle.getLong(DecoderService.KEY_DECODE_TIME));

        Bitmap decodedBitmap = bundle.getParcelable(DecoderService.KEY_IMAGE_BITMAP);
        Assert.assertFalse("Decoded bitmap should not be null", null == decodedBitmap);
        Assert.assertEquals(50, decodedBitmap.getWidth());
        Assert.assertEquals(50, decodedBitmap.getHeight());
    }
}

// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.app.Activity;
import android.content.Context;
import android.content.pm.PackageManager;
import android.graphics.Bitmap;
import android.os.Build;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.CallbackUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.permissions.PermissionCallback;

/** Tests for the {@link SaveBitmapDelegate}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(sdk = Build.VERSION_CODES.S_V2)
public class SaveBitmapDelegateTest {
    private SaveBitmapDelegate mSaveBitmapDelegate;
    private TestWindowAndroid mPermissionDelegate;
    private boolean mBitmapSaved;

    @Before
    public void setUp() {
        Activity activity = Robolectric.buildActivity(TestActivity.class).setup().get();
        mPermissionDelegate = new TestWindowAndroid(activity);
        Bitmap bitmap = Bitmap.createBitmap(10, 10, Bitmap.Config.ALPHA_8);
        mSaveBitmapDelegate =
                new SaveBitmapDelegate(
                        activity,
                        bitmap,
                        R.string.screenshot_filename_prefix,
                        CallbackUtils.emptyRunnable(),
                        mPermissionDelegate) {
                    @Override
                    protected void finishDownloadWithPermission(boolean granted) {
                        mBitmapSaved = true;
                    }
                };
    }

    @After
    public void tearDown() {
        mPermissionDelegate.destroy();
    }

    @Test
    public void testSaveWithPermission() {
        mPermissionDelegate.setHasPermission(true);
        mSaveBitmapDelegate.save();

        assertTrue(mPermissionDelegate.calledHasPermission());
        assertFalse(mPermissionDelegate.calledCanRequestPermission());
        assertTrue(mBitmapSaved);
    }

    @Test
    public void testSaveWithoutPermissionCanNotAsk() {
        mPermissionDelegate.setHasPermission(false);
        mPermissionDelegate.setCanRequestPermission(false);
        mSaveBitmapDelegate.save();

        assertTrue(mPermissionDelegate.calledHasPermission());
        assertTrue(mPermissionDelegate.calledCanRequestPermission());
        assertTrue(mSaveBitmapDelegate.getDialog().isShowing());
        assertFalse(mBitmapSaved);
    }

    /** Test implementation of {@link WindowAndroid}. */
    private static class TestWindowAndroid extends WindowAndroid {
        private final int mPermissionResult = PackageManager.PERMISSION_GRANTED;

        private boolean mHasPermission;
        private boolean mCanRequestPermission;
        private boolean mCalledHasPermission;
        private boolean mCalledCanRequestPermission;

        public TestWindowAndroid(Context context) {
            super(context, /* occlusionTrackingAllowed= */ true);
        }

        public void setHasPermission(boolean hasPermission) {
            mHasPermission = hasPermission;
        }

        public void setCanRequestPermission(boolean canRequestPermission) {
            mCanRequestPermission = canRequestPermission;
        }

        public boolean calledHasPermission() {
            return mCalledHasPermission;
        }

        public boolean calledCanRequestPermission() {
            return mCalledCanRequestPermission;
        }

        @Override
        public boolean hasPermission(String permission) {
            mCalledHasPermission = true;
            return mHasPermission;
        }

        @Override
        public boolean canRequestPermission(String permission) {
            mCalledCanRequestPermission = true;
            return mCanRequestPermission;
        }

        @Override
        public boolean isPermissionRevokedByPolicy(String permission) {
            return false;
        }

        @Override
        public void requestPermissions(String[] permissions, PermissionCallback callback) {
            int[] results = new int[permissions.length];
            for (int i = 0; i < permissions.length; i++) {
                results[i] = mPermissionResult;
            }
            callback.onRequestPermissionsResult(permissions, results);
        }

        @Override
        public boolean handlePermissionResult(
                int requestCode, String[] permissions, int[] grantResults) {
            return false;
        }
    }
}

// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share;

import android.app.Activity;
import android.content.Context;
import android.content.pm.PackageManager;
import android.graphics.Bitmap;
import android.os.Build;

import androidx.test.annotation.UiThreadTest;
import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.util.MaxAndroidSdkLevel;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.R;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.permissions.PermissionCallback;
import org.chromium.ui.test.util.BlankUiTestActivity;

/** Tests for the {@link SaveBitmapDelegate}. */
@RunWith(ChromeJUnit4ClassRunner.class)
public class SaveBitmapDelegateTest {
    private SaveBitmapDelegate mSaveBitmapDelegate;

    @Rule
    public BaseActivityTestRule<BlankUiTestActivity> mActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    @Mock private Runnable mCloseDialogRunnable;

    private TestWindowAndroid mPermissionDelegate;

    private boolean mBitmapSaved;

    @Before
    public void setUp() throws Exception {
        MockitoAnnotations.initMocks(this);

        mActivityTestRule.launchActivity(null);
        Activity activity = mActivityTestRule.getActivity();
        mPermissionDelegate =
                ThreadUtils.runOnUiThreadBlocking(() -> new TestWindowAndroid(activity));
        Bitmap bitmap = Bitmap.createBitmap(10, 10, Bitmap.Config.ALPHA_8);
        mSaveBitmapDelegate =
                new SaveBitmapDelegate(
                        activity,
                        bitmap,
                        R.string.screenshot_filename_prefix,
                        mCloseDialogRunnable,
                        mPermissionDelegate) {
                    @Override
                    protected void finishDownloadWithPermission(boolean granted) {
                        mBitmapSaved = true;
                    }
                };
    }

    @After
    public void tearDown() {
        ThreadUtils.runOnUiThreadBlocking(() -> mPermissionDelegate.destroy());
    }

    @Test
    @MediumTest
    @UiThreadTest
    @MaxAndroidSdkLevel(
            value = Build.VERSION_CODES.S_V2,
            reason = "Permission request is not made on T+")
    public void testSaveWithPermission() {
        mPermissionDelegate.setHasPermission(true);
        mSaveBitmapDelegate.save();

        Assert.assertTrue(mPermissionDelegate.calledHasPermission());
        Assert.assertFalse(mPermissionDelegate.calledCanRequestPermission());
        Assert.assertTrue(mBitmapSaved);
    }

    @Test
    @MediumTest
    @UiThreadTest
    @MaxAndroidSdkLevel(
            value = Build.VERSION_CODES.S_V2,
            reason = "Permission request is not made on T+")
    public void testSaveWithoutPermissionCanNotAsk() {
        mPermissionDelegate.setHasPermission(false);
        mPermissionDelegate.setCanRequestPermission(false);
        mSaveBitmapDelegate.save();

        Assert.assertTrue(mPermissionDelegate.calledHasPermission());
        Assert.assertTrue(mPermissionDelegate.calledCanRequestPermission());
        Assert.assertTrue(mSaveBitmapDelegate.getDialog().isShowing());
        Assert.assertFalse(mBitmapSaved);
    }

    /** Test implementation of {@link WindowAndroid}. */
    private static class TestWindowAndroid extends WindowAndroid {
        private boolean mHasPermission;
        private boolean mCanRequestPermission;

        private boolean mCalledHasPermission;
        private boolean mCalledCanRequestPermission;
        private int mPermissionResult = PackageManager.PERMISSION_GRANTED;

        public TestWindowAndroid(Context context) {
            super(context);
        }

        public void setPermissionResults(int result) {
            mPermissionResult = result;
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

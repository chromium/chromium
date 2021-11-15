// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share;

import android.Manifest.permission;
import android.app.Activity;
import android.content.pm.PackageManager;
import android.graphics.Bitmap;

import androidx.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.UiThreadTest;
import org.chromium.chrome.R;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.ui.base.AndroidPermissionDelegate;
import org.chromium.ui.base.PermissionCallback;
import org.chromium.ui.test.util.DummyUiActivity;

/**
 * Tests for the {@link SaveBitmapDelegate}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
public class SaveBitmapDelegateTest {
    private SaveBitmapDelegate mSaveBitmapDelegate;

    @Rule
    public BaseActivityTestRule<DummyUiActivity> mActivityTestRule =
            new BaseActivityTestRule<>(DummyUiActivity.class);

    @Mock
    private Runnable mCloseDialogRunnable;

    private TestAndroidPermissionDelegate mPermissionDelegate;

    private boolean mBitmapSaved;

    @Before
    public void setUp() throws Exception {
        MockitoAnnotations.initMocks(this);

        mActivityTestRule.launchActivity(null);
        Activity activity = mActivityTestRule.getActivity();
        mPermissionDelegate = new TestAndroidPermissionDelegate();
        Bitmap bitmap = Bitmap.createBitmap(10, 10, Bitmap.Config.ALPHA_8);
        mSaveBitmapDelegate = new SaveBitmapDelegate(activity, bitmap,
                R.string.screenshot_filename_prefix, mCloseDialogRunnable, mPermissionDelegate) {
            @Override
            protected void finishDownloadWithPermission(boolean granted) {
                mBitmapSaved = true;
            }
        };
    }

    @Test
    @MediumTest
    @UiThreadTest
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
    public void testSaveWithoutPermissionCanNotAsk() {
        mPermissionDelegate.setHasPermission(false);
        mPermissionDelegate.setCanRequestPermission(false);
        mSaveBitmapDelegate.save();

        Assert.assertTrue(mPermissionDelegate.calledHasPermission());
        Assert.assertTrue(mPermissionDelegate.calledCanRequestPermission());
        Assert.assertTrue(mSaveBitmapDelegate.getDialog().isShowing());
        Assert.assertFalse(mBitmapSaved);
    }

    /**
     * Test implementation of {@link AndroidPermissionDelegate}.
     */
    private class TestAndroidPermissionDelegate implements AndroidPermissionDelegate {
        private boolean mHasPermission;
        private boolean mCanRequestPermission;

        private boolean mCalledHasPermission;
        private boolean mCalledCanRequestPermission;
        private int mPermissionResult = PackageManager.PERMISSION_GRANTED;

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

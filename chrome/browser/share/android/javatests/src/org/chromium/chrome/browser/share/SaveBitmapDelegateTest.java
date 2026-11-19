// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.content.Context;
import android.content.pm.PackageManager;
import android.graphics.Bitmap;
import android.os.Build;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.CallbackUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.permissions.PermissionCallback;

/** Tests for the {@link SaveBitmapDelegate}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(sdk = Build.VERSION_CODES.S_V2)
public class SaveBitmapDelegateTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Profile mProfile;
    @Mock private Profile mOtrProfile;
    @Mock private BitmapDownloadRequest.Natives mBitmapDownloadRequestJni;

    private Activity mActivity;
    private Bitmap mBitmap;
    private SaveBitmapDelegate mSaveBitmapDelegate;
    private TestWindowAndroid mPermissionDelegate;
    private boolean mBitmapSaved;

    @Before
    public void setUp() {
        BitmapDownloadRequestJni.setInstanceForTesting(mBitmapDownloadRequestJni);
        mActivity = Robolectric.buildActivity(TestActivity.class).setup().get();
        mPermissionDelegate = new TestWindowAndroid(mActivity);
        mBitmap = Bitmap.createBitmap(10, 10, Bitmap.Config.ALPHA_8);
        mSaveBitmapDelegate =
                new SaveBitmapDelegate(
                        mActivity,
                        mBitmap,
                        R.string.screenshot_filename_prefix,
                        CallbackUtils.emptyRunnable(),
                        mPermissionDelegate,
                        mProfile) {
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

    @Test
    public void testDownloadPassesRegularProfile() {
        mPermissionDelegate.setHasPermission(true);
        boolean[] callbackRan = {false};
        SaveBitmapDelegate delegate =
                new SaveBitmapDelegate(
                        mActivity,
                        mBitmap,
                        R.string.screenshot_filename_prefix,
                        () -> callbackRan[0] = true,
                        mPermissionDelegate,
                        mProfile);
        delegate.save();

        verify(mBitmapDownloadRequestJni).downloadBitmap(eq(mProfile), anyString(), eq(mBitmap));
        assertTrue(callbackRan[0]);
    }

    @Test
    public void testDownloadPassesOffTheRecordProfile() {
        mPermissionDelegate.setHasPermission(true);
        SaveBitmapDelegate delegate =
                new SaveBitmapDelegate(
                        mActivity,
                        mBitmap,
                        R.string.screenshot_filename_prefix,
                        CallbackUtils.emptyRunnable(),
                        mPermissionDelegate,
                        mOtrProfile);
        delegate.save();

        verify(mBitmapDownloadRequestJni).downloadBitmap(eq(mOtrProfile), anyString(), eq(mBitmap));
    }

    @Test
    public void testDownloadSkipsWhenProfileShutdownStarted() {
        mPermissionDelegate.setHasPermission(true);
        when(mOtrProfile.shutdownStarted()).thenReturn(true);
        boolean[] callbackRan = {false};
        SaveBitmapDelegate delegate =
                new SaveBitmapDelegate(
                        mActivity,
                        mBitmap,
                        R.string.screenshot_filename_prefix,
                        () -> callbackRan[0] = true,
                        mPermissionDelegate,
                        mOtrProfile);
        delegate.save();

        verify(mBitmapDownloadRequestJni, never()).downloadBitmap(any(), any(), any());
        assertFalse(callbackRan[0]);
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

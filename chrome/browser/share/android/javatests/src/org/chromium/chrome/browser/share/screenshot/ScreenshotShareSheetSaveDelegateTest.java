// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.screenshot;

import static org.mockito.ArgumentMatchers.eq;

import android.Manifest.permission;
import android.app.Activity;
import android.content.pm.PackageManager;
import android.graphics.Bitmap;
import android.support.test.rule.ActivityTestRule;

import androidx.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.UiThreadTest;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.ui.base.AndroidPermissionDelegate;
import org.chromium.ui.base.PermissionCallback;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.test.util.DummyUiActivity;

/**
 * Tests for the {@link ScreenshotShareSheetView}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@Features.EnableFeatures(ChromeFeatureList.CHROME_SHARE_SCREENSHOT)
public class ScreenshotShareSheetSaveDelegateTest {
    private ScreenshotShareSheetSaveDelegate mScreenshotShareSheetSaveDelegate;

    @Rule
    public ActivityTestRule<DummyUiActivity> mActivityTestRule =
            new ActivityTestRule<>(DummyUiActivity.class);

    @Mock
    private PropertyModel mModel;

    @Mock
    private Runnable mCloseDialogRunnable;

    private TestAndroidPermissionDelegate mPermissionDelegate;

    @Before
    public void setUp() throws Exception {
        MockitoAnnotations.initMocks(this);

        Mockito.when(mModel.get(eq(ScreenshotShareSheetViewProperties.SCREENSHOT_BITMAP)))
                .thenReturn(Bitmap.createBitmap(10, 10, Bitmap.Config.ALPHA_8));

        Activity activity = mActivityTestRule.getActivity();
        mPermissionDelegate = new TestAndroidPermissionDelegate();
        mScreenshotShareSheetSaveDelegate = new ScreenshotShareSheetSaveDelegate(
                activity, mModel, mCloseDialogRunnable, mPermissionDelegate);
    }

    @Test
    @MediumTest
    @UiThreadTest
    public void testSaveWithPermission() {
        mPermissionDelegate.setHasPermission(true);
        mScreenshotShareSheetSaveDelegate.save();

        Assert.assertTrue(mPermissionDelegate.calledHasPermission());
        Assert.assertFalse(mPermissionDelegate.calledCanRequestPermission());
    }

    @Test
    @MediumTest
    @UiThreadTest
    public void testSaveWithoutPermissionCanAsk() {
        mPermissionDelegate.setHasPermission(false);
        mPermissionDelegate.setCanRequestPermission(true);
        mScreenshotShareSheetSaveDelegate.save();

        Assert.assertTrue(mPermissionDelegate.calledHasPermission());
        Assert.assertTrue(mPermissionDelegate.calledCanRequestPermission());
    }

    @Test
    @MediumTest
    @UiThreadTest
    public void testSaveWithoutPermissionCanNotAsk() {
        mPermissionDelegate.setHasPermission(false);
        mPermissionDelegate.setCanRequestPermission(false);
        mScreenshotShareSheetSaveDelegate.save();

        Assert.assertTrue(mPermissionDelegate.calledHasPermission());
        Assert.assertTrue(mPermissionDelegate.calledCanRequestPermission());
        Assert.assertTrue(mScreenshotShareSheetSaveDelegate.getDialog().isShowing());
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

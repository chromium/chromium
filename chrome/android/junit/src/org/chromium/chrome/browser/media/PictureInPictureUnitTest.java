// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.AppOpsManager;
import android.app.AppOpsManager.OnOpChangedListener;
import android.content.Context;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.build.annotations.NullMarked;

/** Unit tests for {@link PictureInPicture}. */
@RunWith(BaseRobolectricTestRunner.class)
@NullMarked
public class PictureInPictureUnitTest {
    private static final int TEST_UID = 10123;
    private static final String TEST_PACKAGE = "org.chromium.chrome";

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Context mContext;
    @Mock private Context mAppContext;
    @Mock private PackageManager mPackageManager;
    @Mock private AppOpsManager mAppOpsManager;
    @Captor private ArgumentCaptor<OnOpChangedListener> mListenerCaptor;

    private ApplicationInfo mApplicationInfo;

    @Before
    public void setUp() {
        mApplicationInfo = new ApplicationInfo();
        mApplicationInfo.uid = TEST_UID;

        when(mContext.getApplicationContext()).thenReturn(mAppContext);
        when(mContext.getPackageManager()).thenReturn(mPackageManager);
        when(mPackageManager.hasSystemFeature(PackageManager.FEATURE_PICTURE_IN_PICTURE))
                .thenReturn(true);
        when(mAppContext.getSystemService(Context.APP_OPS_SERVICE)).thenReturn(mAppOpsManager);
        when(mAppContext.getApplicationInfo()).thenReturn(mApplicationInfo);
        when(mAppContext.getPackageName()).thenReturn(TEST_PACKAGE);
    }

    @Test
    public void testIsEnabled_cachesResult() {
        when(mAppOpsManager.checkOpNoThrow(
                        AppOpsManager.OPSTR_PICTURE_IN_PICTURE, TEST_UID, TEST_PACKAGE))
                .thenReturn(AppOpsManager.MODE_ALLOWED);

        assertTrue(PictureInPicture.isEnabled(mContext));
        assertTrue(PictureInPicture.isEnabled(mContext));

        verify(mAppOpsManager, times(1))
                .checkOpNoThrow(AppOpsManager.OPSTR_PICTURE_IN_PICTURE, TEST_UID, TEST_PACKAGE);
    }

    @Test
    public void testIsEnabled_onOpChangedInvalidatesCache() {
        when(mAppOpsManager.checkOpNoThrow(
                        AppOpsManager.OPSTR_PICTURE_IN_PICTURE, TEST_UID, TEST_PACKAGE))
                .thenReturn(AppOpsManager.MODE_ALLOWED);

        assertTrue(PictureInPicture.isEnabled(mContext));
        verify(mAppOpsManager)
                .startWatchingMode(
                        eq(AppOpsManager.OPSTR_PICTURE_IN_PICTURE),
                        eq(TEST_PACKAGE),
                        mListenerCaptor.capture());

        when(mAppOpsManager.checkOpNoThrow(
                        AppOpsManager.OPSTR_PICTURE_IN_PICTURE, TEST_UID, TEST_PACKAGE))
                .thenReturn(AppOpsManager.MODE_IGNORED);
        mListenerCaptor
                .getValue()
                .onOpChanged(AppOpsManager.OPSTR_PICTURE_IN_PICTURE, TEST_PACKAGE);
        ShadowLooper.idleMainLooper();

        assertFalse(PictureInPicture.isEnabled(mContext));
        verify(mAppOpsManager, times(2))
                .checkOpNoThrow(AppOpsManager.OPSTR_PICTURE_IN_PICTURE, TEST_UID, TEST_PACKAGE);
    }
}

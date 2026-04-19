// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.ArgumentMatchers.isNull;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoInteractions;
import static org.mockito.Mockito.when;

import android.app.ActivityManager.AppTask;
import android.graphics.Rect;
import android.os.Build;
import android.view.WindowManager;
import android.view.WindowMetrics;
import android.widget.FrameLayout;

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

import org.chromium.base.AconfigFlaggedApiDelegate;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.util.AndroidTaskUtils;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.ActivityWindowAndroid;
import org.chromium.ui.display.DisplayAndroid;
import org.chromium.ui.display.DisplayAndroidManager;

/** Unit tests for {@link DocumentPictureInPictureActivity}. */
@RunWith(BaseRobolectricTestRunner.class)
public class DocumentPictureInPictureActivityUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private ActivityWindowAndroid mActivityWindowAndroid;
    @Mock private DisplayAndroid mDisplayAndroid;
    @Mock private PictureInPictureBoundsCacheBridge.Natives mMockNatives;
    @Mock private WindowManager mWindowManager;
    @Mock private WindowMetrics mWindowMetrics;
    @Mock private FrameLayout mContentLayout;
    @Mock private AconfigFlaggedApiDelegate mAconfigFlaggedApiDelegate;
    @Mock private AppTask mAppTask;
    @Mock private DisplayAndroidManager mDisplayAndroidManager;

    private DocumentPictureInPictureActivity mActivity;

    @Before
    public void setUp() {
        AconfigFlaggedApiDelegate.setInstanceForTesting(mAconfigFlaggedApiDelegate);
        AndroidTaskUtils.setAppTaskForTesting(mAppTask);
        DisplayAndroidManager.setInstanceForTesting(mDisplayAndroidManager);
        DisplayAndroid.setNonMultiDisplayForTesting(mDisplayAndroid);
        PictureInPictureBoundsCacheBridgeJni.setInstanceForTesting(mMockNatives);

        // Common Display setup
        when(mDisplayAndroid.getDipScale()).thenReturn(1.0f);
        Rect displayBounds = new Rect(0, 0, 1000, 1000);
        when(mDisplayAndroid.getBounds()).thenReturn(displayBounds);
        when(mDisplayAndroid.getLocalBounds()).thenReturn(displayBounds);
        when(mDisplayAndroid.getDisplayId()).thenReturn(0);

        mActivity = spy(Robolectric.buildActivity(DocumentPictureInPictureActivity.class).get());

        doReturn(mActivityWindowAndroid).when(mActivity).getWindowAndroid();
        when(mActivityWindowAndroid.getDisplay()).thenReturn(mDisplayAndroid);

        doReturn(mWindowManager).when(mActivity).getWindowManager();
        when(mWindowManager.getCurrentWindowMetrics()).thenReturn(mWindowMetrics);

        doReturn(mContentLayout)
                .when(mActivity)
                .findViewById(R.id.document_picture_in_picture_content);

        when(mDisplayAndroidManager.getDisplayMatching(any(Rect.class)))
                .thenReturn(mDisplayAndroid);
    }

    @After
    public void tearDown() {
        AconfigFlaggedApiDelegate.setInstanceForTesting(null);
        AndroidTaskUtils.setAppTaskForTesting(null);
        DisplayAndroidManager.resetInstanceForTesting();
        DisplayAndroid.setNonMultiDisplayForTesting(null);
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.S)
    public void testResizeContents() {
        when(mContentLayout.getWidth()).thenReturn(100);
        when(mContentLayout.getHeight()).thenReturn(100);

        int newWidthDp = 150;
        int newHeightDp = 150;

        // Current window bounds in pixels (from WindowMetrics)
        Rect windowBoundsPx = new Rect(100, 100, 300, 300); // 200x200
        when(mWindowMetrics.getBounds()).thenReturn(windowBoundsPx);

        mActivity.resizeContents(newWidthDp, newHeightDp);

        // Expectation:
        // widthDiff = 150 - 100 = 50
        // heightDiff = 150 - 100 = 50
        // currentWindowBounds (Global Dip) = windowBoundsPx (since density 1.0 and origin 0,0) =
        // 100, 100, 300, 300
        // newBounds (Global Dip) = left - 50, top - 50, right, bottom
        //                        = 100 - 50, 100 - 50, 300, 300
        //                        = 50, 50, 300, 300
        // localBounds (Px) = newBounds (Global Dip) (since density 1.0 and origin 0,0) = 50, 50,
        // 300, 300

        verify(mAconfigFlaggedApiDelegate)
                .moveTaskTo(
                        eq(mAppTask),
                        eq(0), // displayId
                        eq(new Rect(50, 50, 300, 300)));
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.S)
    public void testResizeContents_NoChange() {
        when(mContentLayout.getWidth()).thenReturn(100);
        when(mContentLayout.getHeight()).thenReturn(100);

        mActivity.resizeContents(100, 100);

        verifyNoInteractions(mAconfigFlaggedApiDelegate);
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.S)
    public void testSaveBoundsToCache() {
        when(mDisplayAndroid.getDipScale()).thenReturn(2.0f);

        WebContents parentWebContents = mock(WebContents.class);
        AutoPictureInPictureTabHelper helper = mock(AutoPictureInPictureTabHelper.class);
        when(parentWebContents.isDestroyed()).thenReturn(false);
        when(parentWebContents.getOrSetUserData(
                        eq(AutoPictureInPictureTabHelper.USER_DATA_KEY), isNull()))
                .thenReturn(helper);

        mActivity.setParentWebContentsOnInstanceForTesting(parentWebContents);

        final Rect windowBoundsPx = new Rect(100, 100, 300, 300); // 200x200
        when(mWindowMetrics.getBounds()).thenReturn(windowBoundsPx);

        mActivity.saveBoundsToCache();

        verify(mMockNatives)
                .updateCachedBounds(
                        eq(parentWebContents),
                        eq(50), // 100 / 2.0
                        eq(50), // 100 / 2.0
                        eq(150), // 300 / 2.0
                        eq(150), // 300 / 2.0
                        anyInt(),
                        anyInt());
    }
}

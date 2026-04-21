// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.ArgumentMatchers.isNull;
import static org.mockito.Mockito.doAnswer;
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
import org.chromium.chrome.browser.util.PictureInPictureWindowOptions;
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

        when(mContentLayout.getWidth()).thenReturn(200);
        when(mContentLayout.getHeight()).thenReturn(200);
        doAnswer(
                        invocation -> {
                            int[] loc = invocation.getArgument(0);
                            loc[0] = 100;
                            loc[1] = 100;
                            return null;
                        })
                .when(mContentLayout)
                .getLocationOnScreen(any(int[].class));

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

    @Test
    @Config(sdk = Build.VERSION_CODES.S)
    public void testSaveBoundsToCache_NullLayout() {
        doReturn(null).when(mActivity).findViewById(R.id.document_picture_in_picture_content);

        mActivity.saveBoundsToCache();

        verifyNoInteractions(mMockNatives);
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.S)
    public void testSaveBoundsToCache_ZeroDimensions() {
        when(mContentLayout.getWidth()).thenReturn(0);
        when(mContentLayout.getHeight()).thenReturn(0);

        mActivity.saveBoundsToCache();

        verifyNoInteractions(mMockNatives);
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.S)
    public void testRevertToRequestedBounds_RevertsSuccessfully() {
        PictureInPictureWindowOptions windowOptions =
                new PictureInPictureWindowOptions(new Rect(100, 100, 300, 300), false);
        mActivity.setWindowOptionsForTesting(windowOptions);

        // Enforced bounds were larger (340x280).
        mActivity.setPromptEnforcedBoundsForTesting(new Rect(100, 100, 440, 380));

        // Current content layout bounds match the enforced bounds.
        when(mContentLayout.getWidth()).thenReturn(340);
        when(mContentLayout.getHeight()).thenReturn(280);

        // Mock current window bounds in pixels (from WindowMetrics).
        Rect windowBoundsPx = new Rect(100, 100, 300, 300); // 200x200
        when(mWindowMetrics.getBounds()).thenReturn(windowBoundsPx);

        mActivity.revertToRequestedBounds();

        // Calculations:
        // curContentWidth = 340, requestedWidth = 200 => widthDiff = -140
        // curContentHeight = 280, requestedHeight = 200 => heightDiff = -80
        // newBounds.left = currentWindowBounds.left - widthDiff = 100 - (-140) = 240
        // newBounds.top = currentWindowBounds.top - heightDiff = 100 - (-80) = 180
        // newBounds.right = 300, newBounds.bottom = 300
        verify(mAconfigFlaggedApiDelegate)
                .moveTaskTo(eq(mAppTask), eq(0), eq(new Rect(240, 180, 300, 300)));
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.S)
    public void testRevertToRequestedBounds_SkipsIfUserManuallyResized() {
        PictureInPictureWindowOptions windowOptions =
                new PictureInPictureWindowOptions(new Rect(100, 100, 300, 300), false);
        mActivity.setWindowOptionsForTesting(windowOptions);

        // Enforced bounds were 340x280.
        mActivity.setPromptEnforcedBoundsForTesting(new Rect(100, 100, 440, 380)); // 340x280

        // Current bounds differ from enforced bounds by more than 2dp tolerance.
        when(mContentLayout.getWidth()).thenReturn(400);
        when(mContentLayout.getHeight()).thenReturn(400);

        mActivity.revertToRequestedBounds();

        // It should NOT revert.
        verifyNoInteractions(mAconfigFlaggedApiDelegate);
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.S)
    public void testRevertToRequestedBounds_SkipsIfNoEnlargementNeeded() {
        PictureInPictureWindowOptions windowOptions =
                new PictureInPictureWindowOptions(new Rect(100, 100, 500, 500), false);
        mActivity.setWindowOptionsForTesting(windowOptions);

        // If no enlargement was needed (because requested bounds were large enough),
        // production leaves this null.
        mActivity.setPromptEnforcedBoundsForTesting(null);

        // Current bounds match requested.
        when(mContentLayout.getWidth()).thenReturn(500);
        when(mContentLayout.getHeight()).thenReturn(500);

        mActivity.revertToRequestedBounds();

        // It should NOT revert since it was already large enough.
        verifyNoInteractions(mAconfigFlaggedApiDelegate);
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.S)
    public void testSaveBoundsToCache_SkipsIfPromptEnlargementActive() {
        WebContents parentWebContents = mock(WebContents.class);
        when(parentWebContents.getTopLevelNativeWindow()).thenReturn(mActivityWindowAndroid);
        mActivity.setParentWebContentsOnInstanceForTesting(parentWebContents);

        PictureInPictureWindowOptions windowOptions =
                new PictureInPictureWindowOptions(
                        new Rect(100, 100, 300, 300), false); // 200x200 requested
        mActivity.setWindowOptionsForTesting(windowOptions);

        // We enforced a larger window for the prompt.
        mActivity.setPromptEnforcedBoundsForTesting(new Rect(100, 100, 440, 380)); // 340x280

        // The user closed the window without interacting, so the bounds are still at the enforced
        // size.
        when(mContentLayout.getWidth()).thenReturn(340);
        when(mContentLayout.getHeight()).thenReturn(280);
        doAnswer(
                        invocation -> {
                            int[] loc = invocation.getArgument(0);
                            loc[0] = 100;
                            loc[1] = 100;
                            return null;
                        })
                .when(mContentLayout)
                .getLocationOnScreen(any(int[].class));

        mActivity.saveBoundsToCache();

        // It should NOT cache these bounds.
        verifyNoInteractions(mMockNatives);
    }
}

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.ArgumentMatchers.isNull;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoInteractions;
import static org.mockito.Mockito.when;

import android.app.ActivityManager.AppTask;
import android.content.Intent;
import android.graphics.Rect;
import android.os.Build;
import android.view.View;
import android.view.WindowManager;
import android.view.WindowMetrics;
import android.widget.FrameLayout;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.ContextUtils;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.RobolectricUtil;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.profiles.ProfileProvider;
import org.chromium.chrome.browser.util.AndroidTaskUtils;
import org.chromium.chrome.browser.util.PictureInPictureWindowOptions;
import org.chromium.content_public.browser.RenderFrameHost;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.ActivityWindowAndroid;
import org.chromium.ui.display.DisplayAndroid;
import org.chromium.ui.display.DisplayAndroidManager;
import org.chromium.url.GURL;
import org.chromium.url.Origin;

/** Unit tests for {@link DocumentPictureInPictureActivity}. */
@RunWith(BaseRobolectricTestRunner.class)
public class DocumentPictureInPictureActivityUnitTest {
    private static class TestDocumentPictureInPictureActivity
            extends DocumentPictureInPictureActivity {
        private ActivityWindowAndroid mTestWindowAndroid;
        private WindowManager mTestWindowManager;
        private int mFinishCount;

        @Override
        public void finish() {
            mFinishCount++;
            super.finish();
        }

        // The real ActivityWindowAndroid is only created during native initialization.
        @Override
        public ActivityWindowAndroid getWindowAndroid() {
            return mTestWindowAndroid;
        }

        // Robolectric always reports window bounds anchored at the display origin.
        @Override
        public WindowManager getWindowManager() {
            return mTestWindowManager;
        }
    }

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private ActivityWindowAndroid mActivityWindowAndroid;
    @Mock private DisplayAndroid mDisplayAndroid;
    @Mock private PictureInPictureBoundsCacheBridge.Natives mMockNatives;
    @Mock private WindowManager mWindowManager;
    @Mock private WindowMetrics mWindowMetrics;
    @Mock private AndroidTaskUtils.MoveTaskDelegate mMoveTaskDelegate;
    @Mock private AppTask mAppTask;
    @Mock private DisplayAndroidManager mDisplayAndroidManager;
    @Mock private DocumentPictureInPictureActivity.Natives mMockActivityNatives;

    private TestDocumentPictureInPictureActivity mActivity;
    private FrameLayout mContentLayout;

    @Before
    public void setUp() {
        AndroidTaskUtils.setMoveTaskDelegateForTesting(mMoveTaskDelegate);
        AndroidTaskUtils.setAppTaskForTesting(mAppTask);
        DisplayAndroidManager.setInstanceForTesting(mDisplayAndroidManager);
        DisplayAndroid.setNonMultiDisplayForTesting(mDisplayAndroid);
        PictureInPictureBoundsCacheBridgeJni.setInstanceForTesting(mMockNatives);
        DocumentPictureInPictureActivityJni.setInstanceForTesting(mMockActivityNatives);

        // Common Display setup
        when(mDisplayAndroid.getDipScale()).thenReturn(1.0f);
        Rect displayBounds = new Rect(0, 0, 1000, 1000);
        when(mDisplayAndroid.getBounds()).thenReturn(displayBounds);
        when(mDisplayAndroid.getLocalBounds()).thenReturn(displayBounds);
        when(mDisplayAndroid.getDisplayId()).thenReturn(0);

        mActivity = Robolectric.buildActivity(TestDocumentPictureInPictureActivity.class).get();

        mActivity.mTestWindowAndroid = mActivityWindowAndroid;
        when(mActivityWindowAndroid.getDisplay()).thenReturn(mDisplayAndroid);

        mActivity.mTestWindowManager = mWindowManager;
        when(mWindowManager.getCurrentWindowMetrics()).thenReturn(mWindowMetrics);

        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);
        FrameLayout root = new FrameLayout(mActivity);
        mContentLayout = new FrameLayout(mActivity);
        mContentLayout.setId(R.id.document_picture_in_picture_content);
        root.addView(mContentLayout);
        mActivity.setContentView(root);
        // Attach the window so that getLocationOnScreen() reports real values.
        ContextUtils.getApplicationContext()
                .getSystemService(WindowManager.class)
                .addView(mActivity.getWindow().getDecorView(), new WindowManager.LayoutParams());

        when(mDisplayAndroidManager.getDisplayMatching(any(Rect.class)))
                .thenReturn(mDisplayAndroid);
    }

    @After
    public void tearDown() {
        ContextUtils.getApplicationContext()
                .getSystemService(WindowManager.class)
                .removeViewImmediate(mActivity.getWindow().getDecorView());
        AndroidTaskUtils.setMoveTaskDelegateForTesting(null);
        AndroidTaskUtils.setAppTaskForTesting(null);
        DisplayAndroidManager.resetInstanceForTesting();
        DisplayAndroid.setNonMultiDisplayForTesting(null);
        DocumentPictureInPictureActivityJni.setInstanceForTesting(null);
    }

    private void layoutContent(int left, int top, int width, int height) {
        FrameLayout.LayoutParams params = new FrameLayout.LayoutParams(width, height);
        params.leftMargin = left;
        params.topMargin = top;
        mContentLayout.setLayoutParams(params);
        RobolectricUtil.runAllBackgroundAndUi();
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.S)
    public void testResizeContents() {
        layoutContent(/* left= */ 0, /* top= */ 0, /* width= */ 100, /* height= */ 100);

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

        verify(mMoveTaskDelegate)
                .moveTaskTo(
                        eq(mAppTask),
                        eq(0), // displayId
                        eq(new Rect(50, 50, 300, 300)));
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.S)
    public void testResizeContents_NoChange() {
        layoutContent(/* left= */ 0, /* top= */ 0, /* width= */ 100, /* height= */ 100);

        mActivity.resizeContents(100, 100);

        verifyNoInteractions(mMoveTaskDelegate);
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

        layoutContent(/* left= */ 100, /* top= */ 100, /* width= */ 200, /* height= */ 200);

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
        mContentLayout.setId(View.NO_ID);

        mActivity.saveBoundsToCache();

        verifyNoInteractions(mMockNatives);
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.S)
    public void testSaveBoundsToCache_ZeroDimensions() {
        layoutContent(/* left= */ 0, /* top= */ 0, /* width= */ 0, /* height= */ 0);

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
        layoutContent(/* left= */ 0, /* top= */ 0, /* width= */ 340, /* height= */ 280);

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
        verify(mMoveTaskDelegate).moveTaskTo(eq(mAppTask), eq(0), eq(new Rect(240, 180, 300, 300)));
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
        layoutContent(/* left= */ 0, /* top= */ 0, /* width= */ 400, /* height= */ 400);

        mActivity.revertToRequestedBounds();

        // It should NOT revert.
        verifyNoInteractions(mMoveTaskDelegate);
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
        layoutContent(/* left= */ 0, /* top= */ 0, /* width= */ 500, /* height= */ 500);

        mActivity.revertToRequestedBounds();

        // It should NOT revert since it was already large enough.
        verifyNoInteractions(mMoveTaskDelegate);
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
        layoutContent(/* left= */ 100, /* top= */ 100, /* width= */ 340, /* height= */ 280);

        mActivity.saveBoundsToCache();

        // It should NOT cache these bounds.
        verifyNoInteractions(mMockNatives);
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.S)
    public void testPerformPreInflationStartup_AbortsIfSessionInvalid() {
        // Mock registerJavaActivity to return false (indicating native session closed).
        when(mMockActivityNatives.registerJavaActivity(any(), any())).thenReturn(false);

        // Setup mock WebContents to prevent crashes during early setup.
        WebContents webContents = mock(WebContents.class);
        when(webContents.isDestroyed()).thenReturn(false);
        DocumentPictureInPictureActivity.setWebContentsForTesting(webContents);

        mActivity.performPreInflationStartup();

        // Verify that performPreInflationStartup aborts early by calling finish().
        Assert.assertTrue(mActivity.isFinishing());
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.S)
    public void testCloseActivity() {
        Assert.assertFalse(mActivity.isFinishing());

        mActivity.closeActivity();

        Assert.assertTrue(mActivity.isFinishing());
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.S)
    public void testCloseActivity_DoesNotFinishIfAlreadyFinishing() {
        mActivity.finish();
        Assert.assertEquals(1, mActivity.mFinishCount);

        mActivity.closeActivity();

        Assert.assertEquals(1, mActivity.mFinishCount);
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.S)
    public void testCreateProfileProvider_BeforeContentsInitialized() {
        // createProfileProvider is called during onCreateInternal() before
        // performPreInflationStartup(). Verify it returns a valid supplier and does not throw.
        OneshotSupplier<ProfileProvider> supplier = mActivity.createProfileProvider();
        Assert.assertNotNull(supplier);
        ProfileProvider profileProvider = supplier.get();
        Assert.assertNotNull(profileProvider);
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.S)
    public void testVerifyOpenerOrigin_ValidHttpsOrigin_ReturnsTrue() {
        GURL url = new GURL("https://example.com/page");
        Intent intent = new Intent();
        intent.putExtra(
                DocumentPictureInPictureActivity.INITIAL_OPENER_ORIGIN_KEY,
                Origin.create(url).toString());
        WebContents parentWebContents = mock(WebContents.class);
        when(parentWebContents.getLastCommittedUrl()).thenReturn(url);

        Assert.assertTrue(mActivity.verifyOpenerOrigin(intent, parentWebContents));
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.S)
    public void testVerifyOpenerOrigin_OpaqueInitialOrigin_ReturnsFalse() {
        Intent intent = new Intent();
        intent.putExtra(DocumentPictureInPictureActivity.INITIAL_OPENER_ORIGIN_KEY, "null");
        WebContents parentWebContents = mock(WebContents.class);
        when(parentWebContents.getLastCommittedUrl()).thenReturn(new GURL("about:blank#fragment"));

        Assert.assertFalse(mActivity.verifyOpenerOrigin(intent, parentWebContents));
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.S)
    public void testVerifyOpenerOrigin_OpaqueCurrentOrigin_ReturnsFalse() {
        Intent intent = new Intent();
        intent.putExtra(
                DocumentPictureInPictureActivity.INITIAL_OPENER_ORIGIN_KEY, "https://example.com");
        WebContents parentWebContents = mock(WebContents.class);
        when(parentWebContents.getLastCommittedUrl()).thenReturn(new GURL("about:blank"));

        Assert.assertFalse(mActivity.verifyOpenerOrigin(intent, parentWebContents));
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.S)
    public void testVerifyOpenerOrigin_OriginMismatch_ReturnsFalse() {
        Intent intent = new Intent();
        intent.putExtra(
                DocumentPictureInPictureActivity.INITIAL_OPENER_ORIGIN_KEY, "https://example.com");
        WebContents parentWebContents = mock(WebContents.class);
        when(parentWebContents.getLastCommittedUrl())
                .thenReturn(new GURL("https://attacker.com/page"));

        Assert.assertFalse(mActivity.verifyOpenerOrigin(intent, parentWebContents));
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.S)
    public void testVerifyOpenerOrigin_MissingInitialOriginExtra_ReturnsFalse() {
        Intent intent = new Intent(); // No INITIAL_OPENER_ORIGIN_KEY extra
        WebContents parentWebContents = mock(WebContents.class);
        when(parentWebContents.getLastCommittedUrl())
                .thenReturn(new GURL("https://example.com/page"));

        Assert.assertFalse(mActivity.verifyOpenerOrigin(intent, parentWebContents));
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.S)
    public void testVerifyOpenerOrigin_NullParentWebContents_ReturnsFalse() {
        Intent intent = new Intent();
        intent.putExtra(
                DocumentPictureInPictureActivity.INITIAL_OPENER_ORIGIN_KEY, "https://example.com");

        Assert.assertFalse(mActivity.verifyOpenerOrigin(intent, null));
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.S)
    public void testVerifyOpenerOrigin_SandboxedOpaqueFrameOrigin_ReturnsFalse() {
        Intent intent = new Intent();
        intent.putExtra(
                DocumentPictureInPictureActivity.INITIAL_OPENER_ORIGIN_KEY, "https://example.com");
        WebContents parentWebContents = mock(WebContents.class);
        RenderFrameHost openerFrame = mock(RenderFrameHost.class);
        when(parentWebContents.getMainFrame()).thenReturn(openerFrame);
        // Sandboxed frame: URL is https://example.com, but frame origin is opaque
        when(parentWebContents.getLastCommittedUrl())
                .thenReturn(new GURL("https://example.com/page"));
        Origin opaqueOrigin = Origin.create(new GURL("about:blank"));
        when(openerFrame.getLastCommittedOrigin()).thenReturn(opaqueOrigin);

        Assert.assertFalse(mActivity.verifyOpenerOrigin(intent, parentWebContents));
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.S)
    public void testVerifyOpenerOrigin_FrameOriginMatchesInitialOrigin_ReturnsTrue() {
        GURL url = new GURL("https://example.com/page");
        Intent intent = new Intent();
        intent.putExtra(
                DocumentPictureInPictureActivity.INITIAL_OPENER_ORIGIN_KEY,
                Origin.create(url).toString());
        WebContents parentWebContents = mock(WebContents.class);
        RenderFrameHost openerFrame = mock(RenderFrameHost.class);
        when(parentWebContents.getMainFrame()).thenReturn(openerFrame);
        when(openerFrame.getLastCommittedOrigin()).thenReturn(Origin.create(url));

        Assert.assertTrue(mActivity.verifyOpenerOrigin(intent, parentWebContents));
    }
}

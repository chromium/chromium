// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features;

import static android.view.WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotEquals;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.os.Build;
import android.view.View;
import android.view.WindowInsets;
import android.view.WindowInsetsController;

import androidx.core.view.WindowCompat;
import androidx.core.view.WindowInsetsCompat;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;

import org.chromium.base.UnownedUserDataHost;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.RobolectricUtil;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.ui.base.ActivityWindowAndroid;
import org.chromium.ui.edge_to_edge.EdgeToEdgeStateProvider;

/** Tests for {@link ImmersiveModeController}. */
@RunWith(BaseRobolectricTestRunner.class)
public class ImmersiveModeControllerTest {
    // Convenience constants to make the tests  more readable.
    private static final boolean NOT_STICKY = false;
    private static final boolean STICKY = true;
    private static final int LAYOUT = LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES;

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock public ActivityLifecycleDispatcher mLifecycleDispatcher;

    @Mock public ActivityWindowAndroid mWindowAndroid;
    @Mock public EdgeToEdgeStateProvider mEdgeToEdgeStateProvider;
    public UnownedUserDataHost mWindowUserDataHost = new UnownedUserDataHost();

    private Activity mActivity;
    private View mDecorView;
    private ImmersiveModeController mController;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        mDecorView = mActivity.getWindow().getDecorView();

        when(mWindowAndroid.getUnownedUserDataHost()).thenReturn(mWindowUserDataHost);
        mController =
                new ImmersiveModeController(
                        mActivity, mWindowAndroid, mEdgeToEdgeStateProvider, mLifecycleDispatcher);
    }

    @Test
    public void enterImmersiveMode() {
        assertSystemBarsVisible(true);
        mController.enterImmersiveMode(LAYOUT, NOT_STICKY);
        RobolectricUtil.runAllBackgroundAndUiIncludingDelayed();
        assertSystemBarsVisible(false);
        if (isUsingWindowInsetsController()) {
            assertEquals(
                    WindowInsetsController.BEHAVIOR_SHOW_BARS_BY_SWIPE,
                    mDecorView.getWindowInsetsController().getSystemBarsBehavior());
        } else {
            assertNotEquals(0, mDecorView.getSystemUiVisibility() & View.SYSTEM_UI_FLAG_IMMERSIVE);
        }
    }

    @Test
    public void enterImmersiveMode_sticky() {
        assertSystemBarsVisible(true);
        mController.enterImmersiveMode(LAYOUT, STICKY);
        RobolectricUtil.runAllBackgroundAndUiIncludingDelayed();
        assertSystemBarsVisible(false);
        if (isUsingWindowInsetsController()) {
            assertEquals(
                    WindowInsetsController.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE,
                    mDecorView.getWindowInsetsController().getSystemBarsBehavior());
        } else {
            assertNotEquals(
                    0, mDecorView.getSystemUiVisibility() & View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY);
        }
    }

    @Test
    public void reApplyImmersiveMode_onResume() {
        mController.enterImmersiveMode(LAYOUT, NOT_STICKY);
        RobolectricUtil.runAllBackgroundAndUiIncludingDelayed();
        assertSystemBarsVisible(false);

        // Simulate the platform restoring the system bars while in the background. Delayed tasks
        // are not run so that the (delayed) restore triggered by the system UI visibility
        // listener does not re-hide the bars.
        WindowCompat.getInsetsController(mActivity.getWindow(), mDecorView)
                .show(WindowInsetsCompat.Type.systemBars());
        RobolectricUtil.runAllBackgroundAndUi();
        assertSystemBarsVisible(true);

        mController.onResumeWithNative();
        RobolectricUtil.runAllBackgroundAndUi();
        assertSystemBarsVisible(false);
    }

    @Test
    public void setsLayoutParams() {
        assertNotEquals(LAYOUT, mActivity.getWindow().getAttributes().layoutInDisplayCutoutMode);
        mController.enterImmersiveMode(LAYOUT, NOT_STICKY);
        assertEquals(LAYOUT, mActivity.getWindow().getAttributes().layoutInDisplayCutoutMode);
    }

    @Test
    public void exitImmersiveMode() {
        mController.enterImmersiveMode(LAYOUT, NOT_STICKY);
        RobolectricUtil.runAllBackgroundAndUiIncludingDelayed();
        assertSystemBarsVisible(false);
        mController.exitImmersiveMode();
        RobolectricUtil.runAllBackgroundAndUiIncludingDelayed();
        assertSystemBarsVisible(true);
    }

    @Test
    public void exitImmersiveMode_sticky() {
        mController.enterImmersiveMode(LAYOUT, STICKY);
        RobolectricUtil.runAllBackgroundAndUiIncludingDelayed();
        assertSystemBarsVisible(false);
        mController.exitImmersiveMode();
        RobolectricUtil.runAllBackgroundAndUiIncludingDelayed();
        assertSystemBarsVisible(true);
    }

    private void assertSystemBarsVisible(boolean visible) {
        if (isUsingWindowInsetsController()) {
            WindowInsets insets = mDecorView.getRootWindowInsets();
            assertEquals(visible, insets.isVisible(WindowInsets.Type.statusBars()));
            assertEquals(visible, insets.isVisible(WindowInsets.Type.navigationBars()));
        } else {
            int flags = mDecorView.getSystemUiVisibility();
            assertEquals(visible, (flags & View.SYSTEM_UI_FLAG_FULLSCREEN) == 0);
            assertEquals(visible, (flags & View.SYSTEM_UI_FLAG_HIDE_NAVIGATION) == 0);
        }
    }

    private boolean isUsingWindowInsetsController() {
        // ImmersiveModeController uses updateImmersiveFlagsOnAndroid11 for API 30,
        // which uses the legacy setSystemUiVisibility.
        // For other APIs, it uses WindowInsetsControllerCompat, which uses
        // WindowInsetsController on API 30+.
        // So on API 30 it actually doesn't use it, but on API 31+ it does.
        return Build.VERSION.SDK_INT > Build.VERSION_CODES.R;
    }
}

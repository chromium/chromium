// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features;

import static android.view.WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_DEFAULT;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotEquals;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.os.Build;
import android.view.View;
import android.view.Window;
import android.view.WindowInsetsController;
import android.view.WindowManager;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.UnownedUserDataHost;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.RobolectricUtil;
import org.chromium.chrome.browser.customtabs.CustomTabActivity;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.ui.base.ActivityWindowAndroid;

/** Tests for {@link ImmersiveModeController}. */
@RunWith(BaseRobolectricTestRunner.class)
public class ImmersiveModeControllerTest {
    // Convenience constants to make the tests  more readable.
    private static final boolean NOT_STICKY = false;
    private static final boolean STICKY = true;
    private static final int LAYOUT = LAYOUT_IN_DISPLAY_CUTOUT_MODE_DEFAULT;

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock public ActivityLifecycleDispatcher mLifecycleDispatcher;

    @Mock public CustomTabActivity mActivity;
    @Mock public ActivityWindowAndroid mWindowAndroid;
    @Mock public Window mWindow;
    @Mock public WindowInsetsController mInsetsController;
    @Mock public View mDecorView;
    private final WindowManager.LayoutParams mLayoutParams = new WindowManager.LayoutParams();
    public UnownedUserDataHost mWindowUserDataHost = new UnownedUserDataHost();

    private ImmersiveModeController mController;
    private int mSystemUiVisibility;

    @Before
    public void setUp() {

        // Wire up the Activity to the DecorView.
        when(mActivity.getWindow()).thenReturn(mWindow);
        when(mWindow.getDecorView()).thenReturn(mDecorView);
        when(mWindow.getAttributes()).thenReturn(mLayoutParams);
        when(mDecorView.getRootView()).thenReturn(mDecorView);
        when(mDecorView.getLayoutParams()).thenReturn(mLayoutParams);

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            when(mWindow.getInsetsController()).thenReturn(mInsetsController);
        }

        // Reflect mSystemUiVisibility in the DecorView.
        when(mDecorView.getSystemUiVisibility()).thenAnswer(invocation -> mSystemUiVisibility);
        doAnswer(
                        invocation -> {
                            mSystemUiVisibility = invocation.getArgument(0);
                            return null;
                        })
                .when(mDecorView)
                .setSystemUiVisibility(anyInt());

        when(mWindowAndroid.getUnownedUserDataHost()).thenReturn(mWindowUserDataHost);
        mController = new ImmersiveModeController(mActivity, mWindowAndroid, mLifecycleDispatcher);
    }

    @Test
    public void enterImmersiveMode() {
        mController.enterImmersiveMode(LAYOUT, NOT_STICKY);
        RobolectricUtil.runAllBackgroundAndUiIncludingDelayed();
        if (isUsingWindowInsetsController()) {
            verify(mInsetsController).hide(anyInt());
            verify(mInsetsController)
                    .setSystemBarsBehavior(WindowInsetsController.BEHAVIOR_SHOW_BARS_BY_SWIPE);
        } else {
            assertNotEquals(0, mSystemUiVisibility & View.SYSTEM_UI_FLAG_IMMERSIVE);
            assertNotEquals(0, mSystemUiVisibility & View.SYSTEM_UI_FLAG_FULLSCREEN);
            assertNotEquals(0, mSystemUiVisibility & View.SYSTEM_UI_FLAG_HIDE_NAVIGATION);
        }
    }

    @Test
    public void enterImmersiveMode_sticky() {
        mController.enterImmersiveMode(LAYOUT, STICKY);
        RobolectricUtil.runAllBackgroundAndUiIncludingDelayed();
        if (isUsingWindowInsetsController()) {
            verify(mInsetsController).hide(anyInt());
            verify(mInsetsController)
                    .setSystemBarsBehavior(
                            WindowInsetsController.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE);
        } else {
            assertNotEquals(0, mSystemUiVisibility & View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY);
            assertNotEquals(0, mSystemUiVisibility & View.SYSTEM_UI_FLAG_FULLSCREEN);
            assertNotEquals(0, mSystemUiVisibility & View.SYSTEM_UI_FLAG_HIDE_NAVIGATION);
        }
    }

    @Test
    public void setsLayoutParams() {
        mController.enterImmersiveMode(LAYOUT, NOT_STICKY);
        assertEquals(LAYOUT, mLayoutParams.layoutInDisplayCutoutMode);
    }

    @Test
    public void exitImmersiveMode() {
        mController.enterImmersiveMode(LAYOUT, NOT_STICKY);
        RobolectricUtil.runAllBackgroundAndUiIncludingDelayed();
        mController.exitImmersiveMode();
        if (isUsingWindowInsetsController()) {
            verify(mInsetsController).show(anyInt());
        } else {
            assertEquals(0, mSystemUiVisibility & View.SYSTEM_UI_FLAG_FULLSCREEN);
            assertEquals(0, mSystemUiVisibility & View.SYSTEM_UI_FLAG_HIDE_NAVIGATION);
        }
    }

    @Test
    public void exitImmersiveMode_sticky() {
        mController.enterImmersiveMode(LAYOUT, STICKY);
        RobolectricUtil.runAllBackgroundAndUiIncludingDelayed();
        mController.exitImmersiveMode();
        if (isUsingWindowInsetsController()) {
            verify(mInsetsController).show(anyInt());
        } else {
            assertEquals(0, mSystemUiVisibility & View.SYSTEM_UI_FLAG_FULLSCREEN);
            assertEquals(0, mSystemUiVisibility & View.SYSTEM_UI_FLAG_HIDE_NAVIGATION);
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

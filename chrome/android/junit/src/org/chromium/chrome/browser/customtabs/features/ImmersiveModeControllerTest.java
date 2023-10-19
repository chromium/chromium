// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features;

import static android.view.WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_DEFAULT;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotEquals;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.os.Build;
import android.view.View;
import android.view.Window;
import android.view.WindowManager;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.UnownedUserDataHost;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.ui.base.WindowAndroid;

/**
 * Tests for {@link ImmersiveModeController}.
 *
 * sdk = P for the cutout mode (setsLayoutParams) test.
 */
@RunWith(RobolectricTestRunner.class)
@Config(sdk = Build.VERSION_CODES.P, manifest = Config.NONE)
public class ImmersiveModeControllerTest {
    // Convenience constants to make the tests  more readable.
    private static final boolean NOT_STICKY = false;
    private static final boolean STICKY = true;
    private static final int LAYOUT = LAYOUT_IN_DISPLAY_CUTOUT_MODE_DEFAULT;

    @Mock public ActivityLifecycleDispatcher mLifecycleDispatcher;

    @Mock public Activity mActivity;
    @Mock public WindowAndroid mWindowAndroid;
    @Mock public Window mWindow;
    @Mock public View mDecorView;
    private WindowManager.LayoutParams mLayoutParams = new WindowManager.LayoutParams();
    public UnownedUserDataHost mWindowUserDataHost = new UnownedUserDataHost();

    private ImmersiveModeController mController;
    private int mSystemUiVisibility;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        // Wire up the Activity to the DecorView.
        when(mActivity.getWindow()).thenReturn(mWindow);
        when(mWindow.getDecorView()).thenReturn(mDecorView);
        when(mWindow.getAttributes()).thenReturn(mLayoutParams);
        when(mDecorView.getRootView()).thenReturn(mDecorView);
        when(mDecorView.getLayoutParams()).thenReturn(mLayoutParams);

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

        mController = new ImmersiveModeController(mLifecycleDispatcher, mActivity, mWindowAndroid);
    }

    @Test
    public void enterImmersiveMode() {
        mController.enterImmersiveMode(LAYOUT, NOT_STICKY);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        assertNotEquals(0, mSystemUiVisibility & View.SYSTEM_UI_FLAG_IMMERSIVE);
        assertNotEquals(0, mSystemUiVisibility & View.SYSTEM_UI_FLAG_FULLSCREEN);
        assertNotEquals(0, mSystemUiVisibility & View.SYSTEM_UI_FLAG_HIDE_NAVIGATION);
    }

    @Test
    public void enterImmersiveMode_sticky() {
        mController.enterImmersiveMode(LAYOUT, STICKY);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        assertNotEquals(0, mSystemUiVisibility & View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY);
        assertNotEquals(0, mSystemUiVisibility & View.SYSTEM_UI_FLAG_FULLSCREEN);
        assertNotEquals(0, mSystemUiVisibility & View.SYSTEM_UI_FLAG_HIDE_NAVIGATION);
    }

    @Test
    public void setsLayoutParams() {
        mController.enterImmersiveMode(LAYOUT, NOT_STICKY);
        assertEquals(LAYOUT, mLayoutParams.layoutInDisplayCutoutMode);
    }

    @Test
    public void exitImmersiveMode() {
        mController.enterImmersiveMode(LAYOUT, NOT_STICKY);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        mController.exitImmersiveMode();
        assertEquals(0, mSystemUiVisibility & View.SYSTEM_UI_FLAG_FULLSCREEN);
        assertEquals(0, mSystemUiVisibility & View.SYSTEM_UI_FLAG_HIDE_NAVIGATION);
    }

    @Test
    public void exitImmersiveMode_sticky() {
        mController.enterImmersiveMode(LAYOUT, STICKY);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        mController.exitImmersiveMode();
        assertEquals(0, mSystemUiVisibility & View.SYSTEM_UI_FLAG_FULLSCREEN);
        assertEquals(0, mSystemUiVisibility & View.SYSTEM_UI_FLAG_HIDE_NAVIGATION);
    }
}

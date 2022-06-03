// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.view.ActionMode;
import android.view.KeyEvent;
import android.view.KeyboardShortcutGroup;
import android.view.Menu;
import android.view.MenuItem;
import android.view.MotionEvent;
import android.view.SearchEvent;
import android.view.View;
import android.view.Window;
import android.view.WindowManager;
import android.view.accessibility.AccessibilityEvent;

import androidx.annotation.RequiresApi;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;
import org.robolectric.Shadows;
import org.robolectric.android.controller.ActivityController;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;
import org.robolectric.shadows.ShadowActivity;

import org.chromium.base.test.BaseRobolectricTestRunner;

import java.util.List;

/** Unit tests for {@link ApplicationStatus}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE, shadows = {ApplicationStatusTest.TrackingShadowActivity.class})
public class ApplicationStatusTest {
    private static class WindowCallbackWrapper implements Window.Callback {
        final Window.Callback mWrapped;

        public WindowCallbackWrapper(Window.Callback wrapped) {
            mWrapped = wrapped;
        }

        @Override
        public boolean dispatchKeyEvent(KeyEvent event) {
            return mWrapped.dispatchKeyEvent(event);
        }

        @Override
        public boolean dispatchKeyShortcutEvent(KeyEvent event) {
            return mWrapped.dispatchKeyShortcutEvent(event);
        }

        @Override
        public boolean dispatchTouchEvent(MotionEvent event) {
            return mWrapped.dispatchTouchEvent(event);
        }

        @Override
        public boolean dispatchTrackballEvent(MotionEvent event) {
            return mWrapped.dispatchTrackballEvent(event);
        }

        @Override
        public boolean dispatchGenericMotionEvent(MotionEvent event) {
            return mWrapped.dispatchGenericMotionEvent(event);
        }

        @Override
        public boolean dispatchPopulateAccessibilityEvent(AccessibilityEvent event) {
            return mWrapped.dispatchPopulateAccessibilityEvent(event);
        }

        @Override
        public View onCreatePanelView(int featureId) {
            return mWrapped.onCreatePanelView(featureId);
        }

        @Override
        public boolean onCreatePanelMenu(int featureId, Menu menu) {
            return mWrapped.onCreatePanelMenu(featureId, menu);
        }

        @Override
        public boolean onPreparePanel(int featureId, View view, Menu menu) {
            return mWrapped.onPreparePanel(featureId, view, menu);
        }

        @Override
        public boolean onMenuOpened(int featureId, Menu menu) {
            return mWrapped.onMenuOpened(featureId, menu);
        }

        @Override
        public boolean onMenuItemSelected(int featureId, MenuItem item) {
            return mWrapped.onMenuItemSelected(featureId, item);
        }

        @Override
        public void onWindowAttributesChanged(WindowManager.LayoutParams attrs) {
            mWrapped.onWindowAttributesChanged(attrs);
        }

        @Override
        public void onContentChanged() {
            mWrapped.onContentChanged();
        }

        @Override
        public void onWindowFocusChanged(boolean hasFocus) {
            mWrapped.onWindowFocusChanged(hasFocus);
        }

        @Override
        public void onAttachedToWindow() {
            mWrapped.onAttachedToWindow();
        }

        @Override
        public void onDetachedFromWindow() {
            mWrapped.onDetachedFromWindow();
        }

        @Override
        public void onPanelClosed(int featureId, Menu menu) {
            mWrapped.onPanelClosed(featureId, menu);
        }

        @RequiresApi(23)
        @Override
        public boolean onSearchRequested(SearchEvent searchEvent) {
            return mWrapped.onSearchRequested(searchEvent);
        }

        @Override
        public boolean onSearchRequested() {
            return mWrapped.onSearchRequested();
        }

        @Override
        public ActionMode onWindowStartingActionMode(ActionMode.Callback callback) {
            return mWrapped.onWindowStartingActionMode(callback);
        }

        @RequiresApi(23)
        @Override
        public ActionMode onWindowStartingActionMode(ActionMode.Callback callback, int type) {
            return mWrapped.onWindowStartingActionMode(callback, type);
        }

        @Override
        public void onActionModeStarted(ActionMode mode) {
            mWrapped.onActionModeStarted(mode);
        }

        @Override
        public void onActionModeFinished(ActionMode mode) {
            mWrapped.onActionModeFinished(mode);
        }

        @RequiresApi(24)
        @Override
        public void onProvideKeyboardShortcuts(
                List<KeyboardShortcutGroup> data, Menu menu, int deviceId) {
            mWrapped.onProvideKeyboardShortcuts(data, menu, deviceId);
        }

        @RequiresApi(26)
        @Override
        public void onPointerCaptureChanged(boolean hasCapture) {
            mWrapped.onPointerCaptureChanged(hasCapture);
        }
    }

    private static class SubclassedCallbackWrapper extends WindowCallbackWrapper {
        public SubclassedCallbackWrapper(Window.Callback callback) {
            super(callback);
        }
    }

    /** Shadow that tracks calls to onWindowFocusChanged and dispatchKeyEvent. */
    @Implements(Activity.class)
    public static class TrackingShadowActivity extends ShadowActivity {
        private int mWindowFocusCalls;
        private int mDispatchKeyEventCalls;
        private boolean mReturnValueForKeyDispatch;

        @Implementation
        public void onWindowFocusChanged(@SuppressWarnings("unused") boolean hasFocus) {
            mWindowFocusCalls++;
        }

        @Implementation
        public boolean dispatchKeyEvent(@SuppressWarnings("unused") KeyEvent event) {
            mDispatchKeyEventCalls++;
            return mReturnValueForKeyDispatch;
        }
    }

    @Test
    public void testWindowsFocusChanged() {
        ApplicationStatus.WindowFocusChangedListener mock =
                mock(ApplicationStatus.WindowFocusChangedListener.class);
        ApplicationStatus.registerWindowFocusChangedListener(mock);

        ActivityController<Activity> controller =
                Robolectric.buildActivity(Activity.class).create().start().visible();
        TrackingShadowActivity shadow = (TrackingShadowActivity) Shadows.shadowOf(controller.get());

        controller.get().getWindow().getCallback().onWindowFocusChanged(true);
        // Assert that listeners were notified.
        verify(mock).onWindowFocusChanged(controller.get(), true);
        // Also ensure that the original activity is forwarded the notification.
        Assert.assertEquals(1, shadow.mWindowFocusCalls);
    }

    @Test
    public void testNullCallback() {
        Assert.assertFalse(ApplicationStatus.reachesWindowCallback(null));
    }

    @Test
    public void testOtherCallback() {
        Assert.assertFalse(ApplicationStatus.reachesWindowCallback(mock(Window.Callback.class)));
    }

    private Window.Callback createWindowCallbackProxy() {
        return ApplicationStatus.createWindowCallbackProxy(
                mock(Activity.class), mock(Window.Callback.class));
    }

    @Test
    public void testNotWrappedCallback() {
        Assert.assertTrue(ApplicationStatus.reachesWindowCallback(createWindowCallbackProxy()));
    }

    @Test
    public void testSingleWrappedCallback() {
        Assert.assertTrue(ApplicationStatus.reachesWindowCallback(
                new WindowCallbackWrapper(createWindowCallbackProxy())));
    }

    @Test
    public void testDoubleWrappedCallback() {
        Assert.assertTrue(ApplicationStatus.reachesWindowCallback(
                new WindowCallbackWrapper(new WindowCallbackWrapper(createWindowCallbackProxy()))));
    }

    @Test
    public void testSubclassWrappedCallback() {
        Assert.assertTrue(ApplicationStatus.reachesWindowCallback(
                new SubclassedCallbackWrapper(createWindowCallbackProxy())));
    }
}

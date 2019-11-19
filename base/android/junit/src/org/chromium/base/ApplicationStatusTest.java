// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.view.KeyEvent;

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
import org.robolectric.shadows.multidex.ShadowMultiDex;

import org.chromium.base.test.BaseRobolectricTestRunner;

/** Unit tests for {@link ApplicationStatus}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE,
        shadows = {ApplicationStatusTest.TrackingShadowActivity.class, ShadowMultiDex.class})
public class ApplicationStatusTest {
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
}

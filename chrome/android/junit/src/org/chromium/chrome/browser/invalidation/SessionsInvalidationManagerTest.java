// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.invalidation;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.verify;
import static org.mockito.MockitoAnnotations.initMocks;

import android.app.Activity;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.recent_tabs.ForeignSessionHelper;

import java.util.concurrent.atomic.AtomicBoolean;

/** Tests for the {@link SessionsInvalidationManager}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class SessionsInvalidationManagerTest {
    @Mock private ResumableDelayedTaskRunner mResumableDelayedTaskRunner;

    @Mock private Profile mProfile;

    @Mock private ForeignSessionHelper mForeignSessionHelper;

    private Activity mActivity;

    @Before
    public void setup() {
        initMocks(this);
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
    }

    /**
     * Test that creating an SessionsInvalidationManager object registers an
     * ApplicationStateListener.
     */
    @Test
    public void testEnsureConstructorRegistersListener() {
        final AtomicBoolean listenerCallbackCalled = new AtomicBoolean();

        // Create instance.
        new SessionsInvalidationManager(mProfile, mResumableDelayedTaskRunner) {
            @Override
            public void onApplicationStateChange(int newState) {
                listenerCallbackCalled.set(true);
            }
        };

        // Ensure initial state is correct.
        assertFalse(listenerCallbackCalled.get());

        // Ensure we get a callback, which means we have registered for them.
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.DESTROYED);
        assertTrue(listenerCallbackCalled.get());
    }

    /** Test that timer pauses when the application goes to the background. */
    @Test
    public void testTimerPausesWhenTheApplicationPauses() {
        SessionsInvalidationManager manager =
                new SessionsInvalidationManager(mProfile, mResumableDelayedTaskRunner);

        manager.onApplicationStateChange(ApplicationState.HAS_RUNNING_ACTIVITIES);
        verify(mResumableDelayedTaskRunner).resume();

        manager.onApplicationStateChange(ApplicationState.HAS_PAUSED_ACTIVITIES);
        verify(mResumableDelayedTaskRunner).pause();
    }
}

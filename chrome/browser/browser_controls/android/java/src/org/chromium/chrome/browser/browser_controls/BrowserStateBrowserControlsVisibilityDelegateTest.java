// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browser_controls;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import static org.chromium.chrome.browser.browser_controls.BrowserStateBrowserControlsVisibilityDelegate.MINIMUM_SHOW_DURATION_MS;

import android.os.SystemClock;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.cc.input.BrowserControlsState;

/** Unit tests for the BrowserStateBrowserControlsVisibilityDelegate. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class BrowserStateBrowserControlsVisibilityDelegateTest {
    @Mock private Callback<Integer> mCallback;

    private BrowserStateBrowserControlsVisibilityDelegate mDelegate;
    private ObservableSupplierImpl<Boolean> mPersistentModeSupplier;

    @Before
    public void beforeTest() {
        MockitoAnnotations.initMocks(this);

        mPersistentModeSupplier = new ObservableSupplierImpl<>();
        mPersistentModeSupplier.set(false);

        mDelegate = new BrowserStateBrowserControlsVisibilityDelegate(mPersistentModeSupplier);
        mDelegate.addObserver(mCallback);
        Mockito.reset(mCallback);
    }

    private void advanceTime(long amount) {
        SystemClock.setCurrentTimeMillis(SystemClock.elapsedRealtime() + amount);
    }

    private int constraints() {
        return mDelegate.get();
    }

    @Test
    public void testTransientShow() {
        assertEquals(BrowserControlsState.BOTH, constraints());
        mDelegate.showControlsTransient();
        assertEquals(BrowserControlsState.SHOWN, constraints());

        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        assertEquals(BrowserControlsState.BOTH, constraints());

        verify(mCallback, times(2)).onResult(Mockito.anyInt());
    }

    @Test
    public void testShowPersistentTokenWithDelayedHide() {
        assertEquals(BrowserControlsState.BOTH, constraints());
        int token = mDelegate.showControlsPersistent();
        assertEquals(BrowserControlsState.SHOWN, constraints());
        // Advance the clock to exceed the minimum show time.
        advanceTime(2 * MINIMUM_SHOW_DURATION_MS);
        assertEquals(BrowserControlsState.SHOWN, constraints());
        mDelegate.releasePersistentShowingToken(token);
        assertEquals(BrowserControlsState.BOTH, constraints());

        verify(mCallback, times(2)).onResult(Mockito.anyInt());
    }

    @Test
    public void testShowPersistentTokenWithImmediateHide() {
        assertEquals(BrowserControlsState.BOTH, constraints());
        int token = mDelegate.showControlsPersistent();
        assertEquals(BrowserControlsState.SHOWN, constraints());
        mDelegate.releasePersistentShowingToken(token);

        // If the controls are not shown for the mimimum allowed time, then a task is posted to
        // keep them shown for longer.  Ensure the controls can not be hidden until this delayed
        // task has been run.
        assertEquals(BrowserControlsState.SHOWN, constraints());
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        assertEquals(BrowserControlsState.BOTH, constraints());

        verify(mCallback, times(2)).onResult(Mockito.anyInt());
    }

    @Test
    public void testShowPersistentBeyondRequiredMinDurationAndShowTransient() {
        assertEquals(BrowserControlsState.BOTH, constraints());
        int token = mDelegate.showControlsPersistent();
        assertEquals(BrowserControlsState.SHOWN, constraints());

        // Advance the clock to exceed the minimum show time.
        advanceTime(2 * MINIMUM_SHOW_DURATION_MS);
        assertEquals(BrowserControlsState.SHOWN, constraints());
        mDelegate.showControlsTransient();

        // Controls should stil be shown since showControlsTransient was just called.
        mDelegate.releasePersistentShowingToken(token);
        assertEquals(BrowserControlsState.SHOWN, constraints());
        advanceTime((long) (0.5 * MINIMUM_SHOW_DURATION_MS));
        assertEquals(BrowserControlsState.SHOWN, constraints());

        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        assertEquals(BrowserControlsState.BOTH, constraints());

        verify(mCallback, times(2)).onResult(Mockito.anyInt());
    }

    @Test
    public void testShowPersistentBelowRequiredMinDurationAndShowTransient() {
        assertEquals(BrowserControlsState.BOTH, constraints());
        int token = mDelegate.showControlsPersistent();
        assertEquals(BrowserControlsState.SHOWN, constraints());

        // Advance the clock but not beyond the min show duration.
        advanceTime((long) (0.5 * MINIMUM_SHOW_DURATION_MS));
        assertEquals(BrowserControlsState.SHOWN, constraints());
        mDelegate.showControlsTransient();
        mDelegate.releasePersistentShowingToken(token);
        assertEquals(BrowserControlsState.SHOWN, constraints());

        // Run the pending tasks on the UI thread, which will include the transient delayed task.
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        assertEquals(BrowserControlsState.BOTH, constraints());

        verify(mCallback, times(2)).onResult(Mockito.anyInt());
    }

    @Test
    public void testShowPersistentMultipleTimes() {
        assertEquals(BrowserControlsState.BOTH, constraints());
        int firstToken = mDelegate.showControlsPersistent();
        assertEquals(BrowserControlsState.SHOWN, constraints());

        int secondToken = mDelegate.showControlsPersistent();
        assertEquals(BrowserControlsState.SHOWN, constraints());

        int thirdToken = mDelegate.showControlsPersistent();
        assertEquals(BrowserControlsState.SHOWN, constraints());

        // Advance the clock to exceed the minimum show time.
        advanceTime(2 * MINIMUM_SHOW_DURATION_MS);
        assertEquals(BrowserControlsState.SHOWN, constraints());

        mDelegate.releasePersistentShowingToken(secondToken);
        assertEquals(BrowserControlsState.SHOWN, constraints());
        mDelegate.releasePersistentShowingToken(firstToken);
        assertEquals(BrowserControlsState.SHOWN, constraints());
        mDelegate.releasePersistentShowingToken(thirdToken);
        assertEquals(BrowserControlsState.BOTH, constraints());

        verify(mCallback, times(2)).onResult(Mockito.anyInt());
    }

    @Test
    public void testGlobalPersistentMode() {
        assertEquals(BrowserControlsState.BOTH, constraints());
        mPersistentModeSupplier.set(true);
        assertEquals(BrowserControlsState.HIDDEN, constraints());
        mPersistentModeSupplier.set(false);
        assertEquals(BrowserControlsState.BOTH, constraints());

        verify(mCallback, times(2)).onResult(Mockito.anyInt());
    }
}

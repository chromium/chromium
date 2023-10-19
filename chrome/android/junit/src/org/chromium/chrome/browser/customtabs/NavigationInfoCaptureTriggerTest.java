// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.tab.Tab;

import java.util.concurrent.TimeUnit;

/** Tests for {@link NavigationInfoCaptureTrigger}. */
@RunWith(BaseRobolectricTestRunner.class)
@Batch(Batch.UNIT_TESTS)
@Config(manifest = Config.NONE)
public class NavigationInfoCaptureTriggerTest {
    @Mock private Callback<Tab> mDelegate;
    private NavigationInfoCaptureTrigger mTrigger;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        mTrigger = new NavigationInfoCaptureTrigger(mDelegate);
    }

    /**
     * Tests the normal flow where onload is called, then first meaningful paint happens soon
     * after. We want the capture to trigger after first meaningful paint.
     */
    @Test
    @Feature({"CustomTabs"})
    public void testNormalFlow() {
        mTrigger.onLoadFinished(null);

        // If we run everything on the Looper, the backup onload capture will trigger. Therefore
        // run long enough for the primary onload to trigger.
        ShadowLooper.idleMainLooper(2, TimeUnit.SECONDS);
        verify(mDelegate, times(0)).onResult(any());

        mTrigger.onFirstMeaningfulPaint(null);
        verifyCaptured(1);

        mTrigger.onHide(null);
        verifyCaptured(1);
    }

    /**
     * Tests the flow where first meaningful paint is called before onload. The screen should only
     * be captured once, after the first meaningful paint and onload.
     */
    @Test
    @Feature({"CustomTabs"})
    public void testDelayedOnload() {
        mTrigger.onFirstMeaningfulPaint(null);
        verifyCaptured(0);

        mTrigger.onLoadFinished(null);
        verifyCaptured(1);

        mTrigger.onHide(null);
        verifyCaptured(1);
    }

    /**
     * Tests the flow where first meaningful paint and onload don't occur and we capture during
     * on hide as a backup.
     */
    @Test
    @Feature({"CustomTabs"})
    public void testOnHide() {
        mTrigger.onHide(null);
        verifyCaptured(1);
    }

    /** Tests that the backup onload trigger works. */
    @Test
    @Feature({"CustomTabs"})
    public void testBackupOnload() {
        mTrigger.onLoadFinished(null);

        ShadowLooper.idleMainLooper(2, TimeUnit.SECONDS);
        verify(mDelegate, times(0)).onResult(any());

        verifyCaptured(1);
    }

    /** Tests that pending capture tasks are cancelled when the page navigates. */
    @Test
    @Feature({"CustomTabs"})
    public void testCancelOnNavigation() {
        mTrigger.onLoadFinished(null);
        mTrigger.onFirstMeaningfulPaint(null);

        mTrigger.onNewNavigation();
        verifyCaptured(0);
    }

    /** Tests that navigation resets the state. */
    @Test
    @Feature({"CustomTabs"})
    @SuppressWarnings("unchecked")
    public void testResetOnNavigation() {
        testNormalFlow();

        mTrigger.onNewNavigation();

        clearInvocations(mDelegate); // Clears the mock so the verifies in the original test work.
        testNormalFlow();

        mTrigger.onNewNavigation();

        clearInvocations(mDelegate);
        testDelayedOnload();
    }

    /** Tests that we capture only on the first FMP. */
    @Test
    @Feature({"CustomTabs"})
    public void testMultipleFmps() {
        mTrigger.onLoadFinished(null);
        mTrigger.onFirstMeaningfulPaint(null);
        mTrigger.onFirstMeaningfulPaint(null);
        verifyCaptured(1);
    }

    private void verifyCaptured(int times) {
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        verify(mDelegate, times(times)).onResult(any());
    }
}

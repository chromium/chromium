// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.multiwindow;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.graphics.Rect;
import android.view.Display;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.ui.browser_window.ChromeAndroidTaskFeature.InitInfo;

import java.util.List;

/** Unit tests for {@link TabbedWindowStateTracker}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@EnableFeatures(ChromeFeatureList.SESSION_RESTORE_AFTER_CRASH)
public class TabbedWindowStateTrackerUnitTest {
    private static final int WINDOW_ID_0 = 0;
    private static final Rect WINDOW_BOUNDS_1 = new Rect(100, 100, 600, 400);
    private static final Rect WINDOW_BOUNDS_2 = new Rect(200, 200, 700, 500);

    private TabbedWindowStateTracker mTracker;

    @Before
    public void setUp() {
        ChromeMultiInstancePersistentStore.ensureInitialized();
        MultiWindowUtils.setMultiInstanceApi31EnabledForTesting(true);
        ChromeMultiInstancePersistentStore.writeLastAccessedTime(WINDOW_ID_0);
        ChromeMultiInstancePersistentStore.writeTabCount(
                WINDOW_ID_0, /* normalTabCount= */ 1, /* incognitoTabCount= */ 0);
        mTracker = TabbedWindowStateTracker.create(WINDOW_ID_0);
    }

    @After
    public void tearDown() {
        ChromeMultiInstancePersistentStore.resetForTesting();
    }

    @Test
    public void testOnAddedToTask_savesInitInfoForDefaultDisplay() {
        // Setup.
        ChromeMultiInstancePersistentStore.writeIsRecoverable(WINDOW_ID_0, true);
        InitInfo initInfo =
                new InitInfo(
                        /* nativeBrowserWindowPtr= */ 0,
                        /* isVisible= */ true,
                        WINDOW_BOUNDS_1,
                        WINDOW_BOUNDS_1,
                        Display.DEFAULT_DISPLAY);

        // Act.
        mTracker.onAddedToTask(initInfo);

        // Verify.
        List<CrashRecoveryWindowInfo> infoList =
                ChromeMultiInstancePersistentStore.readCrashRecoveryData();
        assertEquals(1, infoList.size());
        assertEquals(WINDOW_ID_0, infoList.get(0).windowId);
        assertTrue(infoList.get(0).isVisible);
        assertEquals(WINDOW_BOUNDS_1, infoList.get(0).bounds);
    }

    @Test
    public void testOnAddedToTask_savesInitInfoForNonDefaultDisplay() {
        // Setup.
        ChromeMultiInstancePersistentStore.writeIsRecoverable(WINDOW_ID_0, true);
        InitInfo initInfo =
                new InitInfo(
                        /* nativeBrowserWindowPtr= */ 0,
                        /* isVisible= */ true,
                        WINDOW_BOUNDS_1,
                        WINDOW_BOUNDS_1,
                        /* displayId= */ 5);

        // Act.
        mTracker.onAddedToTask(initInfo);

        // Verify.
        List<CrashRecoveryWindowInfo> infoList =
                ChromeMultiInstancePersistentStore.readCrashRecoveryData();
        assertEquals(1, infoList.size());
        assertEquals(WINDOW_ID_0, infoList.get(0).windowId);
        assertTrue(infoList.get(0).isVisible);
        assertEquals(new Rect(), infoList.get(0).bounds);
    }

    @Test
    public void testOnAddedToTask_recordsWindowWidth() {
        // Setup.
        InitInfo initInfo =
                new InitInfo(
                        /* nativeBrowserWindowPtr= */ 0,
                        /* isVisible= */ true,
                        WINDOW_BOUNDS_1,
                        WINDOW_BOUNDS_1,
                        Display.DEFAULT_DISPLAY);

        // Act.
        // WINDOW_BOUNDS_1.width() is 500. Robolectric's default density is 1.0.
        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord("Android.WindowWidth", WINDOW_BOUNDS_1.width())
                        .build();
        mTracker.onAddedToTask(initInfo);

        // Verify.
        histogramWatcher.assertExpected();
    }

    @Test
    public void testOnTaskVisibilityChanged_isVisible() {
        // Setup.
        ChromeMultiInstancePersistentStore.writeIsRecoverable(WINDOW_ID_0, true);

        // Act.
        mTracker.onTaskVisibilityChanged(true);

        // Verify.
        List<CrashRecoveryWindowInfo> infoList =
                ChromeMultiInstancePersistentStore.readCrashRecoveryData();
        assertEquals(1, infoList.size());
        assertEquals(WINDOW_ID_0, infoList.get(0).windowId);
        assertTrue(infoList.get(0).isVisible);
    }

    @Test
    public void testOnTaskVisibilityChanged_isNotVisible() {
        // Setup.
        ChromeMultiInstancePersistentStore.writeIsRecoverable(WINDOW_ID_0, true);

        // Act.
        mTracker.onTaskVisibilityChanged(false);

        // Verify.
        List<CrashRecoveryWindowInfo> infoList =
                ChromeMultiInstancePersistentStore.readCrashRecoveryData();
        assertEquals(1, infoList.size());
        assertEquals(WINDOW_ID_0, infoList.get(0).windowId);
        assertFalse(infoList.get(0).isVisible);
    }

    @Test
    public void testOnTaskBoundsChanged_savesWindowBoundsOnDefaultDisplay() {
        // Setup.
        ChromeMultiInstancePersistentStore.writeIsRecoverable(WINDOW_ID_0, true);

        // Act.
        mTracker.onTaskBoundsChanged(Display.DEFAULT_DISPLAY, WINDOW_BOUNDS_2, WINDOW_BOUNDS_2);

        // Verify.
        List<CrashRecoveryWindowInfo> infoList =
                ChromeMultiInstancePersistentStore.readCrashRecoveryData();
        assertEquals(1, infoList.size());
        assertEquals(WINDOW_ID_0, infoList.get(0).windowId);
        assertEquals(WINDOW_BOUNDS_2, infoList.get(0).bounds);
    }

    @Test
    public void testOnTaskBoundsChanged_savesWindowBoundsOnNonDefaultDisplay() {
        // Setup.
        ChromeMultiInstancePersistentStore.writeIsRecoverable(WINDOW_ID_0, true);

        // Act.
        mTracker.onTaskBoundsChanged(Display.DEFAULT_DISPLAY, WINDOW_BOUNDS_1, WINDOW_BOUNDS_1);
        mTracker.onTaskBoundsChanged(/* displayId= */ 5, WINDOW_BOUNDS_2, WINDOW_BOUNDS_2);

        // Verify.
        List<CrashRecoveryWindowInfo> infoList =
                ChromeMultiInstancePersistentStore.readCrashRecoveryData();
        assertEquals(1, infoList.size());
        assertEquals(WINDOW_ID_0, infoList.get(0).windowId);
        assertEquals(new Rect(), infoList.get(0).bounds);
    }

    @Test
    public void testOnTaskBoundsChanged_recordsWindowWidth() {
        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord("Android.WindowWidth", WINDOW_BOUNDS_1.width())
                        .build();
        mTracker.onTaskBoundsChanged(Display.DEFAULT_DISPLAY, WINDOW_BOUNDS_1, WINDOW_BOUNDS_1);
        histogramWatcher.assertExpected();
    }
}

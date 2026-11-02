// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.shortcuts;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoInteractions;
import static org.mockito.Mockito.when;

import android.view.KeyEvent;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.ChromeCommandIds;
import org.chromium.content_public.browser.test.NativeLibraryTestUtils;
import org.chromium.ui.base.AcceleratorManager;
import org.chromium.ui.base.WindowAndroid;

/**
 * Native unit tests for {@link BrowserShortcutManager} and {@code BrowserShortcutManagerAndroid}.
 */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class BrowserShortcutManagerTest {
    private static final String TRIGGERED_HISTOGRAM = "Browser.Shortcuts.TriggeredCommandId";

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private BrowserCommandDelegate mCommandDelegate;

    private WindowAndroid mWindowAndroid;
    private AcceleratorManager mAcceleratorManager;
    private BrowserShortcutManager mShortcutManager;

    @BeforeClass
    public static void setUpBeforeClass() {
        NativeLibraryTestUtils.loadNativeLibraryNoBrowserProcess();
    }

    @Before
    public void setUp() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mWindowAndroid =
                            new WindowAndroid(
                                    ContextUtils.getApplicationContext(),
                                    /* occlusionTrackingAllowed= */ false);
                    mAcceleratorManager = AcceleratorManager.getOrCreate(mWindowAndroid);
                    mShortcutManager = new BrowserShortcutManager(mWindowAndroid, mCommandDelegate);
                });
    }

    @After
    public void tearDown() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    if (mShortcutManager != null) {
                        mShortcutManager.destroy();
                    }
                    mAcceleratorManager.destroy();
                    mWindowAndroid.destroy();
                });
    }

    private boolean processKeyDown(int keyCode, int metaState) {
        return ThreadUtils.runOnUiThreadBlocking(
                () ->
                        mAcceleratorManager.processKeyEvent(
                                new KeyEvent(
                                        /* downTime= */ 0,
                                        /* eventTime= */ 0,
                                        KeyEvent.ACTION_DOWN,
                                        keyCode,
                                        /* repeat= */ 0,
                                        metaState)));
    }

    @Test
    @SmallTest
    public void testProcessKeyEvent_ExecutesCommandAndRecordsHistogram() {
        when(mCommandDelegate.executeCommand(ChromeCommandIds.NEW_TAB)).thenReturn(true);

        try (var watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        TRIGGERED_HISTOGRAM, ChromeCommandIds.NEW_TAB)) {
            assertTrue(processKeyDown(KeyEvent.KEYCODE_T, KeyEvent.META_CTRL_ON));
        }
        verify(mCommandDelegate).executeCommand(ChromeCommandIds.NEW_TAB);

        assertFalse(processKeyDown(KeyEvent.KEYCODE_A, /* metaState= */ 0));
    }

    @Test
    @SmallTest
    public void testProcessKeyEvent_UnhandledCommandReturnsFalse() {
        when(mCommandDelegate.executeCommand(ChromeCommandIds.CLOSE_FIND_OR_STOP))
                .thenReturn(false);

        assertFalse(processKeyDown(KeyEvent.KEYCODE_ESCAPE, /* metaState= */ 0));
        verify(mCommandDelegate).executeCommand(ChromeCommandIds.CLOSE_FIND_OR_STOP);
    }

    @Test
    @SmallTest
    public void testDestroy_UnregistersAccelerators() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mShortcutManager.destroy();
                    mShortcutManager = null;
                });

        assertFalse(processKeyDown(KeyEvent.KEYCODE_T, KeyEvent.META_CTRL_ON));
        verifyNoInteractions(mCommandDelegate);
    }
}

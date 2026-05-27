// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoInteractions;

import android.app.ApplicationExitInfo;
import android.os.Build;
import android.os.Process;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.multiwindow.MultiInstanceOrchestrator;
import org.chromium.chrome.browser.multiwindow.MultiInstanceOrchestratorFactory;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;

/** Unit tests for {@link BrowserExitReasonTracker}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE, sdk = Build.VERSION_CODES.R)
@EnableFeatures(ChromeFeatureList.SESSION_RESTORE_AFTER_CRASH)
public class BrowserExitReasonTrackerUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private MultiInstanceOrchestrator mOrchestrator;

    private SharedPreferencesManager mPrefs;

    @Before
    public void setUp() {
        mPrefs = ChromeSharedPreferences.getInstance();
        MultiInstanceOrchestratorFactory.setInstanceForTesting(mOrchestrator);
    }

    @Test
    public void testInitForegroundBrowserProcess_withReason() {
        int reason = ApplicationExitInfo.REASON_CRASH;
        mPrefs.writeInt(ChromePreferenceKeys.LAST_SESSION_BROWSER_EXIT_REASON, reason);

        BrowserExitReasonTracker.initForegroundBrowserProcess();

        verify(mOrchestrator).onForegroundBrowserProcessInitialized(reason);
        assertFalse(mPrefs.contains(ChromePreferenceKeys.LAST_SESSION_BROWSER_EXIT_REASON));
        assertEquals(
                Process.myPid(), mPrefs.readInt(ChromePreferenceKeys.LAST_SESSION_BROWSER_PID));
    }

    @Test
    public void testInitForegroundBrowserProcess_withoutReason() {
        BrowserExitReasonTracker.initForegroundBrowserProcess();

        verifyNoInteractions(mOrchestrator);
        assertFalse(mPrefs.contains(ChromePreferenceKeys.LAST_SESSION_BROWSER_EXIT_REASON));
        assertEquals(
                Process.myPid(), mPrefs.readInt(ChromePreferenceKeys.LAST_SESSION_BROWSER_PID));
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.Q)
    public void testInitForegroundBrowserProcess_preR() {
        int reason = ApplicationExitInfo.REASON_CRASH;
        mPrefs.writeInt(ChromePreferenceKeys.LAST_SESSION_BROWSER_EXIT_REASON, reason);

        BrowserExitReasonTracker.initForegroundBrowserProcess();

        verifyNoInteractions(mOrchestrator);
        // Pre-R it should just return early and not even touch prefs or record histograms.
        assertTrue(mPrefs.contains(ChromePreferenceKeys.LAST_SESSION_BROWSER_EXIT_REASON));
    }
}

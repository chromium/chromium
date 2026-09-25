// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omaha.inline;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.content.Context;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.ContextUtils;
import org.chromium.base.DeviceInfo;
import org.chromium.base.FakeTimeTestRule;
import org.chromium.base.TimeUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.gms.ChromiumPlayServicesAvailability;

import java.util.concurrent.TimeUnit;

/** Unit tests for {@link InAppUpdatePolicy}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@EnableFeatures(ChromeFeatureList.IN_APP_UPDATE_FLOW)
public class InAppUpdatePolicyTest {
    @Rule public FakeTimeTestRule mFakeTimeRule = new FakeTimeTestRule();

    private Context mContext;

    @Before
    public void setUp() {
        mContext = ContextUtils.getApplicationContext();
        ChromiumPlayServicesAvailability.setIsAvailableForTesting(true);
        InAppUpdatePolicy.setIsOfficialBuildForTesting(true);
    }

    private static void clearPrefs() {
        ChromeSharedPreferences.getInstance()
                .removeKey(InAppUpdatePolicy.PREF_KEY_DISCOVERY_BACKOFF);
        ChromeSharedPreferences.getInstance().removeKey(InAppUpdatePolicy.PREF_KEY_FAILURE_BACKOFF);
        ChromeSharedPreferences.getInstance().removeKey(InAppUpdatePolicy.PREF_KEY_RESTART_BACKOFF);
    }

    // Eligibility guards.

    @Test
    public void testIsEligible_allGuardsPass() {
        assertTrue(InAppUpdatePolicy.isEligible(mContext));
    }

    @Test
    @DisableFeatures(ChromeFeatureList.IN_APP_UPDATE_FLOW)
    public void testIsEligible_featureDisabled() {
        assertFalse(InAppUpdatePolicy.isEligible(mContext));
    }

    @Test
    public void testIsEligible_playServicesUnavailable() {
        ChromiumPlayServicesAvailability.setIsAvailableForTesting(false);
        assertFalse(InAppUpdatePolicy.isEligible(mContext));
    }

    @Test
    public void testIsEligible_unofficialBuild() {
        InAppUpdatePolicy.setIsOfficialBuildForTesting(false);
        assertFalse(InAppUpdatePolicy.isEligible(mContext));
    }

    @Test
    public void testIsEligible_automotive() {
        DeviceInfo.setIsAutomotiveForTesting(true);
        assertFalse(InAppUpdatePolicy.isEligible(mContext));
    }

    @Test
    @DisableFeatures(ChromeFeatureList.IN_APP_UPDATE_FLOW)
    @CommandLineFlags.Add({InAppUpdatePolicy.FORCE_IN_APP_UPDATE})
    public void testIsEligible_forceUpdateOverridesEveryGuard() {
        ChromiumPlayServicesAvailability.setIsAvailableForTesting(false);
        DeviceInfo.setIsAutomotiveForTesting(true);
        InAppUpdatePolicy.setIsOfficialBuildForTesting(false);

        assertTrue(InAppUpdatePolicy.isForceUpdate());
        assertTrue(InAppUpdatePolicy.isEligible(mContext));
    }

    // Eligibility short-circuiting inside the prompt predicates.

    @Test
    @DisableFeatures(ChromeFeatureList.IN_APP_UPDATE_FLOW)
    public void testDiscoveryPrompt_blockedWhenIneligible() {
        assertFalse(InAppUpdatePolicy.isDiscoveryPromptAllowed(mContext));
    }

    @Test
    @DisableFeatures(ChromeFeatureList.IN_APP_UPDATE_FLOW)
    public void testRestartPrompt_blockedWhenIneligible() {
        assertFalse(InAppUpdatePolicy.isRestartPromptAllowed(mContext));
    }

    /** Ineligibility must block both prompts regardless of which guard fails. */
    @Test
    public void testBothPrompts_blockedWhenPlayServicesUnavailable() {
        ChromiumPlayServicesAvailability.setIsAvailableForTesting(false);

        assertFalse(InAppUpdatePolicy.isDiscoveryPromptAllowed(mContext));
        assertFalse(InAppUpdatePolicy.isRestartPromptAllowed(mContext));
    }

    // Discovery throttle.

    @Test
    public void testDiscoveryPrompt_allowedWhenNothingRecorded() {
        assertTrue(InAppUpdatePolicy.isDiscoveryPromptAllowed(mContext));
    }

    @Test
    public void testDiscoveryPrompt_blockedAfterUpdateDeclined() {
        InAppUpdatePolicy.recordUpdateDeclined();

        assertFalse(InAppUpdatePolicy.isDiscoveryPromptAllowed(mContext));
    }

    @Test
    public void testDiscoveryPrompt_allowedOnceDeclineWindowElapses() {
        InAppUpdatePolicy.recordUpdateDeclined();

        mFakeTimeRule.advanceMillis(
                TimeUnit.HOURS.toMillis(InAppUpdatePolicy.DEFAULT_DISCOVERY_BACKOFF_HOURS) + 1);

        assertTrue(
                "A throttle older than its window must not block",
                InAppUpdatePolicy.isDiscoveryPromptAllowed(mContext));
    }

    @Test
    public void testDiscoveryPrompt_exactBoundary() {
        InAppUpdatePolicy.recordUpdateDeclined();

        // 1 ms before the window expires: prompt must remain blocked.
        mFakeTimeRule.advanceMillis(
                TimeUnit.HOURS.toMillis(InAppUpdatePolicy.DEFAULT_DISCOVERY_BACKOFF_HOURS) - 1);
        assertFalse(InAppUpdatePolicy.isDiscoveryPromptAllowed(mContext));

        // Advancing 1 ms reaches exactly 24 hours: prompt is allowed (< vs <= boundary).
        mFakeTimeRule.advanceMillis(1);
        assertTrue(InAppUpdatePolicy.isDiscoveryPromptAllowed(mContext));
    }

    /** Regression test for the retry storm after a failed download. */
    @Test
    public void testDiscoveryPrompt_blockedAfterDownloadFailed() {
        InAppUpdatePolicy.recordDownloadFailed();

        assertFalse(InAppUpdatePolicy.isDiscoveryPromptAllowed(mContext));
    }

    @Test
    public void testDiscoveryPrompt_allowedOnceFailureWindowElapses() {
        InAppUpdatePolicy.recordDownloadFailed();

        mFakeTimeRule.advanceMillis(
                TimeUnit.HOURS.toMillis(InAppUpdatePolicy.DEFAULT_FAILURE_BACKOFF_HOURS) + 1);

        assertTrue(InAppUpdatePolicy.isDiscoveryPromptAllowed(mContext));
    }

    @Test
    public void testDiscoveryPrompt_failureExactBoundary() {
        InAppUpdatePolicy.recordDownloadFailed();

        // 1 ms before the failure window expires: prompt must remain blocked.
        mFakeTimeRule.advanceMillis(
                TimeUnit.HOURS.toMillis(InAppUpdatePolicy.DEFAULT_FAILURE_BACKOFF_HOURS) - 1);
        assertFalse(InAppUpdatePolicy.isDiscoveryPromptAllowed(mContext));

        // Advancing 1 ms reaches exactly 6 hours: prompt is allowed (< vs <= boundary).
        mFakeTimeRule.advanceMillis(1);
        assertTrue(InAppUpdatePolicy.isDiscoveryPromptAllowed(mContext));
    }

    /**
     * The failure window is deliberately shorter than the decline window, so an age that has
     * cleared the former must still be inside the latter.
     */
    @Test
    public void testFailureWindowIsShorterThanDeclineWindow() {
        long age = TimeUnit.HOURS.toMillis(InAppUpdatePolicy.DEFAULT_FAILURE_BACKOFF_HOURS) + 1;
        assertTrue(
                age < TimeUnit.HOURS.toMillis(InAppUpdatePolicy.DEFAULT_DISCOVERY_BACKOFF_HOURS));

        InAppUpdatePolicy.recordUpdateDeclined();
        mFakeTimeRule.advanceMillis(age);
        assertFalse(InAppUpdatePolicy.isDiscoveryPromptAllowed(mContext));

        clearPrefs();
        InAppUpdatePolicy.recordDownloadFailed();
        mFakeTimeRule.advanceMillis(age);
        assertTrue(InAppUpdatePolicy.isDiscoveryPromptAllowed(mContext));
    }

    // Restart throttle.

    @Test
    public void testRestartPrompt_allowedWhenNothingRecorded() {
        assertTrue(InAppUpdatePolicy.isRestartPromptAllowed(mContext));
    }

    @Test
    public void testRestartPrompt_blockedAfterRestartDeclined() {
        InAppUpdatePolicy.recordRestartDeclined();

        assertFalse(InAppUpdatePolicy.isRestartPromptAllowed(mContext));
    }

    @Test
    public void testRestartPrompt_exactBoundary() {
        InAppUpdatePolicy.recordRestartDeclined();

        // 1 ms before the restart window expires: prompt must remain blocked.
        mFakeTimeRule.advanceMillis(
                TimeUnit.HOURS.toMillis(InAppUpdatePolicy.DEFAULT_RESTART_BACKOFF_HOURS) - 1);
        assertFalse(InAppUpdatePolicy.isRestartPromptAllowed(mContext));

        // Advancing 1 ms reaches exactly 24 hours: prompt is allowed (< vs <= boundary).
        mFakeTimeRule.advanceMillis(1);
        assertTrue(InAppUpdatePolicy.isRestartPromptAllowed(mContext));
    }

    /**
     * Declining a download must not suppress the offer to relaunch into an update that is already
     * staged on the device. The payload is already there; the two decisions are unrelated.
     */
    @Test
    public void testRestartPrompt_notBlockedByDeclinedDownload() {
        InAppUpdatePolicy.recordUpdateDeclined();

        assertFalse(InAppUpdatePolicy.isDiscoveryPromptAllowed(mContext));
        assertTrue(InAppUpdatePolicy.isRestartPromptAllowed(mContext));
    }

    /** A failed download likewise says nothing about an already staged payload. */
    @Test
    public void testRestartPrompt_notBlockedByFailedDownload() {
        InAppUpdatePolicy.recordDownloadFailed();

        assertFalse(InAppUpdatePolicy.isDiscoveryPromptAllowed(mContext));
        assertTrue(InAppUpdatePolicy.isRestartPromptAllowed(mContext));
    }

    @Test
    public void testDiscoveryPrompt_notBlockedByDeclinedRestart() {
        InAppUpdatePolicy.recordRestartDeclined();

        assertFalse(InAppUpdatePolicy.isRestartPromptAllowed(mContext));
        assertTrue(InAppUpdatePolicy.isDiscoveryPromptAllowed(mContext));
    }

    @Test
    public void testClearRestartBackoff_reenablesRestartPrompt() {
        InAppUpdatePolicy.recordRestartDeclined();
        assertFalse(InAppUpdatePolicy.isRestartPromptAllowed(mContext));

        InAppUpdatePolicy.clearRestartBackoff();

        assertTrue(InAppUpdatePolicy.isRestartPromptAllowed(mContext));
    }

    // Cross-cutting behaviour.

    @Test
    @CommandLineFlags.Add({InAppUpdatePolicy.FORCE_IN_APP_UPDATE})
    public void testForceUpdate_bypassesEveryThrottle() {
        InAppUpdatePolicy.recordUpdateDeclined();
        InAppUpdatePolicy.recordDownloadFailed();
        InAppUpdatePolicy.recordRestartDeclined();

        assertTrue(InAppUpdatePolicy.isDiscoveryPromptAllowed(mContext));
        assertTrue(InAppUpdatePolicy.isRestartPromptAllowed(mContext));
    }

    /**
     * A timestamp in the future, which happens when the device clock moves backwards, fails open
     * rather than indefinitely blocking the prompt. The future timestamp is preserved in storage to
     * avoid query side effects and transient NTP desync wipeouts.
     */
    @Test
    public void testFutureTimestamp_failsOpenWithoutClearingStorage() {
        long futureTime = TimeUtils.currentTimeMillis() + TimeUnit.HOURS.toMillis(1);
        ChromeSharedPreferences.getInstance()
                .writeLong(InAppUpdatePolicy.PREF_KEY_DISCOVERY_BACKOFF, futureTime);
        ChromeSharedPreferences.getInstance()
                .writeLong(InAppUpdatePolicy.PREF_KEY_RESTART_BACKOFF, futureTime);

        assertTrue(InAppUpdatePolicy.isDiscoveryPromptAllowed(mContext));
        assertTrue(InAppUpdatePolicy.isRestartPromptAllowed(mContext));

        var prefs = ChromeSharedPreferences.getInstance();
        assertTrue(prefs.contains(InAppUpdatePolicy.PREF_KEY_DISCOVERY_BACKOFF));
        assertTrue(prefs.contains(InAppUpdatePolicy.PREF_KEY_RESTART_BACKOFF));
    }

    @Test
    public void testThrottlesAreIndependentlyStored() {
        InAppUpdatePolicy.recordUpdateDeclined();
        InAppUpdatePolicy.recordDownloadFailed();
        InAppUpdatePolicy.recordRestartDeclined();

        var prefs = ChromeSharedPreferences.getInstance();
        assertTrue(prefs.contains(InAppUpdatePolicy.PREF_KEY_DISCOVERY_BACKOFF));
        assertTrue(prefs.contains(InAppUpdatePolicy.PREF_KEY_FAILURE_BACKOFF));
        assertTrue(prefs.contains(InAppUpdatePolicy.PREF_KEY_RESTART_BACKOFF));

        InAppUpdatePolicy.clearRestartBackoff();

        assertTrue(prefs.contains(InAppUpdatePolicy.PREF_KEY_DISCOVERY_BACKOFF));
        assertTrue(prefs.contains(InAppUpdatePolicy.PREF_KEY_FAILURE_BACKOFF));
        assertFalse(prefs.contains(InAppUpdatePolicy.PREF_KEY_RESTART_BACKOFF));
    }

    // Field trial parameter overrides.

    @Test
    @EnableFeatures(
            ChromeFeatureList.IN_APP_UPDATE_FLOW
                    + ":"
                    + InAppUpdatePolicy.PARAM_DISCOVERY_BACKOFF_HOURS
                    + "/12")
    public void testDiscoveryPrompt_customDiscoveryBackoffParamHonored() {
        InAppUpdatePolicy.recordUpdateDeclined();

        // 1 ms before 12 hours: still blocked.
        mFakeTimeRule.advanceMillis(TimeUnit.HOURS.toMillis(12) - 1);
        assertFalse(InAppUpdatePolicy.isDiscoveryPromptAllowed(mContext));

        // Exactly 12 hours: allowed (under default 24h it would still be blocked).
        mFakeTimeRule.advanceMillis(1);
        assertTrue(InAppUpdatePolicy.isDiscoveryPromptAllowed(mContext));
    }

    @Test
    @EnableFeatures(
            ChromeFeatureList.IN_APP_UPDATE_FLOW
                    + ":"
                    + InAppUpdatePolicy.PARAM_FAILURE_BACKOFF_HOURS
                    + "/2")
    public void testDiscoveryPrompt_customFailureBackoffParamHonored() {
        InAppUpdatePolicy.recordDownloadFailed();

        // 1 ms before 2 hours: still blocked.
        mFakeTimeRule.advanceMillis(TimeUnit.HOURS.toMillis(2) - 1);
        assertFalse(InAppUpdatePolicy.isDiscoveryPromptAllowed(mContext));

        // Exactly 2 hours: allowed (under default 6h it would still be blocked).
        mFakeTimeRule.advanceMillis(1);
        assertTrue(InAppUpdatePolicy.isDiscoveryPromptAllowed(mContext));
    }

    @Test
    @EnableFeatures(
            ChromeFeatureList.IN_APP_UPDATE_FLOW
                    + ":"
                    + InAppUpdatePolicy.PARAM_RESTART_BACKOFF_HOURS
                    + "/8")
    public void testRestartPrompt_customRestartBackoffParamHonored() {
        InAppUpdatePolicy.recordRestartDeclined();

        // 1 ms before 8 hours: still blocked.
        mFakeTimeRule.advanceMillis(TimeUnit.HOURS.toMillis(8) - 1);
        assertFalse(InAppUpdatePolicy.isRestartPromptAllowed(mContext));

        // Exactly 8 hours: allowed (under default 24h it would still be blocked).
        mFakeTimeRule.advanceMillis(1);
        assertTrue(InAppUpdatePolicy.isRestartPromptAllowed(mContext));
    }
}

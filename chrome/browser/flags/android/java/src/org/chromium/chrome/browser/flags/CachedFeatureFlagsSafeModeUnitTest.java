// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.flags;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.LooperMode;

import org.chromium.base.FeatureList;
import org.chromium.base.task.test.ShadowPostTask;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.flags.CachedFlagsSafeMode.Behavior;

import java.util.Arrays;
import java.util.HashMap;
import java.util.Map;

/**
 * Unit Tests for the Safe Mode mechanism for {@link CachedFeatureFlags}.
 *
 * Tests the public API {@link CachedFeatureFlags} rather than the implementation
 * {@link CachedFlagsSafeMode}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(shadows = {ShadowPostTask.class})
@LooperMode(LooperMode.Mode.LEGACY)
public class CachedFeatureFlagsSafeModeUnitTest {
    private static final String CRASHY_FEATURE = "CrashyFeature";
    private static final String OK_FEATURE = "OkFeature";

    Map<String, Boolean> mDefaultsSwapped;

    @Before
    public void setUp() {
        CachedFeatureFlags.resetFlagsForTesting();
        Map<String, Boolean> defaults = makeFeatureMap(false, false);
        mDefaultsSwapped = CachedFeatureFlags.swapDefaultsForTesting(defaults);
    }

    @After
    public void tearDown() {
        CachedFeatureFlags.resetFlagsForTesting();
        CachedFeatureFlags.swapDefaultsForTesting(mDefaultsSwapped);

        FeatureList.setTestFeatures(null);
        CachedFlagsSafeMode.clearDiskForTesting();
    }

    @Test
    public void testTwoCrashesInARow_engageSafeMode() {
        startRun();
        // Crash streak is 0. Do not engage Safe Mode.
        // There are no safe values.
        // There are no cached flag values, so the defaults false/false are used.
        assertEquals(Behavior.NOT_ENGAGED_BELOW_THRESHOLD,
                CachedFeatureFlags.getSafeModeBehaviorForTesting());
        assertFalse(CachedFeatureFlags.isEnabled(CRASHY_FEATURE));
        assertFalse(CachedFeatureFlags.isEnabled(OK_FEATURE));
        endCleanRun(false, true);
        // Safe values became false/false.
        // Cached values became false/true.

        startRun();
        // Crash streak is 0. Do not engage Safe Mode.
        // Safe values are false/false.
        // Cached flag values are false/true, from previous run.
        assertEquals(Behavior.NOT_ENGAGED_BELOW_THRESHOLD,
                CachedFeatureFlags.getSafeModeBehaviorForTesting());
        assertFalse(CachedFeatureFlags.isEnabled(CRASHY_FEATURE));
        assertTrue(CachedFeatureFlags.isEnabled(OK_FEATURE));
        endCleanRun(true, true);
        // Safe values became false/true.
        // Cached values became true(crashy)/true.

        startRun();
        // Crash streak is 0. Do not engage Safe Mode.
        // Safe values are false/true.
        // Cached values remain true(crashy)/true and are used.
        assertEquals(Behavior.NOT_ENGAGED_BELOW_THRESHOLD,
                CachedFeatureFlags.getSafeModeBehaviorForTesting());
        assertTrue(CachedFeatureFlags.isEnabled(CRASHY_FEATURE));
        assertTrue(CachedFeatureFlags.isEnabled(OK_FEATURE));
        endCrashyRun();
        // Cached values remain true(crashy)/true.

        startRun();
        // Crash streak is 1. Do not engage Safe Mode.
        // Safe values are false/true.
        // Cached values remain true(crashy)/true and are used.
        assertEquals(Behavior.NOT_ENGAGED_BELOW_THRESHOLD,
                CachedFeatureFlags.getSafeModeBehaviorForTesting());
        assertTrue(CachedFeatureFlags.isEnabled(CRASHY_FEATURE));
        assertTrue(CachedFeatureFlags.isEnabled(OK_FEATURE));
        endCrashyRun();
        // Cached values remain true(crashy)/true.

        startRun();
        // Crash streak is 2. Engage Safe Mode.
        // Safe values are false/true, and are used during this run.
        // Cached values remain true(crashy)/true, but are not used because Safe Mode is engaged.
        assertEquals(Behavior.ENGAGED_WITH_SAFE_VALUES,
                CachedFeatureFlags.getSafeModeBehaviorForTesting());
        // TODO(crbug.com/1217708): Assert cached flags values are false/true.
        endCleanRun(true, false);
        // Cached values became true(crashy)/false, cached from native.

        startRun();
        // Second run of Safe Mode.
        // Safe values are false/true, and are used during this run.
        // Cached values true(crashy)/false are used, cached from native last run, but are not used
        // because Safe Mode is engaged.
        assertEquals(Behavior.ENGAGED_WITH_SAFE_VALUES,
                CachedFeatureFlags.getSafeModeBehaviorForTesting());
        // TODO(crbug.com/1217708): Assert cached flags values are false/true.
        endCleanRun(false, false);
        // Cached values became false/false, cached from native.

        startRun();
        // Crash streak is 1. Do not engage Safe Mode.
        // Safe values are false/true still.
        // Cached values false/false are used, cached from native last run.
        assertEquals(Behavior.NOT_ENGAGED_BELOW_THRESHOLD,
                CachedFeatureFlags.getSafeModeBehaviorForTesting());
        // TODO(crbug.com/1217708): Assert cached flags values are false/false.
    }

    @Test
    public void testSafeModeFetchesBadConfig_keepsStreak() {
        startRun();
        assertEquals(Behavior.NOT_ENGAGED_BELOW_THRESHOLD,
                CachedFeatureFlags.getSafeModeBehaviorForTesting());
        endCleanRun(false, true);

        startRun();
        assertEquals(Behavior.NOT_ENGAGED_BELOW_THRESHOLD,
                CachedFeatureFlags.getSafeModeBehaviorForTesting());
        endCleanRun(true, true);

        startRun();
        // Crash streak is 0. Do not engage Safe Mode.
        assertEquals(Behavior.NOT_ENGAGED_BELOW_THRESHOLD,
                CachedFeatureFlags.getSafeModeBehaviorForTesting());
        endCrashyRun();

        startRun();
        // Crash streak is 1. Do not engage Safe Mode.
        assertEquals(Behavior.NOT_ENGAGED_BELOW_THRESHOLD,
                CachedFeatureFlags.getSafeModeBehaviorForTesting());
        endCrashyRun();

        startRun();
        // Crash streak is 2. Engage Safe Mode.
        assertEquals(Behavior.ENGAGED_WITH_SAFE_VALUES,
                CachedFeatureFlags.getSafeModeBehaviorForTesting());
        endCleanRun(true, false);

        startRun();
        // Second run of safe mode.
        assertEquals(Behavior.ENGAGED_WITH_SAFE_VALUES,
                CachedFeatureFlags.getSafeModeBehaviorForTesting());
        endCleanRun(true, false);

        startRun();
        // Crash streak is 1. Do not engage Safe Mode.
        assertEquals(Behavior.NOT_ENGAGED_BELOW_THRESHOLD,
                CachedFeatureFlags.getSafeModeBehaviorForTesting());
        endCrashyRun();

        startRun();
        // Crash streak is back directly to 2. Engage Safe Mode.
        assertEquals(Behavior.ENGAGED_WITH_SAFE_VALUES,
                CachedFeatureFlags.getSafeModeBehaviorForTesting());
    }

    @Test
    public void testSafeModeFetchesGoodConfig_decreasesStreak() {
        startRun();
        assertEquals(Behavior.NOT_ENGAGED_BELOW_THRESHOLD,
                CachedFeatureFlags.getSafeModeBehaviorForTesting());
        endCleanRun(false, true);

        startRun();
        assertEquals(Behavior.NOT_ENGAGED_BELOW_THRESHOLD,
                CachedFeatureFlags.getSafeModeBehaviorForTesting());
        endCleanRun(true, true);

        startRun();
        // Crash streak is 0. Do not engage Safe Mode.
        assertEquals(Behavior.NOT_ENGAGED_BELOW_THRESHOLD,
                CachedFeatureFlags.getSafeModeBehaviorForTesting());
        endCrashyRun();

        startRun();
        // Crash streak is 1. Do not engage Safe Mode.
        assertEquals(Behavior.NOT_ENGAGED_BELOW_THRESHOLD,
                CachedFeatureFlags.getSafeModeBehaviorForTesting());
        endCrashyRun();

        startRun();
        // Crash streak is 2. Engage Safe Mode.
        assertEquals(Behavior.ENGAGED_WITH_SAFE_VALUES,
                CachedFeatureFlags.getSafeModeBehaviorForTesting());
        endCleanRun(true, false);

        startRun();
        // Second run of safe mode.
        assertEquals(Behavior.ENGAGED_WITH_SAFE_VALUES,
                CachedFeatureFlags.getSafeModeBehaviorForTesting());
        endCleanRun(true, false);

        startRun();
        // Crash streak is 1. Do not engage Safe Mode.
        assertEquals(Behavior.NOT_ENGAGED_BELOW_THRESHOLD,
                CachedFeatureFlags.getSafeModeBehaviorForTesting());
        endCleanRun(true, false);

        startRun();
        // Crash streak is down to 0. Do not engage Safe Mode.
        assertEquals(Behavior.NOT_ENGAGED_BELOW_THRESHOLD,
                CachedFeatureFlags.getSafeModeBehaviorForTesting());
        endCrashyRun();

        startRun();
        // Crash streak is 1. Do not engage Safe Mode.
        assertEquals(Behavior.NOT_ENGAGED_BELOW_THRESHOLD,
                CachedFeatureFlags.getSafeModeBehaviorForTesting());
    }

    @Test
    public void testTwoCrashesInterrupted_normalMode() {
        startRun();
        // Crash streak is 0. Do not engage Safe Mode.
        // There are no safe values.
        // There are no cached flag values, so the defaults false/false are used.
        assertEquals(Behavior.NOT_ENGAGED_BELOW_THRESHOLD,
                CachedFeatureFlags.getSafeModeBehaviorForTesting());
        assertFalse(CachedFeatureFlags.isEnabled(CRASHY_FEATURE));
        assertFalse(CachedFeatureFlags.isEnabled(OK_FEATURE));
        endCleanRun(true, true);
        // Safe values became false/false.
        // Cached values became true(flaky)/true.

        startRun();
        // Crash streak is 0. Do not engage Safe Mode.
        // Safe values are false/false.
        // Cached flag values are true(flaky)/true.
        assertEquals(Behavior.NOT_ENGAGED_BELOW_THRESHOLD,
                CachedFeatureFlags.getSafeModeBehaviorForTesting());
        assertTrue(CachedFeatureFlags.isEnabled(CRASHY_FEATURE));
        assertTrue(CachedFeatureFlags.isEnabled(OK_FEATURE));
        endCrashyRun();
        // Cached values remain true(crashy)/true.

        startRun();
        // Crash streak is 1. Do not engage Safe Mode.
        // Safe values are false/false.
        // Cached flag values are true(flaky)/true.
        assertEquals(Behavior.NOT_ENGAGED_BELOW_THRESHOLD,
                CachedFeatureFlags.getSafeModeBehaviorForTesting());
        // Cached flag values are the flaky ones cached from native.
        assertTrue(CachedFeatureFlags.isEnabled(CRASHY_FEATURE));
        assertTrue(CachedFeatureFlags.isEnabled(OK_FEATURE));
        endCleanRun(true, true);
        // Safe values became true(flaky)/true.
        // Cached values remain true(flaky)/true.

        startRun();
        // Crash streak is 0, do not engage, use flaky values.
        // Safe values are true(flaky)/true.
        // Cached flag values are true(flaky)/true.
        assertEquals(Behavior.NOT_ENGAGED_BELOW_THRESHOLD,
                CachedFeatureFlags.getSafeModeBehaviorForTesting());
        assertTrue(CachedFeatureFlags.isEnabled(CRASHY_FEATURE));
        assertTrue(CachedFeatureFlags.isEnabled(OK_FEATURE));
    }

    /**
     * Tests that decrementing the crash streak to account for an aborted run prevents Safe Mode
     * from engaging.
     */
    @Test
    public void testTwoFREs_normalMode() {
        startRun();
        // Crash streak is 0. Do not engage Safe Mode.
        // There are no safe values.
        // There are no cached flag values, so the defaults false/false are used.
        assertEquals(Behavior.NOT_ENGAGED_BELOW_THRESHOLD,
                CachedFeatureFlags.getSafeModeBehaviorForTesting());
        assertFalse(CachedFeatureFlags.isEnabled(CRASHY_FEATURE));
        assertFalse(CachedFeatureFlags.isEnabled(OK_FEATURE));
        endFirstRunWithKill();

        startRun();
        // Crash streak is 0. Do not engage Safe Mode.
        // There are no safe values.
        // There are no cached flag values, so the defaults false/false are used.
        assertEquals(Behavior.NOT_ENGAGED_BELOW_THRESHOLD,
                CachedFeatureFlags.getSafeModeBehaviorForTesting());
        assertFalse(CachedFeatureFlags.isEnabled(CRASHY_FEATURE));
        assertFalse(CachedFeatureFlags.isEnabled(OK_FEATURE));
        endFirstRunWithKill();

        startRun();
        // Crash streak is 0, do not engage, use flaky values.
        // There are no safe values.
        // There are no cached flag values, so the defaults false/false are used.
        assertEquals(Behavior.NOT_ENGAGED_BELOW_THRESHOLD,
                CachedFeatureFlags.getSafeModeBehaviorForTesting());
        assertFalse(CachedFeatureFlags.isEnabled(CRASHY_FEATURE));
        assertFalse(CachedFeatureFlags.isEnabled(OK_FEATURE));
    }

    @Test
    public void testTwoCrashesInARow_engageSafeModeWithoutSafeValues() {
        // Simulate a cache without writing safe values. This happens before Safe Mode was
        // implemented and will become rare as clients start writing safe values.
        // Cache a crashy value.
        FeatureList.setTestFeatures(makeFeatureMap(true, true));
        CachedFeatureFlags.cacheNativeFlags(Arrays.asList(CRASHY_FEATURE, OK_FEATURE));
        CachedFeatureFlags.resetFlagsForTesting();
        // Cached values became true(crashy)/true.

        startRun();
        // Crash streak is 0. Do not engage Safe Mode.
        // There are no safe values.
        // Cached values are true(crashy)/true.
        assertEquals(Behavior.NOT_ENGAGED_BELOW_THRESHOLD,
                CachedFeatureFlags.getSafeModeBehaviorForTesting());
        assertTrue(CachedFeatureFlags.isEnabled(CRASHY_FEATURE));
        assertTrue(CachedFeatureFlags.isEnabled(OK_FEATURE));
        endCrashyRun();
        // Cached values remain true(crashy)/true.

        startRun();
        // Crash streak is 1. Do not engage Safe Mode.
        // There are no safe values.
        // Cached values are true(crashy)/true.
        assertEquals(Behavior.NOT_ENGAGED_BELOW_THRESHOLD,
                CachedFeatureFlags.getSafeModeBehaviorForTesting());
        assertTrue(CachedFeatureFlags.isEnabled(CRASHY_FEATURE));
        assertTrue(CachedFeatureFlags.isEnabled(OK_FEATURE));
        endCrashyRun();
        // Cached values remain true(crashy)/true.

        startRun();
        // Crash streak is 2. Engage Safe Mode without safe values.
        // There are no safe values.
        // Cached values are true(crashy)/true, but the default values false/false are returned
        // since Safe Mode is falling back to default.
        assertEquals(Behavior.ENGAGED_WITHOUT_SAFE_VALUES,
                CachedFeatureFlags.getSafeModeBehaviorForTesting());
        // TODO(crbug.com/1217708): Assert cached flags values are false/false.
    }

    @Test
    public void testTwoCrashesInARow_engageSafeModeIgnoringOutdated() {
        startRun();
        // Crash streak is 0. Do not engage Safe Mode.
        // There are no safe values.
        // There are no cached flag values, so the defaults false/false are used.
        assertEquals(Behavior.NOT_ENGAGED_BELOW_THRESHOLD,
                CachedFeatureFlags.getSafeModeBehaviorForTesting());
        assertFalse(CachedFeatureFlags.isEnabled(CRASHY_FEATURE));
        assertFalse(CachedFeatureFlags.isEnabled(OK_FEATURE));
        endCleanRun(false, true);
        // Safe values became false/false.
        // Cached values became false/true.

        startRun();
        // Crash streak is 0. Do not engage Safe Mode.
        // Safe values are false/false.
        // Cached flag values are false/true, from previous run.
        assertEquals(Behavior.NOT_ENGAGED_BELOW_THRESHOLD,
                CachedFeatureFlags.getSafeModeBehaviorForTesting());
        assertFalse(CachedFeatureFlags.isEnabled(CRASHY_FEATURE));
        assertTrue(CachedFeatureFlags.isEnabled(OK_FEATURE));
        endCleanRun(true, true);
        // Safe values became false/true.
        // Cached values became true(crashy)/true.

        // Pretend safe values are from an older version
        CachedFlagsSafeMode.getSafeValuePreferences()
                .edit()
                .putString(CachedFlagsSafeMode.PREF_SAFE_VALUES_VERSION, "1.0.0.0")
                .apply();

        startRun();
        // Crash streak is 0. Do not engage Safe Mode.
        // Safe values are false/true, but from another version.
        // Cached values are true(crashy)/true.
        assertEquals(Behavior.NOT_ENGAGED_BELOW_THRESHOLD,
                CachedFeatureFlags.getSafeModeBehaviorForTesting());
        assertTrue(CachedFeatureFlags.isEnabled(CRASHY_FEATURE));
        assertTrue(CachedFeatureFlags.isEnabled(OK_FEATURE));
        endCrashyRun();
        // Cached values remain true(crashy)/true.

        startRun();
        // Crash streak is 1. Do not engage Safe Mode.
        // Safe values are false/true, but from another version.
        // Cached values are true(crashy)/true.
        assertEquals(Behavior.NOT_ENGAGED_BELOW_THRESHOLD,
                CachedFeatureFlags.getSafeModeBehaviorForTesting());
        assertTrue(CachedFeatureFlags.isEnabled(CRASHY_FEATURE));
        assertTrue(CachedFeatureFlags.isEnabled(OK_FEATURE));
        endCrashyRun();
        // Cached values remain true(crashy)/true.

        startRun();
        assertEquals(Behavior.ENGAGED_IGNORING_OUTDATED_SAFE_VALUES,
                CachedFeatureFlags.getSafeModeBehaviorForTesting());
        // Crash streak is 2. Engage Safe Mode with obsolete safe values.
        // Safe values are false/true, but from another version.
        // Cached values are true(crashy)/true, but the default values false/false are returned
        // since Safe Mode is falling back to default.
        // TODO(crbug.com/1217708): Assert cached flags values are false/false.
    }

    @Test
    public void testMultipleStartCheckpoints_normalMode() {
        startRun();
        // Crash streak is 0. Do not engage Safe Mode.
        // There are no safe values.
        // There are no cached flag values, so the defaults false/false are used.
        assertEquals(Behavior.NOT_ENGAGED_BELOW_THRESHOLD,
                CachedFeatureFlags.getSafeModeBehaviorForTesting());
        assertFalse(CachedFeatureFlags.isEnabled(CRASHY_FEATURE));
        assertFalse(CachedFeatureFlags.isEnabled(OK_FEATURE));
        endCleanRun(true, true);
        // Safe values became false/false.
        // Cached values became true(flaky)/true.

        startRun();
        startRun();
        startRun();
        startRun();
        // Crash streak is 0. Do not engage Safe Mode.
        // Safe values are false/false.
        // Cached flag values are true(flaky)/true.
        assertEquals(Behavior.NOT_ENGAGED_BELOW_THRESHOLD,
                CachedFeatureFlags.getSafeModeBehaviorForTesting());
        assertTrue(CachedFeatureFlags.isEnabled(CRASHY_FEATURE));
        assertTrue(CachedFeatureFlags.isEnabled(OK_FEATURE));
        endCrashyRun();
        // Cached values remain true(crashy)/true.

        startRun();
        // Crash streak is 1, despite the multiple startRun() calls above. Do not engage Safe Mode.
        assertEquals(Behavior.NOT_ENGAGED_BELOW_THRESHOLD,
                CachedFeatureFlags.getSafeModeBehaviorForTesting());
    }

    private void startRun() {
        CachedFeatureFlags.isEnabled(CRASHY_FEATURE);
        CachedFeatureFlags.onStartOrResumeCheckpoint();
    }

    private void endFirstRunWithKill() {
        CachedFeatureFlags.onPauseCheckpoint();
        CachedFeatureFlags.resetFlagsForTesting();
    }

    private void endCrashyRun() {
        CachedFeatureFlags.resetFlagsForTesting();
    }

    private void endCleanRun(boolean crashyFeatureValue, boolean okFeatureValue) {
        FeatureList.setTestFeatures(makeFeatureMap(crashyFeatureValue, okFeatureValue));
        CachedFeatureFlags.cacheNativeFlags(Arrays.asList(CRASHY_FEATURE, OK_FEATURE));

        CachedFeatureFlags.onEndCheckpoint();
        // Async task writing values should have run synchronously because of ShadowPostTask.
        assertTrue(CachedFlagsSafeMode.getSafeValuePreferences().contains(
                "Chrome.Flags.CachedFlag.CrashyFeature"));

        CachedFeatureFlags.resetFlagsForTesting();
    }

    private HashMap<String, Boolean> makeFeatureMap(
            boolean crashyFeatureValue, boolean okFeatureValue) {
        return new HashMap<String, Boolean>() {
            {
                put(CRASHY_FEATURE, crashyFeatureValue);
                put(OK_FEATURE, okFeatureValue);
            }
        };
    }
}

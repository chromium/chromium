// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.prefs;

import static com.google.common.truth.Truth.assertWithMessage;

import androidx.test.filters.SmallTest;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.components.prefs.PrefService;

/** Tests for {@link LocalStatePrefs}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
public class LocalStatePrefsTest {

    private static final String TEST_PREF_KEY = Pref.SSL_VERSION_MIN;
    private static final String TEST_VALUE = "tls1.3";

    @Rule
    public FreshCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();

    @Test
    @SmallTest
    public void testPref() {
        mActivityTestRule.startOnBlankPage();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    PrefService prefService = LocalStatePrefs.get();
                    String existingValue =
                            prefService.hasPrefPath(TEST_PREF_KEY)
                                    ? prefService.getString(TEST_PREF_KEY)
                                    : TEST_VALUE;
                    prefService.setString(TEST_PREF_KEY, TEST_VALUE);
                    assertWithMessage("Expected pref key to be set to testing value")
                            .that(prefService.getString(TEST_PREF_KEY))
                            .isEqualTo(TEST_VALUE);
                    assertWithMessage(
                                    "Expected "
                                            + TEST_PREF_KEY
                                            + " preference key to be marked as set")
                            .that(prefService.hasPrefPath(TEST_PREF_KEY))
                            .isTrue();
                    prefService.setString(TEST_PREF_KEY, existingValue);
                    assertWithMessage("Expected pref key to be correct after being set back")
                            .that(prefService.getString(TEST_PREF_KEY))
                            .isEqualTo(existingValue);
                });
    }
}

// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.prefs;

import static com.google.common.truth.Truth.assertWithMessage;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.FeatureList;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.components.prefs.PrefService;
import org.chromium.content_public.browser.BrowserStartupController;

import java.util.concurrent.atomic.AtomicBoolean;

/** Tests for {@link LocalStatePrefs}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@DoNotBatch(
        reason =
                "Tests LocalStatePrefs before BrowserStartupController.isFullBrowserStarted()"
                        + " becomes true.")
public class LocalStatePrefsTest {

    private static final String TEST_PREF_KEY = Pref.SSL_VERSION_MIN;
    private static final String TEST_VALUE = "tls1.3";

    @Rule
    public FreshCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();

    @Test
    @SmallTest
    public void testPref() {
        verifyPrefAcrossStartup(/* expectEarlyFeatureList= */ true);
    }

    @Test
    @SmallTest
    public void testObserver() {
        AtomicBoolean called = new AtomicBoolean(false);
        Runnable observer = () -> called.set(true);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    LocalStatePrefs.setNativePrefsLoadedForTesting(false);
                    LocalStatePrefs.addObserver(observer);
                });

        assertWithMessage("Observer should not be called yet").that(called.get()).isFalse();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    LocalStatePrefs.setNativePrefsLoadedForTesting(true);
                });

        assertWithMessage("Observer should be called").that(called.get()).isTrue();

        called.set(false);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    LocalStatePrefs.removeObserver(observer);
                    LocalStatePrefs.setNativePrefsLoadedForTesting(false);
                    LocalStatePrefs.setNativePrefsLoadedForTesting(true);
                });
        assertWithMessage("Observer should not be called after removal")
                .that(called.get())
                .isFalse();
    }

    @Test
    @SmallTest
    // Equivalent to VariationsSwitches.VARIATIONS_SERVER_URL.
    @CommandLineFlags.Add("variations-server-url=http://127.0.0.1:4/seed")
    public void testEarlyLocalStateOnly() {
        verifyPrefAcrossStartup(/* expectEarlyFeatureList= */ false);
    }

    private void verifyPrefAcrossStartup(boolean expectEarlyFeatureList) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertWithMessage(
                                    "Full browser should not be started before startOnBlankPage()")
                            .that(BrowserStartupController.getInstance().isFullBrowserStarted())
                            .isFalse();
                    Assert.assertEquals(expectEarlyFeatureList, FeatureList.isNativeInitialized());
                    assertWithMessage("Expected areNativePrefsLoaded() to return true")
                            .that(LocalStatePrefs.areNativePrefsLoaded())
                            .isTrue();
                });

        try {
            ThreadUtils.runOnUiThreadBlocking(
                    () -> {
                        PrefService prefService = LocalStatePrefs.get();
                        assertWithMessage("Expected pref key to be unset initially")
                                .that(prefService.hasPrefPath(TEST_PREF_KEY))
                                .isFalse();
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
                    });

            mActivityTestRule.startOnBlankPage();

            ThreadUtils.runOnUiThreadBlocking(
                    () -> {
                        assertWithMessage("FeatureList should be initialized after browser startup")
                                .that(FeatureList.isNativeInitialized())
                                .isTrue();
                        assertWithMessage("Full browser should be started after startOnBlankPage()")
                                .that(BrowserStartupController.getInstance().isFullBrowserStarted())
                                .isTrue();
                        PrefService prefService = LocalStatePrefs.get();
                        assertWithMessage(
                                        "Expected pref value to survive handoff to"
                                                + " BrowserProcessImpl")
                                .that(prefService.getString(TEST_PREF_KEY))
                                .isEqualTo(TEST_VALUE);
                    });
        } finally {
            ThreadUtils.runOnUiThreadBlocking(() -> LocalStatePrefs.get().clearPref(TEST_PREF_KEY));
        }
    }
}

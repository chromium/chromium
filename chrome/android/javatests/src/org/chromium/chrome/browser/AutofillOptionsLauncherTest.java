// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import static androidx.test.espresso.intent.Intents.intended;
import static androidx.test.espresso.intent.matcher.IntentMatchers.hasComponent;
import static androidx.test.espresso.intent.matcher.IntentMatchers.hasExtra;

import static org.hamcrest.Matchers.allOf;

import static org.chromium.chrome.browser.autofill.AutofillClientProviderUtils.setAutofillOptionsDeepLinkPref;

import android.content.Intent;

import androidx.test.core.app.ActivityScenario;
import androidx.test.core.app.ApplicationProvider;
import androidx.test.espresso.intent.Intents;
import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.chrome.browser.autofill.options.AutofillOptionsFragment;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.settings.SettingsActivity;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;

/** Tests for the {@link AutofillOptionsLauncher}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@DoNotBatch(reason = "crbug.com/391296691: Replace ActivityScenario with ActivityScenarioRule.")
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class AutofillOptionsLauncherTest {

    @Before
    public void setup() {
        Intents.init();
    }

    @After
    public void tearDown() {
        Intents.release();
    }

    @Test
    @MediumTest
    public void testLauncherStartsAutofillOptionsFragment() {
        setAutofillOptionsDeepLinkPref(true);
        launchActivity();
        intended(
                allOf(
                        hasComponent(SettingsActivity.class.getName()),
                        hasExtra(
                                SettingsActivity.EXTRA_SHOW_FRAGMENT,
                                AutofillOptionsFragment.class.getName())));
    }

    @Test
    @MediumTest
    public void testAutofillOptionsFragmentNotStartedWithDeepLinkFeatureOff() {
        setAutofillOptionsDeepLinkPref(false);
        launchActivity();
        intended(hasComponent(SettingsActivity.class.getName()), Intents.times(0));
    }

    public void launchActivity() {
        Intent intent = new Intent(Intent.ACTION_APPLICATION_PREFERENCES);
        intent.addCategory(Intent.CATEGORY_DEFAULT);
        intent.addCategory(Intent.CATEGORY_APP_BROWSER);
        intent.addCategory(Intent.CATEGORY_PREFERENCE);
        // This intent is implicit, so any of the installed Chrome channels could run it.
        // To make the test execution reliable on any device or emulator, regardless of the amount
        // of Chrome apks installed on it, the package name is set so the intent would be called
        // for the same Chrome apk every time. If the package wasn't set, the intent would start on
        // its own when there is only one Chrome and for the case of multiple Chromes, the picker
        // UI would show up. The picker is part of the Android UI and can't be reliably tested, so
        // it's deliberately avoided here.
        intent.setPackage(ApplicationProvider.getApplicationContext().getPackageName());
        ActivityScenario.launchActivityForResult(intent);
    }
}

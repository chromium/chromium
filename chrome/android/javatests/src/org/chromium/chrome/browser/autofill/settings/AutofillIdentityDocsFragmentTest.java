// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.settings;

import androidx.test.filters.SmallTest;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.settings.SettingsActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;

/** Tests for {@link AutofillIdentityDocsFragment}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class AutofillIdentityDocsFragmentTest {
    @Rule
    public SettingsActivityTestRule<AutofillIdentityDocsFragment> mSettingsActivityTestRule =
            new SettingsActivityTestRule<>(AutofillIdentityDocsFragment.class);

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.YOUR_SAVED_INFO_SETTINGS_PAGE_ANDROID)
    public void testIdentityDocsFragmentToggleVisible() {
        mSettingsActivityTestRule.startSettingsActivity();

         // TODO(crbug.com/482994257): Tests in next CLs once the fragment is implemented.
    }
}

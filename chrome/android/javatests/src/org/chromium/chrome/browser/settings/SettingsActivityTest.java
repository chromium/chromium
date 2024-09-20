// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings;

import static org.junit.Assert.assertEquals;

import android.content.Intent;
import android.graphics.Color;

import androidx.fragment.app.Fragment;
import androidx.test.filters.SmallTest;
import androidx.test.runner.lifecycle.Stage;

import org.hamcrest.Matchers;
import org.junit.After;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.ApplicationTestUtils;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.about_settings.AboutChromeSettings;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.ui.test.util.DeviceRestriction;

/** Tests for the Settings menu. */
@RunWith(ChromeJUnit4ClassRunner.class)
@DoNotBatch(reason = "Tests cannot run batched because they launch a Settings activity.")
public class SettingsActivityTest {
    @Rule
    public SettingsActivityTestRule<MainSettings> mSettingsActivityTestRule =
            new SettingsActivityTestRule<>(MainSettings.class);

    @After
    public void tearDown() {
        mSettingsActivityTestRule.getActivity().finish();
    }

    /** Test status bar is always black in Automotive devices. */
    @Test
    @SmallTest
    @Feature({"StatusBar, Automotive Toolbar"})
    @Restriction(DeviceRestriction.RESTRICTION_TYPE_AUTO)
    public void testStatusBarBlackInAutomotive() {
        mSettingsActivityTestRule.startSettingsActivity();
        assertEquals(
                "Status bar should always be black in automotive devices.",
                Color.BLACK,
                mSettingsActivityTestRule.getActivity().getWindow().getStatusBarColor());
    }

    @Test
    @SmallTest
    @EnableFeatures({ChromeFeatureList.SETTINGS_SINGLE_ACTIVITY})
    public void testStandaloneFragments() {
        // Start the main settings, which is an embeddable fragment.
        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();

        // Open an embeddable fragment. This does NOT start a new activity.
        final Intent intent1 =
                SettingsIntentUtil.createIntent(
                        activity, AboutChromeSettings.class.getName(), null);
        activity.startActivity(intent1);
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            activity.getMainFragment(),
                            Matchers.instanceOf(AboutChromeSettings.class));
                });

        // Open a standalone fragment. This will create a new activity.
        final Intent intent2 =
                SettingsIntentUtil.createIntent(activity, TestFragment.class.getName(), null);
        ApplicationTestUtils.waitForActivityWithClass(
                SettingsActivity.class, Stage.CREATED, () -> activity.startActivity(intent2));

        // Open an embeddable fragment. This starts a new activity as the last fragment is
        // standalone.
        final Intent intent3 =
                SettingsIntentUtil.createIntent(activity, MainSettings.class.getName(), null);
        ApplicationTestUtils.waitForActivityWithClass(
                SettingsActivity.class, Stage.CREATED, () -> activity.startActivity(intent3));
    }

    public static class TestFragment extends Fragment {}
}

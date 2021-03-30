// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.allOf;

import android.content.Context;
import android.content.Intent;
import android.support.test.InstrumentationRegistry;

import androidx.preference.PreferenceFragmentCompat;
import androidx.preference.PreferenceScreen;
import androidx.test.espresso.ViewInteraction;
import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.browser_ui.settings.ChromeImageViewPreference;
import org.chromium.components.browser_ui.settings.R;
import org.chromium.components.browser_ui.settings.SettingsLauncher;

/**
 * Tests of {@link ChromeImageViewPreference}.
 *
 * TODO(crbug.com/1166810): Move these tests to //components/browser_ui/settings/.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
public class ChromeImageViewPreferenceTest {
    @Rule
    public BaseActivityTestRule<SettingsActivity> mRule =
            new BaseActivityTestRule<>(SettingsActivity.class);

    private PreferenceScreen mPreferenceScreen;
    private Context mContext;

    private static final String TITLE = "Preference Title";
    private static final String SUMMARY = "This is a summary.";
    private static final int DRAWABLE_RES = R.drawable.ic_folder_blue_24dp;
    private static final int CONTENT_DESCRIPTION_RES = R.string.ok;

    @Before
    public void setUp() {
        SettingsLauncher settingsLauncher = new SettingsLauncherImpl();
        Intent intent = settingsLauncher.createSettingsActivityIntent(
                InstrumentationRegistry.getInstrumentation().getContext(),
                DummySettingsForTest.class.getName());
        mRule.launchActivity(intent);

        PreferenceFragmentCompat fragment =
                (PreferenceFragmentCompat) mRule.getActivity().getMainFragment();
        mPreferenceScreen = fragment.getPreferenceScreen();
        mContext = fragment.getPreferenceManager().getContext();
    }

    @Test
    @SmallTest
    public void testChromeImageViewPreference() {
        ChromeImageViewPreference preference = new ChromeImageViewPreference(mContext);
        preference.setTitle(TITLE);
        preference.setSummary(SUMMARY);
        preference.setImageView(DRAWABLE_RES, CONTENT_DESCRIPTION_RES, null);
        mPreferenceScreen.addPreference(preference);

        Assert.assertTrue(preference.isEnabled());

        getTitleView().check(matches(allOf(withText(TITLE), isDisplayed())));
        getSummaryView().check(matches(allOf(withText(SUMMARY), isDisplayed())));
        getImageViewWidget().check(matches(isDisplayed()));
    }

    @Test
    @SmallTest
    public void testChromeImageViewPreferenceManaged() {
        ChromeImageViewPreference preference = new ChromeImageViewPreference(mContext);
        preference.setTitle(TITLE);
        preference.setImageView(DRAWABLE_RES, CONTENT_DESCRIPTION_RES, null);
        preference.setManagedPreferenceDelegate(ManagedPreferencesUtilsTest.POLICY_DELEGATE);
        mPreferenceScreen.addPreference(preference);

        Assert.assertFalse(preference.isEnabled());

        getTitleView().check(matches(allOf(withText(TITLE), isDisplayed())));
        getSummaryView().check(
                matches(allOf(withText(R.string.managed_by_your_organization), isDisplayed())));
        getImageViewWidget().check(matches(isDisplayed()));
    }

    private ViewInteraction getTitleView() {
        return onView(withId(android.R.id.title));
    }

    private ViewInteraction getSummaryView() {
        return onView(withId(android.R.id.summary));
    }

    private ViewInteraction getImageViewWidget() {
        return onView(withId(R.id.image_view_widget));
    }
}

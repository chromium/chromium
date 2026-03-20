// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.settings;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withParent;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.allOf;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoInteractions;

import androidx.preference.Preference;
import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.settings.SettingsActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.R;
import org.chromium.components.browser_ui.settings.search.SettingsIndexData;

/** Tests for {@link HomeOfTransactionsFragment}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class HomeOfTransactionsFragmentTest {
    @Rule
    public SettingsActivityTestRule<HomeOfTransactionsFragment> mSettingsActivityTestRule =
            new SettingsActivityTestRule<>(HomeOfTransactionsFragment.class);

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private SettingsIndexData mSearchIndexDataMock;
    @Mock private Profile mProfileMock;

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.YOUR_SAVED_INFO_SETTINGS_PAGE_ANDROID})
    public void testHomeOfTransactionsFormsAiPreferencesVisible() {
        mSettingsActivityTestRule.startSettingsActivity();
        HomeOfTransactionsFragment fragment = mSettingsActivityTestRule.getFragment();

        Preference identityDocsPref =
                fragment.findPreference(HomeOfTransactionsFragment.PREF_AUTOFILL_IDENTITY_DOCS);
        assertTrue(identityDocsPref.isVisible());

        Preference travelPref =
                fragment.findPreference(HomeOfTransactionsFragment.PREF_AUTOFILL_TRAVEL);
        assertTrue(travelPref.isVisible());
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.YOUR_SAVED_INFO_SETTINGS_PAGE_ANDROID)
    public void testSearchIndexWhenAllEnabled() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    HomeOfTransactionsFragment.SEARCH_INDEX_DATA_PROVIDER.updateDynamicPreferences(
                            mSettingsActivityTestRule.getActivity(),
                            mSearchIndexDataMock,
                            mProfileMock);
                });

        verifyNoInteractions(mSearchIndexDataMock);
    }

    @Test
    @SmallTest
    @DisableFeatures(ChromeFeatureList.YOUR_SAVED_INFO_SETTINGS_PAGE_ANDROID)
    public void testSearchIndexEmptyWhenFeatureDisabled() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    HomeOfTransactionsFragment.SEARCH_INDEX_DATA_PROVIDER.updateDynamicPreferences(
                            mSettingsActivityTestRule.getActivity(),
                            mSearchIndexDataMock,
                            mProfileMock);
                });

        verify(mSearchIndexDataMock)
                .removeEntry(
                        HomeOfTransactionsFragment.SEARCH_INDEX_DATA_PROVIDER.getUniqueId(
                                HomeOfTransactionsFragment.PREF_PASSWORDS));
        verify(mSearchIndexDataMock)
                .removeEntry(
                        HomeOfTransactionsFragment.SEARCH_INDEX_DATA_PROVIDER.getUniqueId(
                                HomeOfTransactionsFragment.PREF_AUTOFILL_PAYMENTS));
        verify(mSearchIndexDataMock)
                .removeEntry(
                        HomeOfTransactionsFragment.SEARCH_INDEX_DATA_PROVIDER.getUniqueId(
                                HomeOfTransactionsFragment.PREF_AUTOFILL_ADDRESSES));
        verify(mSearchIndexDataMock)
                .removeEntry(
                        HomeOfTransactionsFragment.SEARCH_INDEX_DATA_PROVIDER.getUniqueId(
                                HomeOfTransactionsFragment.PREF_AUTOFILL_SETTINGS));
        verify(mSearchIndexDataMock)
                .removeEntry(
                        HomeOfTransactionsFragment.SEARCH_INDEX_DATA_PROVIDER.getUniqueId(
                                HomeOfTransactionsFragment.PREF_AUTOFILL_IDENTITY_DOCS));
        verify(mSearchIndexDataMock)
                .removeEntry(
                        HomeOfTransactionsFragment.SEARCH_INDEX_DATA_PROVIDER.getUniqueId(
                                HomeOfTransactionsFragment.PREF_AUTOFILL_TRAVEL));
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.YOUR_SAVED_INFO_SETTINGS_PAGE_ANDROID)
    public void testClickPaymentsLaunchesPayments() {
        mSettingsActivityTestRule.startSettingsActivity();

        onView(withText(R.string.autofill_payments_title)).perform(click());

        onView(
                        allOf(
                                withText(R.string.autofill_payments_title),
                                withParent(withId(R.id.action_bar))))
                .check(matches(isDisplayed()));
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.YOUR_SAVED_INFO_SETTINGS_PAGE_ANDROID)
    public void testClickContactInfoLaunchesContactInfo() {
        mSettingsActivityTestRule.startSettingsActivity();

        onView(withText(R.string.autofill_contact_info_title)).perform(click());

        onView(
                        allOf(
                                withText(R.string.autofill_contact_info_title),
                                withParent(withId(R.id.action_bar))))
                .check(matches(isDisplayed()));
    }
}

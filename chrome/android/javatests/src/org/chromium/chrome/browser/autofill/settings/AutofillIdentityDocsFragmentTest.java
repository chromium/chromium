// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.settings;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.matcher.ViewMatchers.withId;

import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoInteractions;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.feedback.HelpAndFeedbackLauncher;
import org.chromium.chrome.browser.feedback.HelpAndFeedbackLauncherFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.settings.SettingsActivity;
import org.chromium.chrome.browser.settings.SettingsActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.browser_ui.settings.search.SettingsIndexData;

/** Tests for {@link AutofillIdentityDocsFragment}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class AutofillIdentityDocsFragmentTest {
    @Rule
    public SettingsActivityTestRule<AutofillIdentityDocsFragment> mSettingsActivityTestRule =
            new SettingsActivityTestRule<>(AutofillIdentityDocsFragment.class);

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private HelpAndFeedbackLauncher mHelpAndFeedbackLauncher;
    @Mock private SettingsIndexData mSearchIndexDataMock;
    @Mock private Profile mProfileMock;

    @Before
    public void setUp() {
        HelpAndFeedbackLauncherFactory.setInstanceForTesting(mHelpAndFeedbackLauncher);
    }

    @Test
    @SmallTest
    public void testHelpMenuTriggersAutofillHelp() {
        SettingsActivity settingsActivity = mSettingsActivityTestRule.startSettingsActivity();

        onView(withId(R.id.menu_id_targeted_help)).perform(click());

        verify(mHelpAndFeedbackLauncher)
                .show(
                        settingsActivity,
                        ContextUtils.getApplicationContext()
                                .getString(R.string.help_context_autofill),
                        /* url= */ null);
    }

    @Test
    @SmallTest
    @EnableFeatures({
        ChromeFeatureList.YOUR_SAVED_INFO_SETTINGS_PAGE_ANDROID,
        ChromeFeatureList.AUTOFILL_AI_WITH_DATA_SCHEMA
    })
    public void testSearchIndexWhenAllEnabled() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    AutofillIdentityDocsFragment.SEARCH_INDEX_DATA_PROVIDER
                            .updateDynamicPreferences(
                                    mSettingsActivityTestRule.getActivity(),
                                    mSearchIndexDataMock,
                                    mProfileMock);
                });

        verifyNoInteractions(mSearchIndexDataMock);
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.YOUR_SAVED_INFO_SETTINGS_PAGE_ANDROID)
    @DisableFeatures(ChromeFeatureList.AUTOFILL_AI_WITH_DATA_SCHEMA)
    public void testSearchIndexEmptyWhenFeatureDisabled() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    AutofillIdentityDocsFragment.SEARCH_INDEX_DATA_PROVIDER
                            .updateDynamicPreferences(
                                    mSettingsActivityTestRule.getActivity(),
                                    mSearchIndexDataMock,
                                    mProfileMock);
                });

        verify(mSearchIndexDataMock)
                .removeEntry(
                        AutofillIdentityDocsFragment.SEARCH_INDEX_DATA_PROVIDER.getUniqueId(
                                AutofillIdentityDocsFragment.PREF_OPT_IN_TOGGLE));
    }
}

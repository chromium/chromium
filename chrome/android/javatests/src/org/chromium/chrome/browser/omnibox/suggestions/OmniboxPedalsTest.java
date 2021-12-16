// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import android.app.Activity;

import androidx.test.filters.MediumTest;

import org.hamcrest.Matchers;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.omnibox.LocationBarLayout;
import org.chromium.chrome.browser.omnibox.UrlBar;
import org.chromium.chrome.browser.omnibox.action.OmniboxPedalType;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.OmniboxTestUtils;
import org.chromium.chrome.test.util.WaitForFocusHelper;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.components.omnibox.AutocompleteMatch;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

/**
 * Tests of the Omnibox Pedals feature.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class OmniboxPedalsTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Before
    public void setUp() throws InterruptedException {
        mActivityTestRule.startMainActivityOnBlankPage();
    }

    /**
     * Type the |text| into |activity|'s url_bar.
     *
     * @param activity The Activity which url_bar is in.
     * @param text The text will be typed into url_bar.
     */
    private void typeInOmnibox(Activity activity, String text) throws InterruptedException {
        final UrlBar urlBar = activity.findViewById(R.id.url_bar);
        Assert.assertNotNull(urlBar);

        WaitForFocusHelper.acquireFocusForView(urlBar);
        OmniboxTestUtils.waitForFocusAndKeyboardActive(urlBar, true);

        TestThreadUtils.runOnUiThreadBlocking(() -> { urlBar.setText(text); });
    }

    /**
     * Find the Omnibox Pedal suggestion which suggests the |pedalType|, and return the
     * suggestion. This method needs to run on the UI thread.
     *
     * @param locationBarLayout The layout which omnibox suggestions will show in.
     * @param pedalType The Omnibox pedal type to be found.
     * @return The suggesstion which suggests the matching OmniboxPedalType.
     */
    private AutocompleteMatch findOmniboxPedalSuggestion(
            LocationBarLayout locationBarLayout, @OmniboxPedalType int pedalType) {
        ThreadUtils.assertOnUiThread();

        AutocompleteCoordinator coordinator = locationBarLayout.getAutocompleteCoordinator();
        // Find the first matching suggestion.
        for (int i = 0; i < coordinator.getSuggestionCount(); ++i) {
            AutocompleteMatch suggestion = coordinator.getSuggestionAt(i);
            if (suggestion != null && suggestion.getOmniboxPedal() != null
                    && suggestion.getOmniboxPedal().getID() == pedalType) {
                return suggestion;
            }
        }
        return null;
    }

    @Test
    @MediumTest
    @EnableFeatures("OmniboxPedalsAndroidBatch1")
    public void testClearBrowsingDataOmniboxPedalSuggestion() throws InterruptedException {
        LocationBarLayout locationBarLayout =
                (LocationBarLayout) mActivityTestRule.getActivity().findViewById(R.id.location_bar);
        typeInOmnibox(mActivityTestRule.getActivity(), "Clear data");

        CriteriaHelper.pollUiThread(() -> {
            AutocompleteMatch matchSuggestion = findOmniboxPedalSuggestion(
                    locationBarLayout, OmniboxPedalType.CLEAR_BROWSING_DATA);
            Criteria.checkThat(matchSuggestion, Matchers.notNullValue());
        });
    }

    @Test
    @MediumTest
    @EnableFeatures("OmniboxPedalsAndroidBatch1")
    public void testManagePasswordsOmniboxPedalSuggestion() throws InterruptedException {
        LocationBarLayout locationBarLayout =
                (LocationBarLayout) mActivityTestRule.getActivity().findViewById(R.id.location_bar);
        typeInOmnibox(mActivityTestRule.getActivity(), "Manage passwords");

        CriteriaHelper.pollUiThread(() -> {
            AutocompleteMatch matchSuggestion = findOmniboxPedalSuggestion(
                    locationBarLayout, OmniboxPedalType.MANAGE_PASSWORDS);
            Criteria.checkThat(matchSuggestion, Matchers.notNullValue());
        });
    }

    @Test
    @MediumTest
    @EnableFeatures("OmniboxPedalsAndroidBatch1")
    public void testManagePaymentMethodsOmniboxPedalSuggestion() throws InterruptedException {
        LocationBarLayout locationBarLayout =
                (LocationBarLayout) mActivityTestRule.getActivity().findViewById(R.id.location_bar);
        typeInOmnibox(mActivityTestRule.getActivity(), "Manage payment methods");

        CriteriaHelper.pollUiThread(() -> {
            AutocompleteMatch matchSuggestion = findOmniboxPedalSuggestion(
                    locationBarLayout, OmniboxPedalType.UPDATE_CREDIT_CARD);
            Criteria.checkThat(matchSuggestion, Matchers.notNullValue());
        });
    }

    @Test
    @MediumTest
    @EnableFeatures("OmniboxPedalsAndroidBatch1")
    public void testOpenIncognitoTabOmniboxPedalSuggestion() throws InterruptedException {
        LocationBarLayout locationBarLayout =
                (LocationBarLayout) mActivityTestRule.getActivity().findViewById(R.id.location_bar);
        typeInOmnibox(mActivityTestRule.getActivity(), "Open Incognito");

        CriteriaHelper.pollUiThread(() -> {
            AutocompleteMatch matchSuggestion = findOmniboxPedalSuggestion(
                    locationBarLayout, OmniboxPedalType.LAUNCH_INCOGNITO);
            Criteria.checkThat(matchSuggestion, Matchers.notNullValue());
        });
    }

    @Test
    @MediumTest
    @EnableFeatures("OmniboxPedalsAndroidBatch1")
    public void testRunChromeSafetyCheckOmniboxPedalSuggestion() throws InterruptedException {
        LocationBarLayout locationBarLayout =
                (LocationBarLayout) mActivityTestRule.getActivity().findViewById(R.id.location_bar);
        typeInOmnibox(mActivityTestRule.getActivity(), "Run safety check");

        CriteriaHelper.pollUiThread(() -> {
            AutocompleteMatch matchSuggestion = findOmniboxPedalSuggestion(
                    locationBarLayout, OmniboxPedalType.RUN_CHROME_SAFETY_CHECK);
            Criteria.checkThat(matchSuggestion, Matchers.notNullValue());
        });
    }

    @Test
    @MediumTest
    @EnableFeatures("OmniboxPedalsAndroidBatch1")
    public void testManageSiteSettingsOmniboxPedalSuggestion() throws InterruptedException {
        LocationBarLayout locationBarLayout =
                (LocationBarLayout) mActivityTestRule.getActivity().findViewById(R.id.location_bar);
        typeInOmnibox(mActivityTestRule.getActivity(), "Change site permissions");

        CriteriaHelper.pollUiThread(() -> {
            AutocompleteMatch matchSuggestion = findOmniboxPedalSuggestion(
                    locationBarLayout, OmniboxPedalType.MANAGE_SITE_SETTINGS);
            Criteria.checkThat(matchSuggestion, Matchers.notNullValue());
        });
    }

    @Test
    @MediumTest
    @EnableFeatures("OmniboxPedalsAndroidBatch1")
    public void testManageChromeSettingsOmniboxPedalSuggestion() throws InterruptedException {
        LocationBarLayout locationBarLayout =
                (LocationBarLayout) mActivityTestRule.getActivity().findViewById(R.id.location_bar);
        typeInOmnibox(mActivityTestRule.getActivity(), "manage settings");

        CriteriaHelper.pollUiThread(() -> {
            AutocompleteMatch matchSuggestion = findOmniboxPedalSuggestion(
                    locationBarLayout, OmniboxPedalType.MANAGE_CHROME_SETTINGS);
            Criteria.checkThat(matchSuggestion, Matchers.notNullValue());
        });
    }

    @Test
    @MediumTest
    @EnableFeatures("OmniboxPedalsAndroidBatch1")
    public void testViewYourChromeHistoryOmniboxPedalSuggestion() throws InterruptedException {
        LocationBarLayout locationBarLayout =
                (LocationBarLayout) mActivityTestRule.getActivity().findViewById(R.id.location_bar);
        typeInOmnibox(mActivityTestRule.getActivity(), "view chrome history");

        CriteriaHelper.pollUiThread(() -> {
            AutocompleteMatch matchSuggestion = findOmniboxPedalSuggestion(
                    locationBarLayout, OmniboxPedalType.VIEW_CHROME_HISTORY);
            Criteria.checkThat(matchSuggestion, Matchers.notNullValue());
        });
    }

    @Test
    @MediumTest
    @EnableFeatures("OmniboxPedalsAndroidBatch1")
    public void testManageAccessibilitySettingsOmniboxPedalSuggestion()
            throws InterruptedException {
        LocationBarLayout locationBarLayout =
                (LocationBarLayout) mActivityTestRule.getActivity().findViewById(R.id.location_bar);
        typeInOmnibox(mActivityTestRule.getActivity(), "Chrome accessibility");

        CriteriaHelper.pollUiThread(() -> {
            AutocompleteMatch matchSuggestion = findOmniboxPedalSuggestion(
                    locationBarLayout, OmniboxPedalType.MANAGE_CHROME_ACCESSIBILITY);
            Criteria.checkThat(matchSuggestion, Matchers.notNullValue());
        });
    }

    @Test
    @MediumTest
    @EnableFeatures("OmniboxPedalsAndroidBatch1")
    public void testPlayChromeDinoGameOmniboxPedalSuggestion() throws InterruptedException {
        LocationBarLayout locationBarLayout =
                (LocationBarLayout) mActivityTestRule.getActivity().findViewById(R.id.location_bar);
        typeInOmnibox(mActivityTestRule.getActivity(), "Dino game");

        CriteriaHelper.pollUiThread(() -> {
            AutocompleteMatch matchSuggestion = findOmniboxPedalSuggestion(
                    locationBarLayout, OmniboxPedalType.PLAY_CHROME_DINO_GAME);
            Criteria.checkThat(matchSuggestion, Matchers.notNullValue());
        });
    }
}

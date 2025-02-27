// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.site_settings;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDescendantOfA;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.CoreMatchers.allOf;
import static org.junit.Assert.assertEquals;

import android.os.Bundle;
import android.view.View;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.UserActionTester;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.settings.SettingsActivity;
import org.chromium.chrome.browser.settings.SettingsActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.components.browser_ui.site_settings.SingleCategorySettings;
import org.chromium.components.browser_ui.site_settings.SiteSettingsCategory;
import org.chromium.ui.test.util.RenderTestRule;
import org.chromium.ui.test.util.RenderTestRule.Component;

import java.io.IOException;

/** Render tests for the Cookie page and subpages under Settings > Site Settings > Cookies. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@EnableFeatures({ChromeFeatureList.ALWAYS_BLOCK_3PCS_INCOGNITO})
public class CookieSettingsTest {
    @Rule
    public SettingsActivityTestRule<SingleCategorySettings> mSettingsActivityTestRule =
            new SettingsActivityTestRule<>(SingleCategorySettings.class);

    @Rule
    public ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setBugComponent(Component.UI_BROWSER_MOBILE_SETTINGS)
                    .build();

    private SettingsActivity mSettingsActivity;
    private UserActionTester mUserActionTester;

    @Before
    public void setUp() {
        Bundle fragmentArgs = new Bundle();
        fragmentArgs.putString(
                SingleCategorySettings.EXTRA_CATEGORY,
                SiteSettingsCategory.preferenceKey(SiteSettingsCategory.Type.THIRD_PARTY_COOKIES));
        mSettingsActivity = mSettingsActivityTestRule.startSettingsActivity(fragmentArgs);
        mUserActionTester = new UserActionTester();
    }

    @After
    public void tearDown() {
        mUserActionTester.tearDown();
    }

    @Test
    @SmallTest
    public void shouldRecordUserActionWhenCookiePreferenceChanges() throws IOException {
        onView(withId(R.id.block_third_party_with_aux)).perform(click());
        assertEquals(1, mUserActionTester.getActionCount("Settings.ThirdPartyCookies.Block"));
        onView(withId(R.id.block_third_party_incognito_with_aux)).perform(click());
        assertEquals(1, mUserActionTester.getActionCount("Settings.ThirdPartyCookies.Allow"));
    }

    // TODO(crbug.com/370008370): Remove once AlwaysBlock3pcsIncognito launched.
    @Test
    @SmallTest
    @DisableFeatures({ChromeFeatureList.ALWAYS_BLOCK_3PCS_INCOGNITO})
    public void shouldDisplayBlockThirdPartyDescriptionAndRwsToggleWhenAuxButtonClicked()
            throws IOException {
        onView(withId(R.id.block_third_party_with_aux)).perform(click());
        onView(
                        allOf(
                                withId(R.id.expand_arrow),
                                isDescendantOfA(withId(R.id.block_third_party_with_aux))))
                .perform(click());
        onView(withText(R.string.website_settings_category_cookie_block_third_party_subtitle))
                .check(matches(isDisplayed()));
        onView(withText(R.string.website_settings_category_cookie_subpage_bullet_one))
                .check(matches(isDisplayed()));
        onView(withText(R.string.website_settings_category_cookie_subpage_bullet_two))
                .check(matches(isDisplayed()));
        onView(withText(R.string.website_settings_category_cookie_rws_toggle_title))
                .check(matches(isDisplayed()));
        onView(withText(R.string.website_settings_category_cookie_rws_toggle_description))
                .check(matches(isDisplayed()));
    }

    // TODO(crbug.com/370008370): Remove once AlwaysBlock3pcsIncognito launched.
    @Test
    @SmallTest
    @DisableFeatures({ChromeFeatureList.ALWAYS_BLOCK_3PCS_INCOGNITO})
    public void shouldDisplayBlockThirdPartyIncognitoDescriptionWhenAuxButtonClicked()
            throws IOException {
        onView(withId(R.id.block_third_party_incognito_with_aux)).perform(click());
        onView(
                        allOf(
                                withId(R.id.expand_arrow),
                                isDescendantOfA(withId(R.id.block_third_party_incognito_with_aux))))
                .perform(click());
        onView(
                        withText(
                                R.string
                                        .website_settings_category_cookie_block_third_party_incognito_subtitle))
                .check(matches(isDisplayed()));
        onView(withText(R.string.website_settings_category_cookie_subpage_incognito_bullet_two))
                .check(matches(isDisplayed()));
    }

    @Test
    @SmallTest
    public void shouldDisplayAllowDescriptionWhenAuxButtonClicked() throws IOException {
        onView(withId(R.id.block_third_party_incognito_with_aux)).perform(click());
        onView(
                        allOf(
                                withId(R.id.expand_arrow),
                                isDescendantOfA(withId(R.id.block_third_party_incognito_with_aux))))
                .perform(click());
        onView(withText(R.string.settings_cookies_block_third_party_settings_allow_bullet_one))
                .check(matches(isDisplayed()));
        onView(withText(R.string.settings_cookies_block_third_party_settings_allow_bullet_two))
                .check(matches(isDisplayed()));
        onView(withText(R.string.settings_cookies_block_third_party_settings_allow_bullet_three))
                .check(matches(isDisplayed()));
    }

    @Test
    @SmallTest
    public void shouldDisplayBlockDescriptionAndRwsToggleWhenAuxButtonClicked() throws IOException {
        onView(withId(R.id.block_third_party_with_aux)).perform(click());
        onView(
                        allOf(
                                withId(R.id.expand_arrow),
                                isDescendantOfA(withId(R.id.block_third_party_with_aux))))
                .perform(click());
        onView(withText(R.string.website_settings_category_cookie_block_third_party_subtitle))
                .check(matches(isDisplayed()));
        onView(withText(R.string.settings_cookies_block_third_party_settings_block_bullet_one))
                .check(matches(isDisplayed()));
        onView(withText(R.string.settings_cookies_block_third_party_settings_block_bullet_two))
                .check(matches(isDisplayed()));
        onView(withText(R.string.settings_cookies_block_third_party_settings_block_bullet_three))
                .check(matches(isDisplayed()));
        onView(withText(R.string.website_settings_category_cookie_rws_toggle_title))
                .check(matches(isDisplayed()));
        onView(withText(R.string.website_settings_category_cookie_rws_toggle_description))
                .check(matches(isDisplayed()));
    }

    // TODO(crbug.com/370008370): Remove once AlwaysBlock3pcsIncognito launched.
    @Test
    @SmallTest
    @Feature({"RenderTest"})
    @DisableFeatures({ChromeFeatureList.ALWAYS_BLOCK_3PCS_INCOGNITO})
    public void renderBlockThirdPartyDescriptionAndRwsToggleWhenAuxButtonClicked()
            throws IOException {
        onView(withId(R.id.block_third_party_with_aux)).perform(click());
        onView(
                        allOf(
                                withId(R.id.expand_arrow),
                                isDescendantOfA(withId(R.id.block_third_party_with_aux))))
                .perform(click());

        mRenderTestRule.render(
                getRootView(R.string.website_settings_category_cookie_block_third_party_subtitle),
                "settings_cookie_rws_subpage_block_third_party");
    }

    // TODO(crbug.com/370008370): Remove once AlwaysBlock3pcsIncognito launched.
    @Test
    @SmallTest
    @Feature({"RenderTest"})
    @DisableFeatures({ChromeFeatureList.ALWAYS_BLOCK_3PCS_INCOGNITO})
    public void renderBlockThirdPartyIncognitoDescriptionWhenAuxButtonClicked() throws IOException {
        onView(withId(R.id.block_third_party_incognito_with_aux)).perform(click());
        onView(
                        allOf(
                                withId(R.id.expand_arrow),
                                isDescendantOfA(withId(R.id.block_third_party_incognito_with_aux))))
                .perform(click());

        mRenderTestRule.render(
                getRootView(
                        R.string
                                .website_settings_category_cookie_block_third_party_incognito_subtitle),
                "settings_cookie_rws_subpage_block_third_party_incognito");
    }

    @Test
    @SmallTest
    @Feature({"RenderTest"})
    public void renderAllowDescriptionWhenAuxButtonClicked() throws IOException {
        onView(withId(R.id.block_third_party_incognito_with_aux)).perform(click());
        onView(
                        allOf(
                                withId(R.id.expand_arrow),
                                isDescendantOfA(withId(R.id.block_third_party_incognito_with_aux))))
                .perform(click());

        mRenderTestRule.render(
                getRootView(R.string.website_settings_category_cookie_allow_third_party_subtitle),
                "settings_cookie_rws_subpage_allow");
    }

    @Test
    @SmallTest
    @Feature({"RenderTest"})
    public void renderBlockDescriptionAndRwsToggleWhenAuxButtonClicked() throws IOException {
        onView(withId(R.id.block_third_party_with_aux)).perform(click());
        onView(
                        allOf(
                                withId(R.id.expand_arrow),
                                isDescendantOfA(withId(R.id.block_third_party_with_aux))))
                .perform(click());

        mRenderTestRule.render(
                getRootView(R.string.website_settings_category_cookie_block_third_party_subtitle),
                "settings_cookie_rws_subpage_block");
    }

    private View getRootView(int text) {
        View[] view = {null};
        onView(withText(text)).check((v, e) -> view[0] = v.getRootView());
        ThreadUtils.runOnUiThreadBlocking(() -> RenderTestRule.sanitize(view[0]));
        return view[0];
    }
}

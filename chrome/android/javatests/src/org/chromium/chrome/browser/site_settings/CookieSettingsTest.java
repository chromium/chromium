// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.site_settings;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.matcher.ViewMatchers.isDescendantOfA;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.CoreMatchers.allOf;
import static org.hamcrest.CoreMatchers.is;

import static org.chromium.ui.test.util.ViewUtils.onViewWaiting;

import android.os.Bundle;
import android.view.View;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.settings.SettingsActivity;
import org.chromium.chrome.browser.settings.SettingsActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.components.browser_ui.settings.SettingsFeatureList;
import org.chromium.components.browser_ui.site_settings.FourStateCookieSettingsPreference;
import org.chromium.components.browser_ui.site_settings.FourStateCookieSettingsPreference.CookieSettingsState;
import org.chromium.components.browser_ui.site_settings.R;
import org.chromium.components.browser_ui.site_settings.SingleCategorySettings;
import org.chromium.components.browser_ui.site_settings.SiteSettingsCategory;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.test.util.RenderTestRule;
import org.chromium.ui.test.util.RenderTestRule.Component;

import java.io.IOException;

/** Render tests for the Cookie page and subpages under Settings > Site Settings > Cookies. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
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

    @Before
    public void setUp() {
        Bundle fragmentArgs = new Bundle();
        fragmentArgs.putString(SingleCategorySettings.EXTRA_CATEGORY,
                SiteSettingsCategory.preferenceKey(SiteSettingsCategory.Type.COOKIES));
        mSettingsActivity = mSettingsActivityTestRule.startSettingsActivity(fragmentArgs);
    }

    @Test
    @SmallTest
    @Feature({"RenderTest"})
    @DisableFeatures(ChromeFeatureList.PRIVACY_SANDBOX_FPS_UI)
    public void testRenderCookiePage() throws IOException {
        // This test is written for when First-Party Sets UI is disabled. When
        // First-Party Sets UI is eventually enabled by default, this test will
        // be rewritten or deleted.
        setCookiesEnabled(mSettingsActivity, false);
        View view = mSettingsActivity.getMainFragment().getView();
        onViewWaiting(allOf(is(view), isDisplayed()));

        mRenderTestRule.render(view, "settings_cookie_page");
    }

    @Test
    @SmallTest
    @Feature({"RenderTest"})
    @EnableFeatures({ChromeFeatureList.PRIVACY_SANDBOX_FPS_UI,
            SettingsFeatureList.HIGHLIGHT_MANAGED_PREF_DISCLAIMER_ANDROID})
    public void
    testRenderCookieFPSSubpage_EnableHighlightManagedPrefDisclaimerAndroid() throws IOException {
        showCookieFPSSubpage();
        mRenderTestRule.render(
                getRootView(R.string.website_settings_category_cookie_block_third_party_subtitle),
                "settings_cookie_fps_subpage");
    }

    @Test
    @SmallTest
    @Feature({"RenderTest"})
    @EnableFeatures(ChromeFeatureList.PRIVACY_SANDBOX_FPS_UI)
    @DisableFeatures(SettingsFeatureList.HIGHLIGHT_MANAGED_PREF_DISCLAIMER_ANDROID)
    public void testRenderCookieFPSSubpage_DisableHighlightManagedPrefDisclaimerAndroid()
            throws IOException {
        showCookieFPSSubpage();
        mRenderTestRule.render(
                getRootView(R.string.website_settings_category_cookie_block_third_party_subtitle),
                "settings_cookie_fps_subpage_DisableHighlightManagedPrefDisclaimerAndroid");
    }

    private void showCookieFPSSubpage() {
        onView(withId(R.id.block_third_party_with_aux)).perform(click());
        onView(allOf(withId(R.id.expand_arrow),
                       isDescendantOfA(withId(R.id.block_third_party_with_aux))))
                .perform(click());
    }

    private void setCookiesEnabled(final SettingsActivity settingsActivity, final boolean enabled) {
        TestThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                final SingleCategorySettings websitePreferences =
                        (SingleCategorySettings) settingsActivity.getMainFragment();
                final FourStateCookieSettingsPreference cookies =
                        (FourStateCookieSettingsPreference) websitePreferences.findPreference(
                                SingleCategorySettings.FOUR_STATE_COOKIE_TOGGLE_KEY);

                websitePreferences.onPreferenceChange(
                        cookies, enabled ? CookieSettingsState.ALLOW : CookieSettingsState.BLOCK);
            }
        });
    }

    private View getRootView(int text) {
        View[] view = {null};
        onView(withText(text)).check(((v, e) -> view[0] = v.getRootView()));
        TestThreadUtils.runOnUiThreadBlocking(() -> RenderTestRule.sanitize(view[0]));
        return view[0];
    }
}

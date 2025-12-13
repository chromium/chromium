// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.scrollTo;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withEffectiveVisibility;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import android.content.Context;
import android.content.res.Configuration;
import android.content.res.Resources;

import androidx.test.espresso.matcher.ViewMatchers;
import androidx.test.filters.SmallTest;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.LocaleUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.ProductConfig;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.transit.AutoResetCtaTransitTestRule;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.ui.text.SpanApplier;
import org.chromium.ui.text.SpanApplier.SpanInfo;

import java.util.Locale;

/** Integration tests for IncognitoNewTabPage. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class IncognitoNewTabPageTest {
    @Rule
    public AutoResetCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.fastAutoResetCtaActivityRule();

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Test
    @SmallTest
    public void incognitoNtpShowsThirdPartyCookieBlockingHeader() throws Exception {
        mActivityTestRule.startOnBlankPage().openNewIncognitoTabOrWindowFast();
        onView(withId(R.id.tracking_protection_card))
                .perform(scrollTo())
                .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)));

        String ntpDescription =
                getIncognitoNtpDescriptionSpanned(
                        mActivityTestRule
                                .getActivity()
                                .getResources()
                                .getString(
                                        R.string
                                                .incognito_ntp_block_third_party_cookies_description_android));

        onView(withText(R.string.incognito_ntp_block_third_party_cookies_header))
                .check(matches(isDisplayed()));
        onView(withText(ntpDescription)).check(matches(isDisplayed()));
    }

    private Context createContextForLocale(Context context, String languageTag) {
        Locale locale = LocaleUtils.forLanguageTag(languageTag);
        Resources res = context.getResources();
        Configuration config = res.getConfiguration();
        config.setLocale(locale);
        return context.createConfigurationContext(config);
    }

    /** Test the ntp text formatting for all locales. */
    @Test
    @SmallTest
    public void testDescriptionLanguages() throws Exception {
        var context = mActivityTestRule.getActivity().getApplicationContext();
        for (String languageTag : ProductConfig.LOCALES) {
            var localeContext = createContextForLocale(context, languageTag);
            IncognitoDescriptionView.getSpannedBulletText(
                    localeContext, R.string.new_tab_otr_not_saved);
            IncognitoDescriptionView.getSpannedBulletText(
                    localeContext, R.string.new_tab_otr_visible);
        }
    }

    private String getIncognitoNtpDescriptionSpanned(String description) {
        return SpanApplier.applySpans(description, new SpanInfo("<link>", "</link>", new Object()))
                .toString();
    }
}

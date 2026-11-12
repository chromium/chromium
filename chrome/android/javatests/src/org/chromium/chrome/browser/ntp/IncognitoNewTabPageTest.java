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

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.timeout;
import static org.mockito.Mockito.verify;

import android.content.Context;
import android.content.res.Configuration;
import android.content.res.Resources;

import androidx.test.espresso.matcher.ViewMatchers;
import androidx.test.filters.SmallTest;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.LocaleUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.build.BuildConfig;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.preloading.AndroidPrerenderManager;
import org.chromium.chrome.browser.preloading.AndroidPrerenderManagerJni;
import org.chromium.chrome.browser.preloading.PreloadingFeatureMap;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabHidingType;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.AutoResetCtaTransitTestRule;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.ntp.IncognitoNewTabPageStation;
import org.chromium.ui.text.SpanApplier;
import org.chromium.ui.text.SpanApplier.SpanInfo;

import java.util.Locale;

/** Integration tests for IncognitoNewTabPage. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
public class IncognitoNewTabPageTest {
    @Rule
    public AutoResetCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.fastAutoResetCtaActivityRule();

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private AndroidPrerenderManager.Natives mPrerenderManagerJniMock;

    @Test
    @SmallTest
    public void incognitoNtpShowsThirdPartyCookieBlockingHeader() throws Exception {
        mActivityTestRule.startOnBlankPage().openNewIncognitoTabOrWindowFast();
        onView(withId(R.id.tracking_protection_card))
                .perform(scrollTo())
                .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)));

        var res = mActivityTestRule.getActivity().getResources();
        int stringId =
                R.string
                .incognito_ntp_block_third_party_cookies_description_android;
        String ntpDescription =
                getIncognitoNtpDescriptionSpanned(res.getString(stringId));

        onView(withText(R.string.incognito_ntp_block_third_party_cookies_header))
                .check(matches(isDisplayed()));
        onView(withText(ntpDescription)).check(matches(isDisplayed()));
    }

    @Test
    @SmallTest
    @EnableFeatures({
        PreloadingFeatureMap.PREWARM,
        PreloadingFeatureMap.PREWARM_PAGE_LOAD_AND_TAB_SWITCH_AFTER_PAINT_TRIGGER
    })
    @DisableFeatures(PreloadingFeatureMap.PREWARM_ZERO_SUGGEST_TRIGGER)
    public void testPrewarmTriggeredOnFirstDrawAndTabSwitch() {
        var blankPage = mActivityTestRule.startOnBlankPage();
        AndroidPrerenderManagerJni.setInstanceForTesting(mPrerenderManagerJniMock);

        IncognitoNewTabPageStation incognitoNtp = blankPage.openNewIncognitoTabOrWindowFast();
        Tab tab = incognitoNtp.getTab();
        verify(mPrerenderManagerJniMock, timeout(CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL))
                .startPrewarm(tab.getWebContents());

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    clearInvocations(mPrerenderManagerJniMock);
                    tab.hide(TabHidingType.CHANGED_TABS);
                    verify(mPrerenderManagerJniMock, never()).startPrewarm(any());
                    tab.show(TabSelectionType.FROM_USER);
                    verify(mPrerenderManagerJniMock).startPrewarm(tab.getWebContents());
                });
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
        for (String languageTag : BuildConfig.LOCALES) {
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

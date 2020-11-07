// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.voice;

import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withEffectiveVisibility;
import static androidx.test.espresso.matcher.ViewMatchers.withId;

import static org.hamcrest.Matchers.allOf;

import static org.chromium.chrome.test.util.ViewUtils.waitForView;

import androidx.test.espresso.matcher.ViewMatchers;
import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.content_public.browser.test.util.ClickUtils;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.io.IOException;

/** Tests for AssistantVoiceSearchConsentDialog */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
// TODO(wylieb): Batch these tests if possible.
public class AssistantVoiceSearchConsentUiTest {
    @Rule
    public ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus().build();
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Before
    public void setUp() {
        mActivityTestRule.startMainActivityOnBlankPage();
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testShow() throws IOException {
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();

        mActivityTestRule.loadUrl(UrlConstants.NTP_URL);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            AssistantVoiceSearchConsentUi.show(cta.getWindowAndroid(), () -> {});
            waitForView(allOf(withId(R.id.avs_consent_ui), isDisplayed()));
        });
        mRenderTestRule.render(cta.findViewById(R.id.avs_consent_ui), "avs_consent_ui_ntp");
    }

    @Test
    @MediumTest
    public void testHide_DialogButtons() {
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            AssistantVoiceSearchConsentUi.show(cta.getWindowAndroid(), () -> {});
            waitForView(allOf(withId(R.id.avs_consent_ui), isDisplayed()));
            ClickUtils.clickButton(cta.findViewById(R.id.button_primary));
            waitForView(allOf(withId(R.id.avs_consent_ui),
                    withEffectiveVisibility(ViewMatchers.Visibility.GONE)));

            AssistantVoiceSearchConsentUi.show(cta.getWindowAndroid(), () -> {});
            ClickUtils.clickButton(cta.findViewById(R.id.button_secondary));
            waitForView(allOf(withId(R.id.avs_consent_ui),
                    withEffectiveVisibility(ViewMatchers.Visibility.GONE)));
        });
    }
}
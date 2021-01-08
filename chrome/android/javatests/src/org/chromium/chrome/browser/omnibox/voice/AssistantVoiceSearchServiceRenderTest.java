// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.voice;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.matcher.ViewMatchers.withId;

import static org.mockito.ArgumentMatchers.anyObject;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.Mockito.doReturn;

import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.ASSISTANT_VOICE_SEARCH_ENABLED;
import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.ASSISTANT_VOICE_SEARCH_SUPPORTED;

import android.support.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.gsa.GSAState;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.externalauth.ExternalAuthUtils;

import java.io.IOException;

/** Tests for AssistantVoiceSearchService */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
        "enable-features=" + ChromeFeatureList.OMNIBOX_ASSISTANT_VOICE_SEARCH + "<Study",
        "force-fieldtrials=Study/Group"})
public class AssistantVoiceSearchServiceRenderTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();
    @Rule
    public ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus().build();
    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock
    GSAState mGsaState;

    @Before
    public void setUp() {
        AssistantVoiceSearchService.setAgsaSupportsAssistantVoiceSearchForTesting(true);
        SharedPreferencesManager.getInstance().writeBoolean(ASSISTANT_VOICE_SEARCH_ENABLED, true);
        SharedPreferencesManager.getInstance().writeBoolean(ASSISTANT_VOICE_SEARCH_SUPPORTED, true);

        GSAState gsaState = Mockito.mock(GSAState.class);
        doReturn(false).when(gsaState).isAgsaVersionBelowMinimum(anyString(), anyString());
        doReturn(true).when(gsaState).doesGsaAccountMatchChrome();
        doReturn(true).when(gsaState).canAgsaHandleIntent(anyObject());
        GSAState.setInstanceForTesting(gsaState);

        ExternalAuthUtils externalAuthUtils = Mockito.mock(ExternalAuthUtils.class);
        doReturn(true).when(externalAuthUtils).isGoogleSigned(anyString());
        doReturn(true).when(externalAuthUtils).isChromeGoogleSigned();
        ExternalAuthUtils.setInstanceForTesting(externalAuthUtils);

        mActivityTestRule.startMainActivityOnBlankPage();
    }

    @Test
    @MediumTest
    @CommandLineFlags.Add({"force-fieldtrial-params=Study.Group:colorful_mic/true"})
    @Feature({"RenderTest"})
    public void testAssistantColorfulMic() throws IOException {
        mActivityTestRule.loadUrlInNewTab(UrlConstants.NTP_URL, /* incognito= */ false);

        mRenderTestRule.render(mActivityTestRule.getActivity().findViewById(R.id.ntp_content),
                "avs_colorful_mic_unfocused_ntp");

        onView(withId(R.id.search_box)).perform(click());
        mRenderTestRule.render(mActivityTestRule.getActivity().findViewById(R.id.toolbar),
                "avs_colorful_mic_focused");
    }

    @Test
    @MediumTest
    @CommandLineFlags.Add({"force-fieldtrial-params=Study.Group:colorful_mic/false"})
    @Feature({"RenderTest"})
    public void testAssistantMic() throws IOException {
        mActivityTestRule.loadUrlInNewTab(UrlConstants.NTP_URL, /* incognito= */ false);

        mRenderTestRule.render(mActivityTestRule.getActivity().findViewById(R.id.ntp_content),
                "avs__mic_unfocused_ntp");

        onView(withId(R.id.search_box)).perform(click());
        mRenderTestRule.render(
                mActivityTestRule.getActivity().findViewById(R.id.toolbar), "avs_mic_focused");
    }
}
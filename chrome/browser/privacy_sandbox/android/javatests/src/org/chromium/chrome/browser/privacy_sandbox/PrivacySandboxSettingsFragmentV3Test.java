// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_sandbox;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.matcher.ViewMatchers.hasSibling;
import static androidx.test.espresso.matcher.ViewMatchers.withChild;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withParent;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.allOf;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.os.Bundle;
import android.view.View;

import androidx.annotation.StringRes;
import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.settings.SettingsActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.content_public.browser.test.NativeLibraryTestUtils;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.test.util.RenderTestRule;

import java.io.IOException;

/** Tests {@link PrivacySandboxSettingsFragment}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@Features.EnableFeatures(ChromeFeatureList.PRIVACY_SANDBOX_SETTINGS_3)
public final class PrivacySandboxSettingsFragmentV3Test {
    @Rule
    public SettingsActivityTestRule<PrivacySandboxSettingsFragmentV3> mSettingsActivityTestRule =
            new SettingsActivityTestRule<>(PrivacySandboxSettingsFragmentV3.class);

    @Rule
    public ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus().build();

    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public JniMocker mocker = new JniMocker();

    @Mock
    private PrivacySandboxBridge.Natives mSandboxBridgeMock;

    @Before
    public void setUp() {
        NativeLibraryTestUtils.loadNativeLibraryNoBrowserProcess();
    }

    private void openPrivacySandboxSettings() {
        Bundle fragmentArgs = new Bundle();
        fragmentArgs.putInt(PrivacySandboxSettingsFragment.PRIVACY_SANDBOX_REFERRER,
                PrivacySandboxReferrer.PRIVACY_SETTINGS);
        mSettingsActivityTestRule.startSettingsActivity(fragmentArgs);
    }

    private View getView(@StringRes int text) {
        View[] view = {null};
        onView(withText(text)).check(((v, e) -> view[0] = v.getRootView()));
        TestThreadUtils.runOnUiThreadBlocking(() -> RenderTestRule.sanitize(view[0]));
        return view[0];
    }

    @Test
    @SmallTest
    @Feature({"RenderTest"})
    public void testRenderMainPage() throws IOException {
        openPrivacySandboxSettings();
        mRenderTestRule.render(
                getView(R.string.privacy_sandbox_trials_title), "privacy_sandbox_main_view");
    }

    @Test
    @SmallTest
    @Feature({"RenderTest"})
    public void testRenderAdPersonalizationView() throws IOException {
        openPrivacySandboxSettings();
        onView(withText(R.string.privacy_sandbox_ad_personalization_title)).perform(click());
        mRenderTestRule.render(getView(R.string.privacy_sandbox_topic_interests_category),
                "privacy_sandbox_ad_personalization_view");
    }

    @Test
    @SmallTest
    @Features.EnableFeatures(ChromeFeatureList.PRIVACY_SANDBOX_SETTINGS_3)
    public void testMainSettingsView() throws IOException {
        mocker.mock(PrivacySandboxBridgeJni.TEST_HOOKS, mSandboxBridgeMock);
        when(mSandboxBridgeMock.isPrivacySandboxEnabled()).thenReturn(true);
        openPrivacySandboxSettings();
        // Toggle sandbox settings.
        onView(withText(R.string.privacy_sandbox_trials_title)).perform(click());
        verify(mSandboxBridgeMock).setPrivacySandboxEnabled(false);
        onView(withText(R.string.privacy_sandbox_trials_title)).perform(click());
        verify(mSandboxBridgeMock).setPrivacySandboxEnabled(true);
    }
    @Test
    @SmallTest
    @Features.EnableFeatures(ChromeFeatureList.PRIVACY_SANDBOX_SETTINGS_3)
    public void testAdPersonalizationView() throws IOException {
        mocker.mock(PrivacySandboxBridgeJni.TEST_HOOKS, mSandboxBridgeMock);
        when(mSandboxBridgeMock.getCurrentTopTopics()).thenReturn(new String[] {"Foo"});
        openPrivacySandboxSettings();
        onView(withText(R.string.privacy_sandbox_ad_personalization_title)).perform(click());
        // Click on the image_button of a the preference with "Foo" text.
        onView(allOf(withId(R.id.image_button), withParent(hasSibling(withChild(withText("Foo"))))))
                .perform(click());
        verify(mSandboxBridgeMock).setTopicAllowed("Foo", false);
    }
}

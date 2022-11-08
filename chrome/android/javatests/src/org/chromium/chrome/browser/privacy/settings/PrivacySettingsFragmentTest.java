// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy.settings;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.doesNotExist;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.hasDescendant;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.chromium.base.test.util.Batch.PER_CLASS;

import android.os.Build;
import android.text.TextUtils;
import android.view.View;
import android.widget.TextView;

import androidx.recyclerview.widget.RecyclerView;
import androidx.test.espresso.contrib.RecyclerViewActions;
import androidx.test.filters.LargeTest;

import org.hamcrest.Matcher;
import org.junit.Before;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.incognito.reauth.IncognitoReauthManager;
import org.chromium.chrome.browser.incognito.reauth.IncognitoReauthSettingUtils;
import org.chromium.chrome.browser.privacy_sandbox.FakePrivacySandboxBridge;
import org.chromium.chrome.browser.privacy_sandbox.PrivacySandboxBridgeJni;
import org.chromium.chrome.browser.settings.SettingsActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.io.IOException;

/**
 * Tests for {@link PrivacySettings}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(PER_CLASS)
public class PrivacySettingsFragmentTest {
    @ClassRule
    public static final ChromeTabbedActivityTestRule sActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Rule
    public final SettingsActivityTestRule<PrivacySettings> mSettingsActivityTestRule =
            new SettingsActivityTestRule<>(PrivacySettings.class);

    @Rule
    public ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setBugComponent(ChromeRenderTestRule.Component.UI_SETTINGS_PRIVACY)
                    .build();

    @Rule
    public JniMocker mocker = new JniMocker();

    private FakePrivacySandboxBridge mFakePrivacySandboxBridge;

    private void waitForOptionsMenu() {
        CriteriaHelper.pollUiThread(() -> {
            return mSettingsActivityTestRule.getActivity().findViewById(R.id.menu_id_targeted_help)
                    != null;
        });
    }

    private void scrollToSetting(Matcher<View> matcher) {
        onView(withId(R.id.recycler_view))
                .perform(RecyclerViewActions.scrollTo(hasDescendant(matcher)));
    }

    private View getIncognitoReauthSettingView(PrivacySettings privacySettings) {
        String incognito_lock_title = mSettingsActivityTestRule.getActivity().getString(
                R.string.settings_incognito_tab_lock_title);
        onView(withId(R.id.recycler_view))
                .perform(RecyclerViewActions.scrollTo(
                        hasDescendant(withText(incognito_lock_title))));
        onView(withText(incognito_lock_title)).check(matches(isDisplayed()));
        for (int i = 0; i < privacySettings.getListView().getChildCount(); ++i) {
            View view = privacySettings.getListView().getChildAt(i);
            String title = ((TextView) view.findViewById(android.R.id.title)).getText().toString();
            if (!TextUtils.isEmpty(title) && TextUtils.equals(incognito_lock_title, title)) {
                return view;
            }
        }
        return null;
    }

    @Before
    public void setUp() {
        mFakePrivacySandboxBridge = new FakePrivacySandboxBridge();
        mocker.mock(PrivacySandboxBridgeJni.TEST_HOOKS, mFakePrivacySandboxBridge);
    }

    @Test
    @LargeTest
    @Feature({"RenderTest"})
    public void testRenderTopView() throws IOException {
        mSettingsActivityTestRule.startSettingsActivity();
        waitForOptionsMenu();
        View view = mSettingsActivityTestRule.getActivity()
                            .findViewById(android.R.id.content)
                            .getRootView();
        mRenderTestRule.render(view, "privacy_and_security_settings_top_view");
    }

    @Test
    @LargeTest
    @Feature({"RenderTest"})
    public void testRenderBottomView() throws IOException {
        mSettingsActivityTestRule.startSettingsActivity();
        waitForOptionsMenu();
        PrivacySettings fragment = mSettingsActivityTestRule.getFragment();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            RecyclerView recyclerView = fragment.getView().findViewById(R.id.recycler_view);
            recyclerView.scrollToPosition(recyclerView.getAdapter().getItemCount() - 1);
        });
        View view = mSettingsActivityTestRule.getActivity()
                            .findViewById(android.R.id.content)
                            .getRootView();
        mRenderTestRule.render(view, "privacy_and_security_settings_bottom_view");
    }

    @Test
    @LargeTest
    @Features.DisableFeatures(ChromeFeatureList.PRIVACY_SANDBOX_SETTINGS_3)
    public void testPrivacySandboxView() throws IOException {
        mSettingsActivityTestRule.startSettingsActivity();
        PrivacySettings fragment = mSettingsActivityTestRule.getFragment();
        // Scroll down and open Privacy Sandbox page.
        scrollToSetting(withText(R.string.prefs_privacy_sandbox));
        onView(withText(R.string.prefs_privacy_sandbox)).perform(click());
        // Verify that the right view is shown depending on feature state.
        scrollToSetting(withText(R.string.privacy_sandbox_toggle));
        onView(withText(R.string.privacy_sandbox_toggle)).check(matches(isDisplayed()));
    }

    @Test
    @LargeTest
    @Features.EnableFeatures(ChromeFeatureList.PRIVACY_SANDBOX_SETTINGS_3)
    public void testPrivacySandboxViewV3() throws IOException {
        mSettingsActivityTestRule.startSettingsActivity();
        PrivacySettings fragment = mSettingsActivityTestRule.getFragment();
        // Scroll down and open Privacy Sandbox page.
        scrollToSetting(withText(R.string.prefs_privacy_sandbox));
        onView(withText(R.string.prefs_privacy_sandbox)).perform(click());
        // Verify that the right view is shown depending on feature state.
        onView(withText(R.string.privacy_sandbox_ad_personalization_title))
                .check(matches(isDisplayed()));
    }

    @Test
    @LargeTest
    @Features.EnableFeatures(ChromeFeatureList.PRIVACY_SANDBOX_SETTINGS_3)
    public void testPrivacySandboxViewV3Restricted() throws IOException {
        mFakePrivacySandboxBridge.setPrivacySandboxRestricted(true);
        mSettingsActivityTestRule.startSettingsActivity();
        PrivacySettings fragment = mSettingsActivityTestRule.getFragment();
        // Scroll down and verify that the Privacy Sandbox is not there.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            RecyclerView recyclerView = fragment.getView().findViewById(R.id.recycler_view);
            recyclerView.scrollToPosition(recyclerView.getAdapter().getItemCount() - 1);
        });
        onView(withText(R.string.prefs_privacy_sandbox)).check(doesNotExist());
    }

    @Test
    @LargeTest
    @Feature({"RenderTest"})
    @DisableIf.Build(sdk_is_less_than = Build.VERSION_CODES.M,
            message = "Invokes IncognitoReauthSettingUtils#isDeviceScreenLockEnabled internally"
                    + "which is available only from M.")
    public void
    testRenderIncognitoLockView_DeviceScreenLockDisabled() throws IOException {
        IncognitoReauthManager.setIsIncognitoReauthFeatureAvailableForTesting(true);
        IncognitoReauthSettingUtils.setIsDeviceScreenLockEnabledForTesting(false);

        mSettingsActivityTestRule.startSettingsActivity();
        waitForOptionsMenu();
        PrivacySettings fragment = mSettingsActivityTestRule.getFragment();

        mRenderTestRule.render(getIncognitoReauthSettingView(fragment),
                "incognito_reauth_setting_screen_lock_disabled");
    }

    @Test
    @LargeTest
    @Feature({"RenderTest"})
    @DisableIf.Build(sdk_is_less_than = Build.VERSION_CODES.M,
            message = "Invokes IncognitoReauthSettingUtils#isDeviceScreenLockEnabled internally"
                    + "which is available only from M.")
    public void
    testRenderIncognitoLockView_DeviceScreenLockEnabled() throws IOException {
        IncognitoReauthManager.setIsIncognitoReauthFeatureAvailableForTesting(true);
        IncognitoReauthSettingUtils.setIsDeviceScreenLockEnabledForTesting(true);

        mSettingsActivityTestRule.startSettingsActivity();
        waitForOptionsMenu();
        PrivacySettings fragment = mSettingsActivityTestRule.getFragment();

        mRenderTestRule.render(getIncognitoReauthSettingView(fragment),
                "incognito_reauth_setting_screen_lock_enabled");
    }
}

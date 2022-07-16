// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy.settings;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import android.os.Build;
import android.text.TextUtils;
import android.view.View;
import android.widget.TextView;

import androidx.recyclerview.widget.RecyclerView;
import androidx.test.espresso.contrib.RecyclerViewActions;
import androidx.test.filters.LargeTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.RuleChain;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.incognito.reauth.IncognitoReauthManager;
import org.chromium.chrome.browser.incognito.reauth.IncognitoReauthSettingUtils;
import org.chromium.chrome.browser.settings.SettingsActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.io.IOException;

/**
 * Tests for {@link PrivacySettings}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class PrivacySettingsFragmentTest {
    public final ChromeTabbedActivityTestRule mActivityTestRule =
            new ChromeTabbedActivityTestRule();
    public final SettingsActivityTestRule<PrivacySettings> mSettingsActivityTestRule =
            new SettingsActivityTestRule<>(PrivacySettings.class);

    // SettingsActivity has to be finished before the outer CTA can be finished or trying to finish
    // CTA won't work.
    @Rule
    public final RuleChain mRuleChain =
            RuleChain.outerRule(mActivityTestRule).around(mSettingsActivityTestRule);

    @Rule
    public ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus().build();

    @Before
    public void setUp() {
        mActivityTestRule.startMainActivityOnBlankPage();
    }

    private void waitForOptionsMenu() {
        CriteriaHelper.pollUiThread(() -> {
            return mSettingsActivityTestRule.getActivity().findViewById(R.id.menu_id_targeted_help)
                    != null;
        });
    }

    private View getIncognitoReauthSettingView(PrivacySettings privacySettings) {
        String incognito_lock_title = mSettingsActivityTestRule.getActivity().getString(
                R.string.settings_incognito_tab_lock_title);
        RecyclerViewActions.scrollTo(withText(incognito_lock_title));
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

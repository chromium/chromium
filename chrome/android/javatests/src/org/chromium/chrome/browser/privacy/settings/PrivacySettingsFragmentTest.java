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

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import static org.chromium.base.test.util.Batch.PER_CLASS;

import android.text.TextUtils;
import android.view.View;
import android.widget.TextView;

import androidx.recyclerview.widget.RecyclerView;
import androidx.test.espresso.contrib.RecyclerViewActions;
import androidx.test.filters.LargeTest;

import org.hamcrest.Matcher;
import org.junit.After;
import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.metrics.HistogramTestRule;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.JniMocker;
import org.chromium.base.test.util.UserActionTester;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.incognito.reauth.IncognitoReauthManager;
import org.chromium.chrome.browser.incognito.reauth.IncognitoReauthSettingUtils;
import org.chromium.chrome.browser.privacy_guide.PrivacyGuideInteractions;
import org.chromium.chrome.browser.privacy_sandbox.FakePrivacySandboxBridge;
import org.chromium.chrome.browser.privacy_sandbox.PrivacySandboxBridgeJni;
import org.chromium.chrome.browser.settings.SettingsActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.content_public.browser.test.NativeLibraryTestUtils;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.io.IOException;

/**
 * Tests for {@link PrivacySettings}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(PER_CLASS)
public class PrivacySettingsFragmentTest {
    // Index of the Privacy Sandbox row entry in the settings list when PRIVACY_SANDBOX_SETTINGS_4
    // is enabled.
    public static final int PRIVACY_SANDBOX_V4_POS_IDX = 4;
    // Name of the histogram to record the entry on Privacy Guide via the S&P link-row.
    public static final String ENTRY_EXIT_HISTOGRAM = "Settings.PrivacyGuide.EntryExit";

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

    @Rule
    public HistogramTestRule mHistogramTestRule = new HistogramTestRule();

    private FakePrivacySandboxBridge mFakePrivacySandboxBridge;
    private UserActionTester mActionTester;

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

    @BeforeClass
    public static void setUpBeforeActivityLaunched() {
        // Only needs to be loaded once and needs to be loaded before HistogramTestRule.
        NativeLibraryTestUtils.loadNativeLibraryNoBrowserProcess();
    }

    @Before
    public void setUp() {
        mFakePrivacySandboxBridge = new FakePrivacySandboxBridge();
        mocker.mock(PrivacySandboxBridgeJni.TEST_HOOKS, mFakePrivacySandboxBridge);
    }

    @After
    public void tearDown() {
        if (mActionTester != null) mActionTester.tearDown();
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
    @Features.DisableFeatures(ChromeFeatureList.PRIVACY_SANDBOX_SETTINGS_4)
    public void testPrivacySandboxV3View() throws IOException {
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
    @Features.EnableFeatures(ChromeFeatureList.PRIVACY_SANDBOX_SETTINGS_4)
    public void testPrivacySandboxV4View() throws IOException {
        mSettingsActivityTestRule.startSettingsActivity();
        PrivacySettings fragment = mSettingsActivityTestRule.getFragment();
        // Scroll down and open Privacy Sandbox page.
        scrollToSetting(withText(R.string.ad_privacy_link_row_label));
        onView(withText(R.string.ad_privacy_link_row_label)).perform(click());
        // Verify that the right view is shown depending on feature state.
        onView(withText(R.string.ad_privacy_page_title)).check(matches(isDisplayed()));
    }

    @Test
    @LargeTest
    @Features.DisableFeatures(ChromeFeatureList.PRIVACY_SANDBOX_SETTINGS_4)
    public void testPrivacySandboxV3ViewRestricted() throws IOException {
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
    @Features.EnableFeatures(ChromeFeatureList.PRIVACY_SANDBOX_SETTINGS_4)
    public void testPrivacySandboxV4ViewRestricted() throws IOException {
        mFakePrivacySandboxBridge.setPrivacySandboxRestricted(true);
        mSettingsActivityTestRule.startSettingsActivity();
        PrivacySettings fragment = mSettingsActivityTestRule.getFragment();
        // Scroll down and verify that the Privacy Sandbox is not there.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            RecyclerView recyclerView = fragment.getView().findViewById(R.id.recycler_view);
            recyclerView.scrollToPosition(PRIVACY_SANDBOX_V4_POS_IDX);
        });
        onView(withText(R.string.ad_privacy_link_row_label)).check(doesNotExist());
    }

    @Test
    @LargeTest
    @Feature({"RenderTest"})
    @Features.DisableFeatures(ChromeFeatureList.PRIVACY_SANDBOX_SETTINGS_4)
    public void testRenderIncognitoLockView_DeviceScreenLockDisabled() throws IOException {
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
    @Features.DisableFeatures(ChromeFeatureList.PRIVACY_SANDBOX_SETTINGS_4)
    public void testRenderIncognitoLockView_DeviceScreenLockEnabled() throws IOException {
        IncognitoReauthManager.setIsIncognitoReauthFeatureAvailableForTesting(true);
        IncognitoReauthSettingUtils.setIsDeviceScreenLockEnabledForTesting(true);

        mSettingsActivityTestRule.startSettingsActivity();
        waitForOptionsMenu();
        PrivacySettings fragment = mSettingsActivityTestRule.getFragment();

        mRenderTestRule.render(getIncognitoReauthSettingView(fragment),
                "incognito_reauth_setting_screen_lock_enabled");
    }

    @Test
    @LargeTest
    @Features.EnableFeatures(ChromeFeatureList.PRIVACY_GUIDE)
    public void testPrivacyGuideLinkRowEntryPointUserAction() throws IOException {
        mSettingsActivityTestRule.startSettingsActivity();
        mActionTester = new UserActionTester();
        PrivacySettings fragment = mSettingsActivityTestRule.getFragment();
        // Scroll down and open Privacy Guide page.
        scrollToSetting(withText(R.string.prefs_privacy_guide_title));
        onView(withText(R.string.prefs_privacy_guide_title)).perform(click());
        // Verify that the user action is emitted when privacy guide is clicked
        assertTrue(
                mActionTester.getActions().contains("Settings.PrivacyGuide.StartPrivacySettings"));
    }

    @Test
    @LargeTest
    @Features.EnableFeatures(ChromeFeatureList.PRIVACY_GUIDE)
    public void testPrivacyGuideLinkRowEntryExitHistogram() throws IOException {
        mSettingsActivityTestRule.startSettingsActivity();
        PrivacySettings fragment = mSettingsActivityTestRule.getFragment();

        assertEquals(0,
                mHistogramTestRule.getHistogramValueCount(
                        ENTRY_EXIT_HISTOGRAM, PrivacyGuideInteractions.SETTINGS_LINK_ROW_ENTRY));

        // Scroll down and open Privacy Guide page.
        scrollToSetting(withText(R.string.prefs_privacy_guide_title));
        onView(withText(R.string.prefs_privacy_guide_title)).perform(click());

        assertEquals(1,
                mHistogramTestRule.getHistogramValueCount(
                        ENTRY_EXIT_HISTOGRAM, PrivacyGuideInteractions.SETTINGS_LINK_ROW_ENTRY));
    }
}

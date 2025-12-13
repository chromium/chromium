// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_sandbox;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.action.ViewActions.scrollTo;
import static androidx.test.espresso.assertion.ViewAssertions.doesNotExist;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.RootMatchers.isDialog;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;

import static org.hamcrest.CoreMatchers.not;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;

import static org.chromium.ui.test.util.ViewUtils.clickOnClickableSpan;
import static org.chromium.ui.test.util.ViewUtils.onViewWaiting;

import android.app.Dialog;
import android.content.Context;
import android.os.Build;
import android.os.Bundle;
import android.view.View;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.espresso.PerformException;
import androidx.test.filters.SmallTest;

import org.hamcrest.Matcher;
import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.UserActionTester;
import org.chromium.chrome.browser.customtabs.CustomTabActivityTestRule;
import org.chromium.chrome.browser.customtabs.CustomTabsIntentTestUtils;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.settings.SettingsNavigationFactory;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.AutoResetCtaTransitTestRule;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.components.browser_ui.settings.SettingsNavigation;
import org.chromium.ui.test.util.RenderTestRule;

import java.io.IOException;

/** Tests {@link PrivacySandboxDialog}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@DoNotBatch(reason = "Need to evaluate these tests for batching; some test startup behavior.")
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public final class PrivacySandboxDialogTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public final AutoResetCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.fastAutoResetCtaActivityRule();

    @Rule
    public CustomTabActivityTestRule mCustomTabActivityTestRule = new CustomTabActivityTestRule();

    @Rule
    public ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setBugComponent(ChromeRenderTestRule.Component.UI_BROWSER_PRIVACY_SANDBOX)
                    .setRevision(3)
                    .setDescription("Launched button equalization")
                    .build();

    private FakePrivacySandboxBridge mFakePrivacySandboxBridge;

    @Mock private SettingsNavigation mSettingsNavigation;

    private Dialog mDialog;
    private String mTestPage;
    private UserActionTester mUserActionTester;
    private WebPageStation mPage;

    @Before
    public void setUp() {
        mTestPage =
                mActivityTestRule.getTestServer().getURL("/chrome/test/data/android/google.html");

        mFakePrivacySandboxBridge = new FakePrivacySandboxBridge();
        PrivacySandboxBridgeJni.setInstanceForTesting(mFakePrivacySandboxBridge);
        PrivacySandboxDialogController.disableAnimations(true);
        SettingsNavigationFactory.setInstanceForTesting(mSettingsNavigation);
        mUserActionTester = new UserActionTester();
    }

    @After
    public void tearDown() {
        if (mUserActionTester != null) {
            mUserActionTester.tearDown();
            mUserActionTester = null;
        }
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    // Dismiss the dialog between the tests. Necessary due to batching.
                    if (mDialog != null) {
                        mDialog.dismiss();
                        mDialog = null;
                    }
                });
    }

    private void renderViewWithId(int id, String renderId) {
        onViewWaiting(withId(id), true);
        onView(withId(id))
                .inRoot(isDialog())
                .check(
                        (v, noMatchException) -> {
                            if (noMatchException != null) throw noMatchException;
                            try {
                                ThreadUtils.runOnUiThreadBlocking(() -> RenderTestRule.sanitize(v));
                                mRenderTestRule.render(v, renderId);
                            } catch (IOException e) {
                                throw new AssertionError(e);
                            }
                        });
    }

    private void launchDialog() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    if (mDialog != null) {
                        mDialog.dismiss();
                        mDialog = null;
                    }
                    PrivacySandboxDialogController.maybeLaunchPrivacySandboxDialog(
                            mActivityTestRule.getActivity(),
                            mActivityTestRule.getProfile(false),
                            SurfaceType.BR_APP,
                            mActivityTestRule.getActivity().getWindowAndroid());
                    mDialog = PrivacySandboxDialogController.getDialog();
                });
    }

    // Returns whether the "more" button was clicked.
    private boolean tryClickOn(Matcher<View> viewMatcher) {
        boolean result = clickMoreButtonUntilFullyScrolledDown();
        onViewWaiting(viewMatcher, true).perform(click());
        return result;
    }

    private boolean clickMoreButtonUntilFullyScrolledDown() {
        boolean moreClicked = false;
        while (true) {
            try {
                onView(withId(R.id.more_button)).inRoot(isDialog()).perform(click());
                moreClicked = true;
                var promptType =
                        mFakePrivacySandboxBridge.getRequiredPromptType(SurfaceType.BR_APP);
                if (promptType == PromptType.M1_CONSENT) {
                    assertEquals(
                            "Last dialog action",
                            PromptAction.CONSENT_MORE_BUTTON_CLICKED,
                            (int) mFakePrivacySandboxBridge.getLastPromptAction());
                } else if (promptType == PromptType.M1_NOTICE_EEA
                        || promptType == PromptType.M1_NOTICE_ROW) {
                    assertEquals(
                            "Last dialog action",
                            PromptAction.NOTICE_MORE_BUTTON_CLICKED,
                            (int) mFakePrivacySandboxBridge.getLastPromptAction());
                } else if (promptType == PromptType.M1_NOTICE_RESTRICTED) {
                    assertEquals(
                            "Last dialog action",
                            PromptAction.RESTRICTED_NOTICE_MORE_BUTTON_CLICKED,
                            (int) mFakePrivacySandboxBridge.getLastPromptAction());
                }
            } catch (PerformException e) {
                return moreClicked;
            }
        }
    }

    @Test
    @SmallTest
    @Feature({"RenderTest"})
    @DisableFeatures(ChromeFeatureList.PRIVACY_SANDBOX_ADS_API_UX_ENHANCEMENTS)
    public void renderEEAConsent() throws IOException {
        mPage = mActivityTestRule.startOnBlankPage();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mDialog =
                            new PrivacySandboxDialogConsentEEA(
                                    mActivityTestRule.getActivity(),
                                    new PrivacySandboxBridge(mActivityTestRule.getProfile(false)),
                                    false,
                                    SurfaceType.BR_APP,
                                    mActivityTestRule.getProfile(false),
                                    mActivityTestRule.getActivity().getWindowAndroid());
                    mDialog.show();
                });
        renderViewWithId(R.id.privacy_sandbox_dialog, "privacy_sandbox_eea_consent_dialog");
    }

    @Test
    @SmallTest
    @Feature({"RenderTest"})
    @EnableFeatures(ChromeFeatureList.PRIVACY_SANDBOX_ADS_API_UX_ENHANCEMENTS)
    @DisableFeatures(ChromeFeatureList.PRIVACY_SANDBOX_AD_TOPICS_CONTENT_PARITY)
    @DisabledTest(message = "https://crbug.com/414613581")
    public void renderEeaConsentV2() throws IOException {
        mPage = mActivityTestRule.startOnBlankPage();
        mFakePrivacySandboxBridge.setRequiredPromptType(PromptType.M1_CONSENT);
        launchDialog();
        onViewWaiting(withId(R.id.privacy_sandbox_dialog));
        renderViewWithId(R.id.privacy_sandbox_dialog, "privacy_sandbox_eea_consent_dialog_v2");
    }

    @Test
    @SmallTest
    @Feature({"RenderTest"})
    @EnableFeatures({ChromeFeatureList.PRIVACY_SANDBOX_ADS_API_UX_ENHANCEMENTS})
    @DisableFeatures(ChromeFeatureList.PRIVACY_SANDBOX_AD_TOPICS_CONTENT_PARITY)
    // TODO(crbug.com/381241999): fix and re-enable on ARM devices.
    @DisableIf.Build(supported_abis_includes = "armeabi-v7a")
    @DisableIf.Build(supported_abis_includes = "arm64-v8a")
    public void renderEeaConsentV2PrivacyPolicyEnabled() throws IOException {
        mPage = mActivityTestRule.startOnBlankPage();
        mFakePrivacySandboxBridge.setRequiredPromptType(PromptType.M1_CONSENT);
        launchDialog();
        onViewWaiting(withId(R.id.privacy_sandbox_dialog));
        onView(withId(R.id.dropdown_element)).inRoot(isDialog()).perform(scrollTo(), click());
        renderViewWithId(
                R.id.privacy_sandbox_dialog,
                "privacy_sandbox_eea_consent_dialog_v2_privacy_policy_link_shown");
    }

    @Test
    @SmallTest
    @Feature({"RenderTest"})
    @EnableFeatures({
        ChromeFeatureList.PRIVACY_SANDBOX_ADS_API_UX_ENHANCEMENTS,
        ChromeFeatureList.PRIVACY_SANDBOX_AD_TOPICS_CONTENT_PARITY
    })
    @DisabledTest(message = "https://crbug.com/425457237")
    public void renderEeaConsentV2ContentParity() throws IOException {
        mPage = mActivityTestRule.startOnBlankPage();
        mFakePrivacySandboxBridge.setRequiredPromptType(PromptType.M1_CONSENT);
        launchDialog();
        onViewWaiting(withId(R.id.privacy_sandbox_dialog));
        renderViewWithId(
                R.id.privacy_sandbox_dialog,
                "privacy_sandbox_eea_consent_dialog_v2_content_parity");
    }

    @Test
    @SmallTest
    @Feature({"RenderTest"})
    @EnableFeatures({
        ChromeFeatureList.PRIVACY_SANDBOX_ADS_API_UX_ENHANCEMENTS,
        ChromeFeatureList.PRIVACY_SANDBOX_AD_TOPICS_CONTENT_PARITY
    })
    @DisabledTest(message = "https://crbug.com/399734809")
    public void renderEeaConsentV2ContentParityPrivacyPolicyEnabled() throws IOException {
        mPage = mActivityTestRule.startOnBlankPage();
        mFakePrivacySandboxBridge.setRequiredPromptType(PromptType.M1_CONSENT);
        launchDialog();
        onViewWaiting(withId(R.id.privacy_sandbox_dialog));
        onView(withId(R.id.dropdown_element)).inRoot(isDialog()).perform(scrollTo(), click());
        renderViewWithId(
                R.id.privacy_sandbox_dialog,
                "privacy_sandbox_eea_consent_dialog_v2_content_parity_privacy_policy_link_shown");
    }

    @Test
    @SmallTest
    @DisableFeatures(ChromeFeatureList.PRIVACY_SANDBOX_ADS_API_UX_ENHANCEMENTS)
    // TODO(crbug.com/369540483): fix and re-enable on ARM devices.
    @DisableIf.Build(supported_abis_includes = "armeabi-v7a")
    @DisableIf.Build(supported_abis_includes = "arm64-v8a")
    public void eeaConsentPrivacyPolicyLink() throws IOException {
        mPage = mActivityTestRule.startOnBlankPage();
        mFakePrivacySandboxBridge.setRequiredPromptType(PromptType.M1_CONSENT);
        launchDialog();
        onViewWaiting(withId(R.id.privacy_sandbox_dialog));
        onView(withId(R.id.dropdown_element)).inRoot(isDialog()).perform(scrollTo(), click());
        onView(withId(R.id.privacy_sandbox_learn_more_text))
                .inRoot(isDialog())
                .check(matches(isDisplayed()));
        // Click "Privacy Policy" link
        onView(withId(R.id.privacy_sandbox_learn_more_text))
                .inRoot(isDialog())
                .perform(clickOnClickableSpan(0));
        // Validate EEA Consent is not shown
        onView(withId(R.id.privacy_sandbox_consent_eea_view))
                .inRoot(isDialog())
                .check(matches(not(isDisplayed())));
        // Validate Privacy Policy View is shown
        onView(withId(R.id.privacy_policy_view)).inRoot(isDialog()).check(matches(isDisplayed()));
        onView(withId(R.id.privacy_policy_title)).inRoot(isDialog()).check(matches(isDisplayed()));
        onView(withId(R.id.privacy_policy_back_button))
                .inRoot(isDialog())
                .check(matches(isDisplayed()));
        assertEquals(
                1,
                mUserActionTester.getActionCount(
                        "Settings.PrivacySandbox.Consent.PrivacyPolicyLinkClicked"));
        HistogramWatcher watcher =
                HistogramWatcher.newSingleRecordWatcher("PrivacySandbox.PrivacyPolicy.LoadingTime");
        watcher.pollInstrumentationThreadUntilSatisfied();
        // Click back button
        onView(withId(R.id.privacy_policy_back_button)).inRoot(isDialog()).perform(click());
        // Validate EEA Consent is shown
        onView(withId(R.id.privacy_sandbox_consent_eea_view))
                .inRoot(isDialog())
                .check(matches(isDisplayed()));
        // Validate Privacy Policy View is not shown
        onView(withId(R.id.privacy_policy_view))
                .inRoot(isDialog())
                .check(matches(not(isDisplayed())));
    }

    @Test
    @SmallTest
    @Feature({"RenderTest"})
    @DisableFeatures(ChromeFeatureList.PRIVACY_SANDBOX_ADS_API_UX_ENHANCEMENTS)
    // TODO(crbug.com/381241999): fix and re-enable on ARM devices.
    @DisableIf.Build(supported_abis_includes = "armeabi-v7a")
    @DisableIf.Build(supported_abis_includes = "arm64-v8a")
    public void renderEEAConsentPrivacyPolicyLink() throws IOException {
        mPage = mActivityTestRule.startOnBlankPage();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mDialog =
                            new PrivacySandboxDialogConsentEEA(
                                    mActivityTestRule.getActivity(),
                                    new PrivacySandboxBridge(mActivityTestRule.getProfile(false)),
                                    false,
                                    SurfaceType.BR_APP,
                                    mActivityTestRule.getProfile(false),
                                    mActivityTestRule.getActivity().getWindowAndroid());
                    mDialog.show();
                });
        onViewWaiting(withId(R.id.privacy_sandbox_dialog));
        onView(withId(R.id.dropdown_element)).inRoot(isDialog()).perform(scrollTo(), click());
        renderViewWithId(
                R.id.privacy_sandbox_dialog, "privacy_sandbox_eea_consent_privacy_policy_link");
    }

    @Test
    @SmallTest
    @Feature({"RenderTest"})
    @EnableFeatures(ChromeFeatureList.PRIVACY_SANDBOX_ADS_API_UX_ENHANCEMENTS)
    public void renderEeaNoticeV2() throws IOException {
        mPage = mActivityTestRule.startOnBlankPage();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mDialog =
                            new PrivacySandboxDialogNoticeEeaV2(
                                    mActivityTestRule.getActivity(),
                                    new PrivacySandboxBridge(mActivityTestRule.getProfile(false)),
                                    SurfaceType.BR_APP,
                                    mActivityTestRule.getProfile(false),
                                    mActivityTestRule.getActivity().getWindowAndroid());
                    mDialog.show();
                });
        onViewWaiting(withId(R.id.privacy_sandbox_dialog));
        renderViewWithId(R.id.privacy_sandbox_dialog, "privacy_sandbox_eea_notice_dialog_v2");
    }

    @Test
    @SmallTest
    @Feature({"RenderTest"})
    @EnableFeatures({ChromeFeatureList.PRIVACY_SANDBOX_ADS_API_UX_ENHANCEMENTS})
    public void renderEeaNoticeV2PrivacyPolicyEnabled() throws IOException {
        mPage = mActivityTestRule.startOnBlankPage();
        mFakePrivacySandboxBridge.setRequiredPromptType(PromptType.M1_NOTICE_EEA);
        launchDialog();
        onViewWaiting(withId(R.id.privacy_sandbox_dialog));
        onView(withId(R.id.site_suggested_ads_dropdown_element))
                .inRoot(isDialog())
                .perform(scrollTo(), click());
        renderViewWithId(
                R.id.privacy_sandbox_dialog,
                "privacy_sandbox_eea_notice_dialog_v2_privacy_policy_link_shown");
    }

    @Test
    @SmallTest
    @Feature({"RenderTest"})
    @EnableFeatures(ChromeFeatureList.PRIVACY_SANDBOX_ADS_API_UX_ENHANCEMENTS)
    public void renderEeaNoticeV2AdMeasurementDropdown() throws IOException {
        mPage = mActivityTestRule.startOnBlankPage();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mDialog =
                            new PrivacySandboxDialogNoticeEeaV2(
                                    mActivityTestRule.getActivity(),
                                    new PrivacySandboxBridge(mActivityTestRule.getProfile(false)),
                                    SurfaceType.BR_APP,
                                    mActivityTestRule.getProfile(false),
                                    mActivityTestRule.getActivity().getWindowAndroid());
                    mDialog.show();
                });
        onViewWaiting(withId(R.id.privacy_sandbox_dialog));
        tryClickOn(withId(R.id.ad_measurement_dropdown_element));
        renderViewWithId(
                R.id.privacy_sandbox_dialog,
                "privacy_sandbox_eea_notice_dialog_v2_ad_measurement_dropdown");
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.PRIVACY_SANDBOX_ADS_API_UX_ENHANCEMENTS)
    public void eeaNoticeV2AckButton() throws IOException {
        mPage = mActivityTestRule.startOnBlankPage();
        mFakePrivacySandboxBridge.setRequiredPromptType(PromptType.M1_NOTICE_EEA);
        launchDialog();
        // Verify that the EEA notice is shown.
        onViewWaiting(withId(R.id.privacy_sandbox_notice_title), true);
        assertEquals(
                "Last dialog action",
                PromptAction.NOTICE_SHOWN,
                (int) mFakePrivacySandboxBridge.getLastPromptAction());
        // Ack the notice and verify it worked correctly.
        tryClickOn(withId(R.id.ack_button));
        assertEquals(
                "Last dialog action",
                PromptAction.NOTICE_ACKNOWLEDGE,
                (int) mFakePrivacySandboxBridge.getLastPromptAction());
        onView(withId(R.id.privacy_sandbox_notice_title)).check(doesNotExist());
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.PRIVACY_SANDBOX_ADS_API_UX_ENHANCEMENTS)
    public void eeaNoticeV2SiteSuggestedAdsDropdown() throws IOException {
        mPage = mActivityTestRule.startOnBlankPage();
        mFakePrivacySandboxBridge.setRequiredPromptType(PromptType.M1_NOTICE_EEA);
        launchDialog();
        // Verify the EEA Notice is shown.
        onViewWaiting(withId(R.id.privacy_sandbox_notice_title), true);
        // Click on the site suggested ads expanding section and verify it worked correctly.
        tryClickOn(withId(R.id.site_suggested_ads_dropdown_element));
        assertEquals(
                "Last dialog action",
                PromptAction.NOTICE_SITE_SUGGESTED_ADS_MORE_INFO_OPENED,
                (int) mFakePrivacySandboxBridge.getLastPromptAction());

        onView(withId(R.id.privacy_sandbox_notice_eea_site_suggested_ads_dropdown))
                .inRoot(isDialog())
                .perform(scrollTo());
        onView(withId(R.id.privacy_sandbox_notice_eea_site_suggested_ads_dropdown))
                .inRoot(isDialog())
                .check(matches(isDisplayed()));
        tryClickOn(withId(R.id.site_suggested_ads_dropdown_element));
        assertEquals(
                "Last dialog action",
                PromptAction.NOTICE_SITE_SUGGESTED_ADS_MORE_INFO_CLOSED,
                (int) mFakePrivacySandboxBridge.getLastPromptAction());
        onView(withId(R.id.privacy_sandbox_notice_eea_site_suggested_ads_dropdown))
                .check(doesNotExist());
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.PRIVACY_SANDBOX_ADS_API_UX_ENHANCEMENTS)
    public void eeaNoticeV2AdMeasurementDropdown() throws IOException {
        mPage = mActivityTestRule.startOnBlankPage();
        mFakePrivacySandboxBridge.setRequiredPromptType(PromptType.M1_NOTICE_EEA);
        launchDialog();
        // Verify the EEA Notice is shown.
        onViewWaiting(withId(R.id.privacy_sandbox_notice_title), true);
        // Click on the Ad Measurement expanding section and verify it worked correctly.
        tryClickOn(withId(R.id.ad_measurement_dropdown_element));
        assertEquals(
                "Last dialog action",
                PromptAction.NOTICE_ADS_MEASUREMENT_MORE_INFO_OPENED,
                (int) mFakePrivacySandboxBridge.getLastPromptAction());

        onView(withId(R.id.privacy_sandbox_notice_eea_ad_measurement_dropdown))
                .inRoot(isDialog())
                .perform(scrollTo());
        onView(withId(R.id.privacy_sandbox_notice_eea_ad_measurement_dropdown))
                .inRoot(isDialog())
                .check(matches(isDisplayed()));
        tryClickOn(withId(R.id.ad_measurement_dropdown_element));
        assertEquals(
                "Last dialog action",
                PromptAction.NOTICE_ADS_MEASUREMENT_MORE_INFO_CLOSED,
                (int) mFakePrivacySandboxBridge.getLastPromptAction());
        onView(withId(R.id.privacy_sandbox_notice_eea_ad_measurement_dropdown))
                .check(doesNotExist());
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.PRIVACY_SANDBOX_ADS_API_UX_ENHANCEMENTS)
    public void eeaNoticeV2SettingsButton() throws IOException {
        mPage = mActivityTestRule.startOnBlankPage();
        mFakePrivacySandboxBridge.setRequiredPromptType(PromptType.M1_NOTICE_EEA);
        launchDialog();
        // Verify the EEA Notice is shown.
        onViewWaiting(withId(R.id.privacy_sandbox_notice_title), true);
        // Click on the settings button and verify it worked correctly.
        tryClickOn(withId(R.id.settings_button));
        onView(withId(R.id.privacy_sandbox_notice_title)).check(doesNotExist());
        assertEquals(
                "Last dialog action",
                PromptAction.NOTICE_OPEN_SETTINGS,
                (int) mFakePrivacySandboxBridge.getLastPromptAction());
        Mockito.verify(mSettingsNavigation)
                .startSettings(
                        any(Context.class),
                        eq(PrivacySandboxSettingsFragment.class),
                        any(Bundle.class),
                        eq(false));
    }

    @Test
    @SmallTest
    @Feature({"RenderTest"})
    @DisableFeatures(ChromeFeatureList.PRIVACY_SANDBOX_ADS_API_UX_ENHANCEMENTS)
    public void renderEEANotice() throws IOException {
        mPage = mActivityTestRule.startOnBlankPage();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mDialog =
                            new PrivacySandboxDialogNoticeEEA(
                                    mActivityTestRule.getActivity(),
                                    new PrivacySandboxBridge(mActivityTestRule.getProfile(false)),
                                    SurfaceType.BR_APP);
                    mDialog.show();
                });
        renderViewWithId(R.id.privacy_sandbox_dialog, "privacy_sandbox_eea_notice_dialog");
    }

    @Test
    @SmallTest
    @Feature({"RenderTest"})
    @DisableFeatures(ChromeFeatureList.PRIVACY_SANDBOX_ADS_API_UX_ENHANCEMENTS)
    public void renderEeaNoticeAdMeasurementDropdown() throws IOException {
        mPage = mActivityTestRule.startOnBlankPage();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mDialog =
                            new PrivacySandboxDialogNoticeEEA(
                                    mActivityTestRule.getActivity(),
                                    new PrivacySandboxBridge(mActivityTestRule.getProfile(false)),
                                    SurfaceType.BR_APP);
                    mDialog.show();
                });
        onViewWaiting(withId(R.id.privacy_sandbox_dialog));
        tryClickOn(withId(R.id.dropdown_element));
        renderViewWithId(
                R.id.privacy_sandbox_dialog,
                "privacy_sandbox_eea_notice_dialog_measurement_dropdown");
    }

    @Test
    @SmallTest
    @Feature({"RenderTest"})
    @DisableFeatures(ChromeFeatureList.PRIVACY_SANDBOX_ADS_API_UX_ENHANCEMENTS)
    public void renderROWNotice() throws IOException {
        mPage = mActivityTestRule.startOnBlankPage();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mDialog =
                            new PrivacySandboxDialogNoticeROW(
                                    mActivityTestRule.getActivity(),
                                    new PrivacySandboxBridge(mActivityTestRule.getProfile(false)),
                                    SurfaceType.BR_APP,
                                    mActivityTestRule.getProfile(false),
                                    mActivityTestRule.getActivity().getWindowAndroid());
                    mDialog.show();
                });
        renderViewWithId(R.id.privacy_sandbox_dialog, "privacy_sandbox_row_notice_dialog");
    }

    @Test
    @SmallTest
    @Feature({"RenderTest"})
    @EnableFeatures(ChromeFeatureList.PRIVACY_SANDBOX_ADS_API_UX_ENHANCEMENTS)
    @DisabledTest(message = "https://crbug.com/383531831 - the test is flaky")
    public void renderRowNoticeV2() throws IOException {
        mPage = mActivityTestRule.startOnBlankPage();
        mFakePrivacySandboxBridge.setRequiredPromptType(PromptType.M1_NOTICE_ROW);
        launchDialog();
        onViewWaiting(withId(R.id.privacy_sandbox_dialog));
        renderViewWithId(R.id.privacy_sandbox_dialog, "privacy_sandbox_row_notice_dialog_v2");
    }

    @Test
    @SmallTest
    @Feature({"RenderTest"})
    @EnableFeatures({ChromeFeatureList.PRIVACY_SANDBOX_ADS_API_UX_ENHANCEMENTS})
    @DisabledTest(message = "https://crbug.com/383473428 - the test is flaky")
    public void renderRowNoticeV2PrivacyPolicyEnabled() throws IOException {
        mPage = mActivityTestRule.startOnBlankPage();
        mFakePrivacySandboxBridge.setRequiredPromptType(PromptType.M1_NOTICE_ROW);
        launchDialog();
        onViewWaiting(withId(R.id.privacy_sandbox_dialog));
        onView(withId(R.id.dropdown_element)).inRoot(isDialog()).perform(scrollTo(), click());
        onView(withId(R.id.dropdown_container)).inRoot(isDialog()).check(matches(isDisplayed()));
        onView(withId(R.id.privacy_sandbox_m1_notice_row_learn_more_description_5_v2))
                .inRoot(isDialog())
                .perform(scrollTo());
        renderViewWithId(
                R.id.privacy_sandbox_dialog,
                "privacy_sandbox_row_notice_dialog_v2_privacy_policy_link_shown");
    }

    @Test
    @SmallTest
    @Feature({"RenderTest"})
    public void renderRestrictedNotice() throws IOException {
        mPage = mActivityTestRule.startOnBlankPage();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mDialog =
                            new PrivacySandboxDialogNoticeRestricted(
                                    mActivityTestRule.getActivity(),
                                    new PrivacySandboxBridge(mActivityTestRule.getProfile(false)),
                                    SurfaceType.BR_APP,
                                    /* showMoreButtonForTesting= */ false);
                    mDialog.show();
                });
        renderViewWithId(R.id.privacy_sandbox_dialog, "privacy_sandbox_restricted_notice_dialog");
    }

    @Test
    @SmallTest
    public void controllerIncognito() throws IOException {
        mPage = mActivityTestRule.startOnBlankPage();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    PrivacySandboxDialogController.maybeLaunchPrivacySandboxDialog(
                            mActivityTestRule.getActivity(),
                            mActivityTestRule.getProfile(true),
                            SurfaceType.BR_APP,
                            mActivityTestRule.getActivity().getWindowAndroid());
                });
        // Verify that nothing is shown.
        onView(withId(R.id.privacy_sandbox_dialog)).check(doesNotExist());
    }

    @Test
    @SmallTest
    public void controllerShowsNothing() throws IOException {
        mPage = mActivityTestRule.startOnBlankPage();
        mFakePrivacySandboxBridge.setRequiredPromptType(PromptType.NONE);
        launchDialog();
        // Verify that nothing is shown. Notice & Consent share a title.
        onView(withId(R.id.privacy_sandbox_dialog)).check(doesNotExist());
    }

    @Test
    @SmallTest
    @EnableFeatures({
        ChromeFeatureList.PRIVACY_SANDBOX_SETTINGS_4
                + ":force-show-notice-row-for-testing/true/notice-required/true"
    })
    @DisableIf.Build(sdk_equals = Build.VERSION_CODES.Q, message = "crbug.com/401594334")
    public void cctLaunchDialogUpdatesDialogClass() throws IOException {
        mPage = mActivityTestRule.startOnBlankPage();
        mFakePrivacySandboxBridge.setRequiredPromptType(PromptType.M1_NOTICE_ROW);
        // Launch a CCT activity and click a button
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(
                CustomTabsIntentTestUtils.createMinimalCustomTabIntent(
                        ApplicationProvider.getApplicationContext(), mTestPage));

        onViewWaiting(withId(R.id.privacy_sandbox_dialog), true);
        tryClickOn(withId(R.id.ack_button));
        assertEquals(
                "Set surface type",
                SurfaceType.AGACCT,
                (int) mFakePrivacySandboxBridge.getLastSurfaceType());
    }

    @Test
    @SmallTest
    @EnableFeatures({
        ChromeFeatureList.PRIVACY_SANDBOX_SETTINGS_4
                + ":force-show-notice-row-for-testing/true/notice-required/true/suppress-dialog-for-external-app-launches/false"
    })
    @CommandLineFlags.Remove({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
    public void brAppLaunchDialogUpdatesDialogClass() throws IOException {
        // Do not call mActivityTestRule.startOnBlankPage() like other tests because the blank
        // page isn't actually shown, but covered .
        mFakePrivacySandboxBridge.setRequiredPromptType(PromptType.M1_NOTICE_ROW);
        // Launch a basic activity and click a button
        mActivityTestRule.getActivityTestRule().loadUrlNoWaiting(mTestPage);

        onViewWaiting(withId(R.id.privacy_sandbox_dialog), true);
        tryClickOn(withId(R.id.ack_button));
        assertEquals(
                "Set surface type",
                SurfaceType.BR_APP,
                (int) mFakePrivacySandboxBridge.getLastSurfaceType());
    }

    @Test
    @SmallTest
    public void controllerShowsEEAConsent() throws IOException {
        mPage = mActivityTestRule.startOnBlankPage();
        PrivacySandboxDialogController.disableEEANotice(true);

        mFakePrivacySandboxBridge.setRequiredPromptType(PromptType.M1_CONSENT);
        launchDialog();

        // Verify that the EEA consent is shown
        onViewWaiting(withId(R.id.privacy_sandbox_m1_consent_title), true);
        assertEquals(
                "Last dialog action",
                PromptAction.CONSENT_SHOWN,
                (int) mFakePrivacySandboxBridge.getLastPromptAction());
        // Accept the consent and verify it worked correctly.
        tryClickOn(withId(R.id.ack_button));
        assertEquals(
                1,
                mUserActionTester.getActionCount(
                        "Settings.PrivacySandbox.ConsentDialog.AckClicked"));
        assertEquals(
                "Last dialog action",
                PromptAction.CONSENT_ACCEPTED,
                (int) mFakePrivacySandboxBridge.getLastPromptAction());
        onView(withId(R.id.privacy_sandbox_consent_eea_dropdown)).check(doesNotExist());
    }

    @Test
    @SmallTest
    public void controllerShowsEEAConsentDropdown() {
        mPage = mActivityTestRule.startOnBlankPage();
        PrivacySandboxDialogController.disableEEANotice(true);

        mFakePrivacySandboxBridge.setRequiredPromptType(PromptType.M1_CONSENT);
        launchDialog();

        // Click on the expanding section and verify it worked correctly.
        onViewWaiting(withId(R.id.privacy_sandbox_m1_consent_title), true);
        onView(withId(R.id.dropdown_element)).inRoot(isDialog()).perform(scrollTo(), click());
        assertEquals(
                "Last dialog action",
                PromptAction.CONSENT_MORE_INFO_OPENED,
                (int) mFakePrivacySandboxBridge.getLastPromptAction());

        onView(withId(R.id.privacy_sandbox_consent_eea_dropdown)).inRoot(isDialog()).perform(scrollTo());
        onView(withId(R.id.privacy_sandbox_consent_eea_dropdown)).inRoot(isDialog()).check(matches(isDisplayed()));
        onView(withId(R.id.dropdown_element)).inRoot(isDialog()).perform(scrollTo(), click());
        assertEquals(
                "Last dialog action",
                PromptAction.CONSENT_MORE_INFO_CLOSED,
                (int) mFakePrivacySandboxBridge.getLastPromptAction());
        onView(withId(R.id.privacy_sandbox_consent_eea_dropdown)).inRoot(isDialog()).check(doesNotExist());

        // Decline the consent and verify it worked correctly.
        tryClickOn(withId(R.id.no_button));
        assertEquals(
                "Last dialog action",
                PromptAction.CONSENT_DECLINED,
                (int) mFakePrivacySandboxBridge.getLastPromptAction());
        onView(withId(R.id.privacy_sandbox_consent_eea_dropdown)).check(doesNotExist());
        assertEquals(
                1,
                mUserActionTester.getActionCount(
                        "Settings.PrivacySandbox.ConsentDialog.NoClicked"));
    }

    @Test
    @SmallTest
    public void afterEEAConsentSpinnerAndNoticeAreShown() throws IOException {
        mPage = mActivityTestRule.startOnBlankPage();
        PrivacySandboxDialogController.disableAnimations(false);

        // Launch the consent
        mFakePrivacySandboxBridge.setRequiredPromptType(PromptType.M1_CONSENT);
        launchDialog();

        // Accept the consent and verify the spinner it's shown.
        tryClickOn(withId(R.id.ack_button));
        onViewWaiting(withId(R.id.privacy_sandbox_m1_consent_title), true)
                .check(matches(not(isDisplayed())));

        onView(withId(R.id.progress_bar_container))
                .inRoot(isDialog())
                .check(matches(isDisplayed()));

        // Wait for the spinner to disappear and check the notice is shown
        onViewWaiting(withId(R.id.privacy_sandbox_notice_title), true)
                .check(matches(isDisplayed()));

        onView(withId(R.id.privacy_sandbox_m1_consent_title))
                .inRoot(isDialog())
                .check(doesNotExist());
        onView(withId(R.id.progress_bar_container)).inRoot(isDialog()).check(doesNotExist());

        // Launch the consent
        launchDialog();

        // Decline the consent and verify the spinner it's shown.
        tryClickOn(withId(R.id.no_button));
        onViewWaiting(withId(R.id.privacy_sandbox_m1_consent_title), true)
                .check(matches(not(isDisplayed())));

        onView(withId(R.id.progress_bar_container))
                .inRoot(isDialog())
                .check(matches(isDisplayed()));

        // Wait for the spinner to disappear and check the notice is shown
        onViewWaiting(withId(R.id.privacy_sandbox_notice_title), true)
                .check(matches(isDisplayed()));
        onView(withId(R.id.privacy_sandbox_m1_consent_title))
                .inRoot(isDialog())
                .check(doesNotExist());
        onView(withId(R.id.progress_bar_container)).inRoot(isDialog()).check(doesNotExist());
    }

    @Test
    @SmallTest
    @DisableFeatures({ChromeFeatureList.PRIVACY_SANDBOX_ADS_API_UX_ENHANCEMENTS})
    public void controllerShowsEEANotice() throws IOException {
        mPage = mActivityTestRule.startOnBlankPage();
        mFakePrivacySandboxBridge.setRequiredPromptType(PromptType.M1_NOTICE_EEA);
        launchDialog();
        // Verify that the EEA notice is shown
        onViewWaiting(withId(R.id.privacy_sandbox_notice_title), true);
        assertEquals(
                "Last dialog action",
                PromptAction.NOTICE_SHOWN,
                (int) mFakePrivacySandboxBridge.getLastPromptAction());
        // Ack the notice and verify it worked correctly.
        tryClickOn(withId(R.id.ack_button));
        assertEquals(
                "Last dialog action",
                PromptAction.NOTICE_ACKNOWLEDGE,
                (int) mFakePrivacySandboxBridge.getLastPromptAction());
        onView(withId(R.id.privacy_sandbox_notice_title)).check(doesNotExist());
        // check for notice ack here
        assertEquals(
                1,
                mUserActionTester.getActionCount(
                        "Settings.PrivacySandbox.NoticeEeaDialog.AckClicked"));

        launchDialog();
        // Click on the expanding section and verify it worked correctly.
        onViewWaiting(withId(R.id.privacy_sandbox_notice_title), true);
        onView(withId(R.id.dropdown_element)).inRoot(isDialog()).perform(scrollTo(), click());
        assertEquals(
                "Last dialog action",
                PromptAction.NOTICE_MORE_INFO_OPENED,
                (int) mFakePrivacySandboxBridge.getLastPromptAction());

        onView(withId(R.id.privacy_sandbox_notice_eea_dropdown))
                .inRoot(isDialog())
                .perform(scrollTo());
        onView(withId(R.id.privacy_sandbox_notice_eea_dropdown))
                .inRoot(isDialog())
                .check(matches(isDisplayed()));
        onView(withId(R.id.dropdown_element)).inRoot(isDialog()).perform(scrollTo(), click());
        assertEquals(
                "Last dialog action",
                PromptAction.NOTICE_MORE_INFO_CLOSED,
                (int) mFakePrivacySandboxBridge.getLastPromptAction());
        onView(withId(R.id.privacy_sandbox_notice_eea_dropdown)).check(doesNotExist());

        // Click on the settings button and verify it worked correctly.
        tryClickOn(withId(R.id.settings_button));
        onView(withId(R.id.privacy_sandbox_notice_title)).check(doesNotExist());
        assertEquals(
                "Last dialog action",
                PromptAction.NOTICE_OPEN_SETTINGS,
                (int) mFakePrivacySandboxBridge.getLastPromptAction());
        assertEquals(
                1,
                mUserActionTester.getActionCount(
                        "Settings.PrivacySandbox.NoticeEeaDialog.OpenSettingsClicked"));
        Mockito.verify(mSettingsNavigation)
                .startSettings(
                        any(Context.class),
                        eq(PrivacySandboxSettingsFragment.class),
                        any(Bundle.class),
                        eq(false));
    }

    @Test
    @SmallTest
    public void controllerShowsROWNotice() throws IOException {
        mPage = mActivityTestRule.startOnBlankPage();
        mFakePrivacySandboxBridge.setRequiredPromptType(PromptType.M1_NOTICE_ROW);
        launchDialog();
        // Verify that the ROW notice is shown
        onViewWaiting(withId(R.id.privacy_sandbox_notice_title), true);
        assertEquals(
                "Last dialog action",
                PromptAction.NOTICE_SHOWN,
                (int) mFakePrivacySandboxBridge.getLastPromptAction());
        // Ack the notice and verify it worked correctly.
        tryClickOn(withId(R.id.ack_button));
        assertEquals(
                "Last dialog action",
                PromptAction.NOTICE_ACKNOWLEDGE,
                (int) mFakePrivacySandboxBridge.getLastPromptAction());
        onView(withId(R.id.privacy_sandbox_notice_title)).check(doesNotExist());
        assertEquals(
                1,
                mUserActionTester.getActionCount(
                        "Settings.PrivacySandbox.NoticeRowDialog.AckClicked"));

        launchDialog();
        // Click on the expanding section and verify it worked correctly.
        onViewWaiting(withId(R.id.privacy_sandbox_notice_title), true);
        onView(withId(R.id.dropdown_element)).inRoot(isDialog()).perform(scrollTo(), click());
        assertEquals(
                "Last dialog action",
                PromptAction.NOTICE_MORE_INFO_OPENED,
                (int) mFakePrivacySandboxBridge.getLastPromptAction());

        onView(withId(R.id.privacy_sandbox_notice_row_dropdown)).inRoot(isDialog()).check(matches(isDisplayed()));
        onView(withId(R.id.dropdown_element)).inRoot(isDialog()).perform(scrollTo(), click());
        assertEquals(
                "Last dialog action",
                PromptAction.NOTICE_MORE_INFO_CLOSED,
                (int) mFakePrivacySandboxBridge.getLastPromptAction());
        onView(withId(R.id.privacy_sandbox_notice_row_dropdown)).inRoot(isDialog()).check(doesNotExist());

        // Click on the settings button and verify it worked correctly.
        tryClickOn(withId(R.id.settings_button));
        assertEquals(
                "Last dialog action",
                PromptAction.NOTICE_OPEN_SETTINGS,
                (int) mFakePrivacySandboxBridge.getLastPromptAction());
        assertEquals(
                1,
                mUserActionTester.getActionCount(
                        "Settings.PrivacySandbox.NoticeRowDialog.OpenSettingsClicked"));
        onView(withId(R.id.privacy_sandbox_notice_title)).check(doesNotExist());
        Mockito.verify(mSettingsNavigation)
                .startSettings(
                        any(Context.class),
                        eq(PrivacySandboxSettingsFragment.class),
                        any(Bundle.class),
                        eq(false));
    }

    @Test
    @SmallTest
    public void controllerShowsRestrictedNotice() throws IOException {
        mPage = mActivityTestRule.startOnBlankPage();
        mFakePrivacySandboxBridge.setRequiredPromptType(PromptType.M1_NOTICE_RESTRICTED);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    PrivacySandboxDialogController.setShowMoreButton(false);
                });
        launchDialog();
        // Verify that the restricted notice is shown
        onViewWaiting(withId(R.id.privacy_sandbox_notice_title), true);
        assertEquals(
                "Last dialog action",
                PromptAction.RESTRICTED_NOTICE_SHOWN,
                (int) mFakePrivacySandboxBridge.getLastPromptAction());
        // Ack the notice and verify it worked correctly.
        tryClickOn(withId(R.id.ack_button));
        assertEquals(
                "Last dialog action",
                PromptAction.RESTRICTED_NOTICE_ACKNOWLEDGE,
                (int) mFakePrivacySandboxBridge.getLastPromptAction());
        onView(withId(R.id.privacy_sandbox_notice_title)).check(doesNotExist());
        assertEquals(
                1,
                mUserActionTester.getActionCount(
                        "Settings.PrivacySandbox.RestrictedNoticeDialog.AckClicked"));

        // Click on the settings button and verify it worked correctly.
        launchDialog();
        tryClickOn(withId(R.id.settings_button));
        assertEquals(
                "Last dialog action",
                PromptAction.RESTRICTED_NOTICE_OPEN_SETTINGS,
                (int) mFakePrivacySandboxBridge.getLastPromptAction());
        assertEquals(
                1,
                mUserActionTester.getActionCount(
                        "Settings.PrivacySandbox.RestrictedNoticeDialog.OpenSettingsClicked"));
        onView(withId(R.id.privacy_sandbox_notice_title)).check(doesNotExist());
        Mockito.verify(mSettingsNavigation)
                .startSettings(any(Context.class), eq(AdMeasurementFragment.class));
    }

    @Test
    @SmallTest
    public void controllerShowsRestrictedNoticeForceMoreButton() throws IOException {
        mPage = mActivityTestRule.startOnBlankPage();
        mFakePrivacySandboxBridge.setRequiredPromptType(PromptType.M1_NOTICE_RESTRICTED);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    PrivacySandboxDialogController.setShowMoreButton(true);
                });
        launchDialog();
        // Verify that the restricted notice is shown
        onViewWaiting(withId(R.id.privacy_sandbox_notice_title), true);
        assertEquals(
                "Last dialog action",
                PromptAction.RESTRICTED_NOTICE_SHOWN,
                (int) mFakePrivacySandboxBridge.getLastPromptAction());
        // "More" button should be clicked.
        assertTrue(tryClickOn(withId(R.id.ack_button)));
        assertEquals(
                "Last dialog action",
                PromptAction.RESTRICTED_NOTICE_ACKNOWLEDGE,
                (int) mFakePrivacySandboxBridge.getLastPromptAction());
        onView(withId(R.id.privacy_sandbox_notice_title)).check(doesNotExist());
        assertEquals(
                1,
                mUserActionTester.getActionCount(
                        "Settings.PrivacySandbox.RestrictedNoticeDialog.AckClicked"));

        // Click on the settings button and verify it worked correctly.
        launchDialog();
        // "More" button should be clicked.
        assertTrue(tryClickOn(withId(R.id.settings_button)));
        assertEquals(
                "Last dialog action",
                PromptAction.RESTRICTED_NOTICE_OPEN_SETTINGS,
                (int) mFakePrivacySandboxBridge.getLastPromptAction());
        assertEquals(
                1,
                mUserActionTester.getActionCount(
                        "Settings.PrivacySandbox.RestrictedNoticeDialog.OpenSettingsClicked"));
        onView(withId(R.id.privacy_sandbox_notice_title)).check(doesNotExist());
        Mockito.verify(mSettingsNavigation)
                .startSettings(any(Context.class), eq(AdMeasurementFragment.class));
    }
}

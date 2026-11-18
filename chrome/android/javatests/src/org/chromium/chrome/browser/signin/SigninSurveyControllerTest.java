// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin;

import androidx.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.junit.runners.model.Statement;

import org.chromium.base.FeatureOverrides;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.params.ParameterAnnotations.ClassParameter;
import org.chromium.base.test.params.ParameterAnnotations.UseRunnerDelegate;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Features;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.hats.TestSurveyUtils;
import org.chromium.chrome.browser.ui.signin.SigninSurveyController;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.chrome.test.util.browser.signin.SigninTestRule;
import org.chromium.components.messages.DismissReason;
import org.chromium.components.messages.MessageBannerProperties;
import org.chromium.components.messages.MessageDispatcher;
import org.chromium.components.messages.MessageDispatcherProvider;
import org.chromium.components.messages.MessageIdentifier;
import org.chromium.components.messages.MessageStateHandler;
import org.chromium.components.messages.MessagesTestHelper;
import org.chromium.components.signin.SigninFeatures;
import org.chromium.components.signin.test.util.TestAccounts;
import org.chromium.content_public.browser.test.NativeLibraryTestUtils;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.Arrays;
import java.util.List;
import java.util.Map;

/** Instrumentation tests for {@link SigninSurveyController}. */
// TODO(crbug.com/493130564): Revert to regular runner after
// MAKE_IDENTITY_MANAGER_SOURCE_OF_ACCOUNTS_PART2 launch.
@RunWith(ParameterizedRunner.class)
@UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@DoNotBatch(reason = "Activity should be restarted")
@CommandLineFlags.Add(ChromeSwitches.CHROME_FORCE_ENABLE_SURVEY)
public class SigninSurveyControllerTest {
    private static final String SIGNED_IN_STATUS = "Signed In";
    private static final String SIGNED_OUT_STATUS = "Signed Out";

    @ClassParameter
    private static final List<ParameterSet> sClassParameters =
            Arrays.asList(
                    new ParameterSet().value(false).name("withAccountManagerFacadeSource"),
                    new ParameterSet().value(true).name("withIdentityManagerSource"));

    @Rule(order = 0)
    public final TestRule mFeatureOverridesRule =
            (base, description) ->
                    new Statement() {
                        @Override
                        public void evaluate() throws Throwable {
                            FeatureOverrides.overrideFlag(
                                    SigninFeatures.MAKE_IDENTITY_MANAGER_SOURCE_OF_ACCOUNTS_PART2,
                                    mIsIdentityManagerSourceOfAccounts);
                            base.evaluate();
                        }
                    };

    @Rule(order = 1)
    public final SigninTestRule mSigninTestRule = new SigninTestRule();

    @Rule(order = 2)
    public final TestSurveyUtils.TestSurveyComponentRule mTestSurveyComponentRule =
            new TestSurveyUtils.TestSurveyComponentRule();

    @Rule(order = 3)
    public final FreshCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();

    private final boolean mIsIdentityManagerSourceOfAccounts;

    private PropertyModel mSurveyMessage;
    private MessageDispatcher mMessageDispatcher;

    public SigninSurveyControllerTest(boolean isIdentityManagerSourceOfAccounts) {
        mIsIdentityManagerSourceOfAccounts = isIdentityManagerSourceOfAccounts;
    }

    @Before
    public void setUp() {
        NativeLibraryTestUtils.loadNativeLibraryAndInitBrowserProcess();
        SigninSurveyController.enableWithoutDelayForTesting();
    }

    @Test
    @MediumTest
    public void acceptFreSigninSurvey() {
        acceptSigninSurvey(
                SigninSurveyController.SigninSurveyType.FRE, "HhgAhQYhw0tK1KeaPYj0NeTaRKBh");
    }

    @Test
    @MediumTest
    public void acceptBookmarkSigninSurvey() {
        acceptSigninSurvey(
                SigninSurveyController.SigninSurveyType.BOOKMARK_PROMO,
                "o2YBX3ZJc0tK1KeaPYj0UveLWhmf");
    }

    @Test
    @MediumTest
    public void acceptNtpSigninButton() {
        acceptSigninSurvey(
                SigninSurveyController.SigninSurveyType.NTP_SIGNIN_BUTTON,
                "yirfCKnhD0tK1KeaPYj0P9BTzPNw");
    }

    @Test
    @MediumTest
    public void acceptNtpAccountAvatarTap() {
        mSigninTestRule.addAccountThenSignin(TestAccounts.ACCOUNT1);
        mSigninTestRule.addAccount(TestAccounts.ACCOUNT2);
        acceptSigninSurvey(
                SigninSurveyController.SigninSurveyType.NTP_ACCOUNT_AVATAR_TAP,
                "DujcsCGkZ0tK1KeaPYj0RGm9FgKX",
                /* expectedAccountCount= */ 2,
                SIGNED_IN_STATUS);
    }

    @Test
    @MediumTest
    public void acceptNtpPromoSigninSurvey() {
        acceptSigninSurvey(
                SigninSurveyController.SigninSurveyType.NTP_PROMO, "15CWgMniG0tK1KeaPYj0RkWoZ4B9");
    }

    @Test
    @MediumTest
    public void acceptWebSigninSurvey() {
        acceptSigninSurvey(
                SigninSurveyController.SigninSurveyType.WEB, "36F2N72TP0tK1KeaPYj0SdXcHEJ4");
    }

    @Test
    @MediumTest
    @SigninTestRule.BlockGetAccounts
    public void acceptSigninSurveyWhileAccountsLoading() {
        mSigninTestRule.addAccount(TestAccounts.ACCOUNT1);
        acceptSigninSurvey(
                SigninSurveyController.SigninSurveyType.FRE,
                "HhgAhQYhw0tK1KeaPYj0NeTaRKBh",
                /* expectedAccountCount= */ 0,
                SIGNED_OUT_STATUS);
    }

    @Test
    @MediumTest
    public void dismissSigninSurvey() {
        showSigninSurvey(SigninSurveyController.SigninSurveyType.BOOKMARK_PROMO);
        waitForSurveyMessageToShow();

        ThreadUtils.runOnUiThreadBlocking(
                () -> mMessageDispatcher.dismissMessage(mSurveyMessage, DismissReason.GESTURE));

        Assert.assertTrue(
                "Survey displayed not recorded.",
                mTestSurveyComponentRule.isPromptShownForTriggerId("o2YBX3ZJc0tK1KeaPYj0UveLWhmf"));
    }

    // Tests that the survey is not shown for a type that is not enabled by the feature.
    @Test
    @MediumTest
    // Disable the feature as it's enabled in testing/variations/fieldtrial_testing_config.json.
    @Features.DisableFeatures(SigninFeatures.CHROME_ANDROID_IDENTITY_SURVEY_WEB)
    public void notShownForDifferentType() {
        Profile profile =
                ThreadUtils.runOnUiThreadBlocking(ProfileManager::getLastUsedRegularProfile);
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        SigninSurveyController.registerTrigger(
                                profile, SigninSurveyController.SigninSurveyType.WEB));
        mActivityTestRule.startOnNtp();
        Assert.assertThrows(
                CriteriaHelper.TimeoutException.class, this::waitForSurveyMessageToShow);
    }

    private void acceptSigninSurvey(
            @SigninSurveyController.SigninSurveyType int type, String triggerId) {
        acceptSigninSurvey(type, triggerId, /* expectedAccountCount= */ 0, SIGNED_OUT_STATUS);
    }

    private void acceptSigninSurvey(
            @SigninSurveyController.SigninSurveyType int type,
            String triggerId,
            int expectedAccountCount,
            String expectedSigninStatus) {
        showSigninSurvey(type);
        waitForSurveyMessageToShow();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    var _ = mSurveyMessage.get(MessageBannerProperties.ON_PRIMARY_ACTION).get();
                });

        Assert.assertEquals(
                "Last shown survey triggerId do not match.",
                triggerId,
                mTestSurveyComponentRule.getLastShownTriggerId());
        Assert.assertEquals(
                "Survey PSD does not match.",
                Map.of(
                        "Number of Google Accounts",
                        String.valueOf(expectedAccountCount),
                        "Sign-in Status",
                        expectedSigninStatus),
                mTestSurveyComponentRule.getLastShownSurveyPsd());
    }

    private void showSigninSurvey(@SigninSurveyController.SigninSurveyType int type) {
        HistogramWatcher watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord("Signin.HatsSurveyAndroid.TriggerRegistered", type)
                        .expectIntRecord("Signin.HatsSurveyAndroid.TriedShowing", type)
                        .build();
        Profile profile =
                ThreadUtils.runOnUiThreadBlocking(ProfileManager::getLastUsedRegularProfile);
        ThreadUtils.runOnUiThreadBlocking(
                () -> SigninSurveyController.registerTrigger(profile, type));
        mActivityTestRule.startOnNtp();
        watcher.assertExpected();
    }

    private void waitForSurveyMessageToShow() {
        Tab tab = mActivityTestRule.getActivityTab();
        CriteriaHelper.pollUiThread(() -> !tab.isLoading() && tab.isUserInteractable());
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mMessageDispatcher =
                            MessageDispatcherProvider.from(
                                    mActivityTestRule.getActivity().getWindowAndroid());
                });
        CriteriaHelper.pollUiThread(
                () -> {
                    mSurveyMessage = getSurveyMessage();
                    return mSurveyMessage != null;
                });
    }

    private @Nullable PropertyModel getSurveyMessage() {
        List<MessageStateHandler> messages =
                MessagesTestHelper.getEnqueuedMessages(
                        mMessageDispatcher, MessageIdentifier.SIGNIN_SURVEY);
        return messages.isEmpty() ? null : MessagesTestHelper.getCurrentMessage(messages.get(0));
    }
}

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin;

import androidx.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Features;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.hats.TestSurveyUtils;
import org.chromium.chrome.browser.ui.signin.SigninSurveyController;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.components.messages.DismissReason;
import org.chromium.components.messages.MessageBannerProperties;
import org.chromium.components.messages.MessageDispatcher;
import org.chromium.components.messages.MessageDispatcherProvider;
import org.chromium.components.messages.MessageIdentifier;
import org.chromium.components.messages.MessageStateHandler;
import org.chromium.components.messages.MessagesTestHelper;
import org.chromium.components.signin.SigninFeatures;
import org.chromium.content_public.browser.test.NativeLibraryTestUtils;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.List;

/** Instrumentation tests for {@link SigninSurveyController}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@DoNotBatch(reason = "Activity should be restarted")
public class SigninSurveyControllerTest {
    private static final String TRIGGER =
            ":" + TestSurveyUtils.TRIGGER_ID_PARAM_NAME + "/" + TestSurveyUtils.TEST_TRIGGER_ID_FOO;

    @Rule
    public FreshCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();

    @Rule
    public TestSurveyUtils.TestSurveyComponentRule mTestSurveyComponentRule =
            new TestSurveyUtils.TestSurveyComponentRule();

    private PropertyModel mSurveyMessage;
    private MessageDispatcher mMessageDispatcher;

    @Before
    public void setUp() {
        NativeLibraryTestUtils.loadNativeLibraryAndInitBrowserProcess();
        SigninSurveyController.enableWithoutDelayForTesting();
    }

    @Test
    @MediumTest
    @Features.EnableFeatures(SigninFeatures.CHROME_ANDROID_IDENTITY_SURVEY_FIRST_RUN + TRIGGER)
    public void acceptFreSigninSurvey() {
        acceptSigninSurvey(
                SigninSurveyController.SigninSurveyType.FRE, TestSurveyUtils.TEST_TRIGGER_ID_FOO);
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
        acceptSigninSurvey(
                SigninSurveyController.SigninSurveyType.NTP_ACCOUNT_AVATAR_TAP,
                "DujcsCGkZ0tK1KeaPYj0RGm9FgKX");
    }

    @Test
    @MediumTest
    public void acceptNtpPromoSigninSurvey() {
        acceptSigninSurvey(
                SigninSurveyController.SigninSurveyType.NTP_PROMO, "15CWgMniG0tK1KeaPYj0RkWoZ4B9");
    }

    @Test
    @MediumTest
    @Features.EnableFeatures(SigninFeatures.CHROME_ANDROID_IDENTITY_SURVEY_WEB + TRIGGER)
    public void acceptWebSigninSurvey() {
        acceptSigninSurvey(
                SigninSurveyController.SigninSurveyType.WEB, TestSurveyUtils.TEST_TRIGGER_ID_FOO);
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
        showSigninSurvey(type);
        waitForSurveyMessageToShow();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    var unused =
                            mSurveyMessage.get(MessageBannerProperties.ON_PRIMARY_ACTION).get();
                });

        Assert.assertEquals(
                "Last shown survey triggerId do not match.",
                triggerId,
                mTestSurveyComponentRule.getLastShownTriggerId());
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
        return messages.size() == 0 ? null : MessagesTestHelper.getCurrentMessage(messages.get(0));
    }
}

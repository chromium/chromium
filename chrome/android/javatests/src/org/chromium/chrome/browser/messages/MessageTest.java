// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.messages;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.FakeTimeTestRule;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.components.messages.MessageBannerProperties;
import org.chromium.components.messages.MessageDispatcher;
import org.chromium.components.messages.MessageDispatcherProvider;
import org.chromium.components.messages.MessagesTestHelper;
import org.chromium.components.messages.PrimaryActionClickBehavior;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.concurrent.TimeoutException;

/** Integration test of Message. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
public class MessageTest {
    @Rule
    public FreshCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();

    @Rule public FakeTimeTestRule mFakeTimeTestRule = new FakeTimeTestRule();

    private WebPageStation mPage;
    private ChromeTabbedActivity mActivity;
    private MessageDispatcher mMessageDispatcher;

    @Before
    public void setUp() {
        mPage = mActivityTestRule.startOnBlankPage();
        mActivity = mPage.getActivity();
        mMessageDispatcher =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> MessageDispatcherProvider.from(mActivity.getWindowAndroid()));
    }

    /**
     * Test that message is not clickable within tap protection period and becomes clickable once it
     * ends.
     */
    @Test
    @SmallTest
    public void testTapProtection() throws TimeoutException {
        MessagesTestHelper.enableTapProtectionDuration(500);
        CallbackHelper helper = new CallbackHelper();
        PropertyModel model =
                ThreadUtils.runOnUiThreadBlocking(
                        () ->
                                new PropertyModel.Builder(MessageBannerProperties.ALL_KEYS)
                                        .with(MessageBannerProperties.TITLE, "Test title")
                                        .with(MessageBannerProperties.PRIMARY_BUTTON_TEXT, "Action")
                                        .with(MessageBannerProperties.ON_DISMISSED, (v) -> {})
                                        .with(
                                                MessageBannerProperties.ON_PRIMARY_ACTION,
                                                () -> {
                                                    helper.notifyCalled();
                                                    return PrimaryActionClickBehavior
                                                            .DISMISS_IMMEDIATELY;
                                                })
                                        .build());
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mMessageDispatcher.enqueueWindowScopedMessage(model, true);
                });
        onView(withId(R.id.message_primary_button)).check(matches(isDisplayed()));
        onView(withId(R.id.message_primary_button)).perform(click());
        Assert.assertEquals("Not clickable with tap protection period", 0, helper.getCallCount());
        mFakeTimeTestRule.advanceMillis(1500);
        onView(withId(R.id.message_primary_button)).perform(click());
        helper.waitForNext("Should able to click when tap protection period ends");
    }
}

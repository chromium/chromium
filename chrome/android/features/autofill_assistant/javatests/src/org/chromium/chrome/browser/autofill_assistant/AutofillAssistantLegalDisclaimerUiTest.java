// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.allOf;
import static org.hamcrest.Matchers.is;
import static org.hamcrest.Matchers.not;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertThat;

import android.support.test.InstrumentationRegistry;
import android.view.View;

import androidx.annotation.Nullable;
import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.customtabs.CustomTabActivity;
import org.chromium.chrome.browser.customtabs.CustomTabActivityTestRule;
import org.chromium.chrome.browser.customtabs.CustomTabsIntentTestUtils;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.autofill_assistant.R;
import org.chromium.components.autofill_assistant.legal_disclaimer.AssistantLegalDisclaimerCoordinator;
import org.chromium.components.autofill_assistant.legal_disclaimer.AssistantLegalDisclaimerDelegate;
import org.chromium.components.autofill_assistant.legal_disclaimer.AssistantLegalDisclaimerModel;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

/**
 * Tests for the Autofill Assistant Legal Disclaimer.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class AutofillAssistantLegalDisclaimerUiTest {
    private AssistantLegalDisclaimerModel mModel;
    private AssistantLegalDisclaimerCoordinator mCoordinator;

    private static class TestViewHolder {
        final View mRootView;
        final View mMessageView;

        private TestViewHolder(View rootView) {
            mRootView = rootView;
            mMessageView = mRootView.findViewById(R.id.legal_disclaimer_text);
        }
    }

    @Rule
    public CustomTabActivityTestRule mCustomTabActivityTestRule = new CustomTabActivityTestRule();

    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule();

    static class MockDelegate implements AssistantLegalDisclaimerDelegate {
        @Nullable
        Integer mLinkClicked;

        @Override
        public void onLinkClicked(int link) {
            mLinkClicked = link;
        }
    }

    @Before
    public void setUp() throws Exception {
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(
                CustomTabsIntentTestUtils.createMinimalCustomTabIntent(
                        InstrumentationRegistry.getTargetContext(), "about:blank"));
        mModel = createLegalDisclaimerModel();
        mCoordinator = createLegalDisclaimerCoordinator(mModel);
    }

    @Test
    @MediumTest
    public void testInitialStateIsHidden() throws Exception {
        TestViewHolder viewHolder = attachToCoordinator();
        onView(is(viewHolder.mRootView)).check(matches(not(isDisplayed())));
    }

    @Test
    @MediumTest
    public void testClickLegalDisclaimerMessageLink() throws Exception {
        TestViewHolder viewHolder = attachToCoordinator();
        MockDelegate delegateMock = new MockDelegate();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mModel.setLegalDisclaimer(delegateMock, "<link3>Terms and Conditions</link3>");
        });
        assertNull(delegateMock.mLinkClicked);
        onView(is(viewHolder.mMessageView))
                .check(matches(allOf(isDisplayed(), withText("Terms and Conditions"))))
                .perform(click());
        assertThat(delegateMock.mLinkClicked, is(3));
    }

    @Test
    @MediumTest
    public void testHideLegalDisclaimer() throws Exception {
        TestViewHolder viewHolder = attachToCoordinator();
        MockDelegate delegateMock = new MockDelegate();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mModel.setLegalDisclaimer(delegateMock, "<link3>Terms and Conditions</link3>");
        });
        onView(is(viewHolder.mMessageView))
                .check(matches(allOf(isDisplayed(), withText("Terms and Conditions"))));

        TestThreadUtils.runOnUiThreadBlocking(() -> { mModel.setLegalDisclaimer(null, null); });
        onView(is(viewHolder.mRootView)).check(matches(not(isDisplayed())));
    }

    private CustomTabActivity getActivity() {
        return mCustomTabActivityTestRule.getActivity();
    }

    private AssistantLegalDisclaimerModel createLegalDisclaimerModel() {
        return TestThreadUtils.runOnUiThreadBlockingNoException(AssistantLegalDisclaimerModel::new);
    }

    private AssistantLegalDisclaimerCoordinator createLegalDisclaimerCoordinator(
            AssistantLegalDisclaimerModel model) throws Exception {
        AssistantLegalDisclaimerCoordinator coordinator = TestThreadUtils.runOnUiThreadBlocking(
                () -> new AssistantLegalDisclaimerCoordinator(getActivity(), model));
        return coordinator;
    }

    private TestViewHolder attachToCoordinator() throws Exception {
        return TestThreadUtils.runOnUiThreadBlocking(() -> {
            AutofillAssistantUiTestUtil.attachToCoordinator(getActivity(), mCoordinator.getView());
            return new TestViewHolder(mCoordinator.getView());
        });
    }
}

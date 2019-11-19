// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import static android.support.test.espresso.Espresso.onView;
import static android.support.test.espresso.action.ViewActions.click;
import static android.support.test.espresso.assertion.ViewAssertions.matches;
import static android.support.test.espresso.matcher.ViewMatchers.withId;
import static android.support.test.espresso.matcher.ViewMatchers.withText;

import static org.mockito.Mockito.verify;

import android.support.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;

import org.chromium.base.Callback;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class PasswordGenerationDialogTest {
    private PasswordGenerationDialogCoordinator mDialog;
    private String mGeneratedPassword = "generatedpassword";
    private String mExplanationString = "Explanation string.";

    @Mock
    private Callback<Boolean> mOnPasswordAcceptedOrRejectedCallback;

    @Rule
    public ChromeActivityTestRule<ChromeActivity> mActivityTestRule =
            new ChromeActivityTestRule<>(ChromeActivity.class);

    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Before
    public void setUp() throws InterruptedException {
        mActivityTestRule.startMainActivityOnBlankPage();
        mDialog = new PasswordGenerationDialogCoordinator(mActivityTestRule.getActivity());
        TestThreadUtils.runOnUiThreadBlocking(
                () -> mDialog.showDialog(mGeneratedPassword, mExplanationString,
                                mOnPasswordAcceptedOrRejectedCallback));
    }

    @Test
    @SmallTest
    public void testDialogSubviewsData() {
        onView(withId(R.id.generated_password)).check(matches(withText(mGeneratedPassword)));
        onView(withId(R.id.generation_save_explanation))
                .check(matches(withText(mExplanationString)));
    }

    @Test
    @SmallTest
    public void testAcceptedPasswordCallback() {
        onView(withId(R.id.positive_button)).perform(click());
        verify(mOnPasswordAcceptedOrRejectedCallback).onResult(true);
    }

    @Test
    @SmallTest
    public void testRejectedPasswordCallback() {
        onView(withId(R.id.negative_button)).perform(click());
        verify(mOnPasswordAcceptedOrRejectedCallback).onResult(false);
    }
}

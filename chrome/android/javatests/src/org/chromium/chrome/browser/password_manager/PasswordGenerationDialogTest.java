// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.mockito.Mockito.verify;

import androidx.test.filters.SmallTest;

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
import org.chromium.base.test.util.DisabledTest;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

/** Test for the password manager dialog. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class PasswordGenerationDialogTest {
    private PasswordGenerationDialogCoordinator mDialog;
    private String mGeneratedPassword = "generatedpassword";
    private String mExplanationString = "Explanation string.";

    @Mock
    private Callback<Boolean> mOnPasswordAcceptedOrRejectedCallback;

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Before
    public void setUp() throws InterruptedException {
        mActivityTestRule.startMainActivityOnBlankPage();
        mDialog = TestThreadUtils.runOnUiThreadBlockingNoException(() -> {
            PasswordGenerationDialogCoordinator dialog = new PasswordGenerationDialogCoordinator(
                    mActivityTestRule.getActivity().getWindowAndroid());
            dialog.showDialog(
                    mGeneratedPassword, mExplanationString, mOnPasswordAcceptedOrRejectedCallback);
            return dialog;
        });
    }

    @Test
    @SmallTest
    @DisabledTest(message = "https://crbug.com/1382439")
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

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.search_engines.choice_screen;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.RootMatchers.isDialog;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.junit.Assert.assertNotNull;

import android.view.View;

import androidx.test.filters.LargeTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features;
import org.chromium.chrome.browser.search_engines.R;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.components.search_engines.SearchEnginesFeatures;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.test.util.BlankUiTestActivity;

/** Render tests for {@link ChoiceDialogCoordinator} */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@Features.EnableFeatures(SearchEnginesFeatures.CLAY_BLOCKING)
public class ChoiceScreenRenderTest {
    public @Rule final ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setBugComponent(ChromeRenderTestRule.Component.UI_BROWSER_OMNIBOX)
                    .build();

    public @Rule final BaseActivityTestRule<BlankUiTestActivity> mActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    public @Rule final MockitoRule mMockitoRule =
            MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    private ModalDialogManager mDialogManager;
    private ChoiceDialogCoordinator mChoiceDialogCoordinator;

    @Before
    public void setUp() {
        mActivityTestRule.launchActivity(null);
        mDialogManager = mActivityTestRule.getActivity().getModalDialogManager();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mChoiceDialogCoordinator =
                            new ChoiceDialogCoordinator(
                                    mActivityTestRule.getActivity(), mDialogManager);
                });
    }

    @Test
    @LargeTest
    @Feature("RenderTest")
    public void testFirstChoiceScreenBlockingDialog() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(mChoiceDialogCoordinator::show);

        mRenderTestRule.render(getDialogView(), "first_choice_screen_blocking_dialog");
    }

    @Test
    @LargeTest
    public void testFirstChoiceScreenBlockingDialogButton() {
        ThreadUtils.runOnUiThreadBlocking(mChoiceDialogCoordinator::show);

        onView(withId(R.id.choice_dialog_button)).inRoot(isDialog()).perform(click());

        onView(withText(R.string.blocking_choice_dialog_second_title))
                .inRoot(isDialog())
                .check(matches(isDisplayed()));
    }

    @Test
    @LargeTest
    @Feature("RenderTest")
    @DisabledTest(message = "b/355054464: UI is not final yet.")
    public void testSecondChoiceScreenDialog() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(mChoiceDialogCoordinator::show);

        onView(withId(R.id.choice_dialog_button)).inRoot(isDialog()).perform(click());

        mRenderTestRule.render(getDialogView(), "second_choice_screen_dialog");
    }

    private View getDialogView() {
        return ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    PropertyModel dialogModel = mDialogManager.getCurrentDialogForTest();
                    assertNotNull(dialogModel);
                    return dialogModel.get(ModalDialogProperties.CUSTOM_VIEW);
                });
    }
}

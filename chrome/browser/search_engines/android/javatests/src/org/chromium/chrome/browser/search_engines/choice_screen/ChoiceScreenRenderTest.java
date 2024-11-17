// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.search_engines.choice_screen;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.RootMatchers.isDialog;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.isNotEnabled;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.chromium.ui.test.util.ViewUtils.onViewWaiting;

import android.view.View;

import androidx.test.filters.LargeTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;

import org.chromium.base.FeatureList;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.params.ParameterAnnotations;
import org.chromium.base.test.params.ParameterAnnotations.ClassParameter;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.search_engines.R;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.components.browser_ui.modaldialog.AppModalPresenter;
import org.chromium.components.search_engines.FakeSearchEngineCountryDelegate;
import org.chromium.components.search_engines.SearchEngineChoiceService;
import org.chromium.components.search_engines.test.util.SearchEnginesFeaturesTestUtil;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.test.util.BlankUiTestActivity;
import org.chromium.ui.test.util.NightModeTestUtils;
import org.chromium.ui.test.util.NightModeTestUtils.NightModeParams;

import java.util.List;
import java.util.Map;

/** Render tests for {@link ChoiceDialogCoordinator} */
@RunWith(ParameterizedRunner.class)
@ParameterAnnotations.UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@Batch(Batch.PER_CLASS)
public class ChoiceScreenRenderTest {
    public @ClassParameter static List<ParameterSet> params = new NightModeParams().getParameters();

    public @Rule final ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setBugComponent(ChromeRenderTestRule.Component.UI_BROWSER_OMNIBOX)
                    .build();

    public @Rule final BaseActivityTestRule<BlankUiTestActivity> mActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    public @Rule final MockitoRule mMockitoRule =
            MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    private @Mock ActivityLifecycleDispatcher mLifecycleDispatcher;

    private ModalDialogManager mDialogManager;
    private FakeSearchEngineCountryDelegate mFakeDelegate;

    public ChoiceScreenRenderTest(boolean nightModeEnabled) {
        // Sets a fake background color to make the screenshots easier to compare with bare eyes.
        NightModeTestUtils.setUpNightModeForBlankUiTestActivity(nightModeEnabled);
        mRenderTestRule.setNightModeEnabled(nightModeEnabled);
    }

    @Before
    public void setUp() {
        FeatureList.setDisableNativeForTesting(true);
        SearchEnginesFeaturesTestUtil.configureClayBlockingFeatureParams(
                Map.of(
                        "dialog_timeout_millis", "0",
                        // For the "pending" dialog mode to be enabled, this needs to be non-0.
                        "silent_pending_duration_millis", "1"));

        mActivityTestRule.launchActivity(null);
        mDialogManager = mActivityTestRule.getActivity().getModalDialogManager();
        mFakeDelegate =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            var delegate =
                                    new FakeSearchEngineCountryDelegate(/* enableLogging= */ false);
                            SearchEngineChoiceService.setInstanceForTests(
                                    new SearchEngineChoiceService(delegate));
                            return delegate;
                        });
    }

    @Test
    @LargeTest
    @Feature("RenderTest")
    public void testLoadingChoiceScreenBlockingDialog() throws Exception {
        // Make the delegate not emit a value, putting the UI in the "loading" state.
        ThreadUtils.runOnUiThreadBlocking(() -> mFakeDelegate.setIsDeviceChoiceRequired(null));

        showDialog();

        onViewWaiting(withText(R.string.next), true)
                .inRoot(isDialog())
                .check(matches(isNotEnabled()));
        mRenderTestRule.render(getDialogView(), "loading_choice_screen_blocking_dialog");
    }

    @Test
    @LargeTest
    @Feature("RenderTest")
    public void testFirstChoiceScreenBlockingDialog() throws Exception {
        showDialog();

        mRenderTestRule.render(getDialogView(), "first_choice_screen_blocking_dialog");
    }

    @Test
    @LargeTest
    @Feature("RenderTest")
    public void testSecondChoiceScreenBlockingDialog() throws Exception {
        showDialog();

        onView(withText(R.string.next)).inRoot(isDialog()).perform(click());

        onViewWaiting(withText(R.string.blocking_choice_dialog_second_title))
                .inRoot(isDialog())
                .check(matches(isDisplayed()));
        mRenderTestRule.render(getDialogView(), "second_choice_screen_dialog");
    }

    private View getDialogView() {
        return ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    AppModalPresenter presenter =
                            (AppModalPresenter) mDialogManager.getCurrentPresenterForTest();
                    return presenter.getDialogViewForTesting();
                });
    }

    private void showDialog() {
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        ChoiceDialogCoordinator.maybeShow(
                                mActivityTestRule.getActivity(),
                                mDialogManager,
                                mLifecycleDispatcher));
        onView(withId(R.id.choice_dialog_title)).inRoot(isDialog()).check(matches(isDisplayed()));
    }
}

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.page_info_sheet.feedback;

import static androidx.test.espresso.matcher.ViewMatchers.isEnabled;
import static androidx.test.espresso.matcher.ViewMatchers.withChild;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.CoreMatchers.not;
import static org.junit.Assert.assertNotNull;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.StringRes;
import androidx.test.espresso.matcher.ViewMatchers;
import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.share.page_info_sheet.feedback.FeedbackSheetCoordinator.FeedbackOption;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.widget.RadioButtonLayout;
import org.chromium.ui.base.TestActivity;

import java.util.List;

/** Unit tests for FeedbackSheetCoordinator. */
@RunWith(BaseRobolectricTestRunner.class)
public class FeedbackSheetCoordinatorUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Mock private BottomSheetController mBottomSheetController;
    @Mock private FeedbackSheetCoordinator.Delegate mFeedbackDelegate;

    @Captor private ArgumentCaptor<FeedbackSheetContent> mSheetContentCaptor;

    @Before
    public void setUp() throws Exception {
        // Set delegate to return some radio button options.
        when(mFeedbackDelegate.getAvailableOptions())
                .thenReturn(
                        List.of(
                                new FeedbackOption("foo_item", R.string.test_radio_button_item_1),
                                new FeedbackOption("bar_item", R.string.test_radio_button_item_2),
                                new FeedbackOption("baz_item", R.string.test_radio_button_item_3)));
    }

    @Test
    public void testInitialState() {
        var activityScenario = mActivityScenarioRule.getScenario();
        activityScenario.onActivity(
                activity -> {
                    // Initialize coordinator and show UI.
                    FeedbackSheetCoordinator coordinator =
                            new FeedbackSheetCoordinator(
                                    activity, mFeedbackDelegate, mBottomSheetController);
                    coordinator.requestShowContent();

                    // Ensure bottom sheet was opened.
                    verify(mBottomSheetController)
                            .requestShowContent(mSheetContentCaptor.capture(), eq(true));

                    // Get UI elements from bottom sheet.
                    View acceptButton = getView(R.id.accept_button);
                    View cancelButton = getView(R.id.cancel_button);
                    View radioButtons = getView(R.id.radio_buttons);

                    // Initially the accept button should be disabled.
                    ViewMatchers.assertThat(acceptButton, not(isEnabled()));
                    ViewMatchers.assertThat(cancelButton, isEnabled());

                    // Radio button should contain the options from the delegate.
                    ViewMatchers.assertThat(
                            radioButtons, withChild(withText(R.string.test_radio_button_item_1)));
                    ViewMatchers.assertThat(
                            radioButtons, withChild(withText(R.string.test_radio_button_item_2)));
                    ViewMatchers.assertThat(
                            radioButtons, withChild(withText(R.string.test_radio_button_item_3)));
                });
    }

    @Test
    public void testSelectAndAccept() {
        var activityScenario = mActivityScenarioRule.getScenario();
        activityScenario.onActivity(
                activity -> {
                    // Initialize coordinator and show UI.
                    FeedbackSheetCoordinator coordinator =
                            new FeedbackSheetCoordinator(
                                    activity, mFeedbackDelegate, mBottomSheetController);
                    coordinator.requestShowContent();

                    // Ensure bottom sheet was opened.
                    verify(mBottomSheetController)
                            .requestShowContent(mSheetContentCaptor.capture(), eq(true));

                    View acceptButton = getView(R.id.accept_button);
                    RadioButtonLayout radioButtons =
                            (RadioButtonLayout) getView(R.id.radio_buttons);

                    getChildViewWithText(radioButtons, R.string.test_radio_button_item_2)
                            .performClick();
                    acceptButton.performClick();

                    verify(mFeedbackDelegate).onAccepted("bar_item");
                });
    }

    @Test
    public void testCancel() {
        var activityScenario = mActivityScenarioRule.getScenario();
        activityScenario.onActivity(
                activity -> {
                    // Initialize coordinator and show UI.
                    FeedbackSheetCoordinator coordinator =
                            new FeedbackSheetCoordinator(
                                    activity, mFeedbackDelegate, mBottomSheetController);
                    coordinator.requestShowContent();

                    // Ensure bottom sheet was opened.
                    verify(mBottomSheetController)
                            .requestShowContent(mSheetContentCaptor.capture(), eq(true));

                    View cancelButton = getView(R.id.cancel_button);

                    cancelButton.performClick();

                    verify(mFeedbackDelegate).onCanceled();
                });
    }

    private View getView(int viewId) {
        View view = mSheetContentCaptor.getValue().getContentView();
        assertNotNull(view);

        return view.findViewById(viewId);
    }

    private View getChildViewWithText(ViewGroup viewGroup, @StringRes int textId) {
        ViewMatchers.assertThat(viewGroup, withChild(withText(textId)));

        for (int i = 0; i < viewGroup.getChildCount(); i++) {
            View childView = viewGroup.getChildAt(i);
            if (withText(textId).matches(childView)) {
                return childView;
            }
        }

        return null;
    }
}

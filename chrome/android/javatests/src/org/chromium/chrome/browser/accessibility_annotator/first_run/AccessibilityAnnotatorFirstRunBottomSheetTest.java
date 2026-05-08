// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.accessibility_annotator.first_run;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.intent.Intents.intending;
import static androidx.test.espresso.intent.matcher.IntentMatchers.anyIntent;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.app.Instrumentation.ActivityResult;
import android.content.Intent;
import android.text.Spanned;
import android.text.style.ClickableSpan;
import android.view.View;
import android.widget.TextView;

import androidx.test.espresso.UiController;
import androidx.test.espresso.ViewAction;
import androidx.test.espresso.intent.Intents;
import androidx.test.filters.SmallTest;

import org.hamcrest.Matcher;
import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.chrome.R;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.AutoResetCtaTransitTestRule;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetTestSupport;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Tests for {@link AccessibilityAnnotatorFirstRunBottomSheet}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
public class AccessibilityAnnotatorFirstRunBottomSheetTest {
    @Rule
    public AutoResetCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.autoResetCtaActivityRule();

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private AccessibilityAnnotatorFirstRunBottomSheetCoordinator.Delegate mDelegate;
    private static final String TEST_TITLE = "Test Title";
    private static final String TEST_DESCRIPTION = "Test Description";
    private static final String TEST_CARD_1_TEXT = "Card 1";
    private static final String TEST_CARD_2_TEXT = "Card 2";
    private static final String TEST_PRIMARY_BUTTON_LABEL = "Agree";
    private static final String TEST_SECONDARY_BUTTON_LABEL = "Settings";
    private static final String MANAGE_SETTINGS_URL = "https://example.com/manage";
    private static final String LEARN_MORE_URL = "https://example.com/learn_more";

    private BottomSheetController mBottomSheetController;
    private AccessibilityAnnotatorFirstRunBottomSheetCoordinator mCoordinator;

    @Before
    public void setUp() {
        Intents.init();
        intending(anyIntent()).respondWith(new ActivityResult(Activity.RESULT_OK, new Intent()));
        mActivityTestRule.startOnBlankPage();
        mBottomSheetController =
                mActivityTestRule
                        .getActivity()
                        .getRootUiCoordinatorForTesting()
                        .getBottomSheetController();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mCoordinator =
                            new AccessibilityAnnotatorFirstRunBottomSheetCoordinator(
                                    mActivityTestRule.getActivity(),
                                    mBottomSheetController,
                                    mDelegate);
                });
    }

    @After
    public void tearDown() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    if (mCoordinator != null) {
                        mCoordinator.hide(BottomSheetController.StateChangeReason.NONE);
                    }
                });
        Intents.release();
    }

    @Test
    @SmallTest
    public void testShowAndClickAcknowledge() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mCoordinator.requestShowContent(MANAGE_SETTINGS_URL, LEARN_MORE_URL);
                });

        BottomSheetTestSupport.waitForOpen(mBottomSheetController);

        onView(withText(R.string.accessibility_annotator_info_primary_button)).perform(click());

        verify(mDelegate).onInfoAcknowledged();
    }

    @Test
    @SmallTest
    public void testShowAndClickLearnMore() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mCoordinator.requestShowContent(MANAGE_SETTINGS_URL, LEARN_MORE_URL);
                });

        BottomSheetTestSupport.waitForOpen(mBottomSheetController);

        onView(withId(R.id.accessibility_annotator_learn_more_description))
                .perform(
                        new ViewAction() {
                            @Override
                            public Matcher<View> getConstraints() {
                                return androidx.test.espresso.matcher.ViewMatchers.isAssignableFrom(
                                        TextView.class);
                            }

                            @Override
                            public String getDescription() {
                                return "click clickable span";
                            }

                            @Override
                            public void perform(UiController uiController, View view) {
                                TextView textView = (TextView) view;
                                Spanned spanned = (Spanned) textView.getText();
                                ClickableSpan[] spans =
                                        spanned.getSpans(0, spanned.length(), ClickableSpan.class);
                                if (spans.length > 0) {
                                    spans[0].onClick(textView);
                                }
                            }
                        });

        verify(mDelegate).onLearnMoreClicked();
    }

    @Test
    @SmallTest
    public void testShowAndClickManageSettings() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mCoordinator.requestShowContent(MANAGE_SETTINGS_URL, LEARN_MORE_URL);
                });

        BottomSheetTestSupport.waitForOpen(mBottomSheetController);

        onView(withText(R.string.accessibility_annotator_info_secondary_button)).perform(click());

        verify(mDelegate).onManageSettingsClicked();
    }

    @Test
    @SmallTest
    public void testViewBinding() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    PropertyModel model =
                            new PropertyModel.Builder(
                                            AccessibilityAnnotatorFirstRunBottomSheetProperties
                                                    .ALL_KEYS)
                                    .with(
                                            AccessibilityAnnotatorFirstRunBottomSheetProperties
                                                    .TITLE,
                                            TEST_TITLE)
                                    .with(
                                            AccessibilityAnnotatorFirstRunBottomSheetProperties
                                                    .DESCRIPTION,
                                            TEST_DESCRIPTION)
                                    .with(
                                            AccessibilityAnnotatorFirstRunBottomSheetProperties
                                                    .CARD_1_TEXT,
                                            TEST_CARD_1_TEXT)
                                    .with(
                                            AccessibilityAnnotatorFirstRunBottomSheetProperties
                                                    .CARD_2_TEXT,
                                            TEST_CARD_2_TEXT)
                                    .with(
                                            AccessibilityAnnotatorFirstRunBottomSheetProperties
                                                    .PRIMARY_BUTTON_LABEL,
                                            TEST_PRIMARY_BUTTON_LABEL)
                                    .with(
                                            AccessibilityAnnotatorFirstRunBottomSheetProperties
                                                    .SECONDARY_BUTTON_LABEL,
                                            TEST_SECONDARY_BUTTON_LABEL)
                                    .build();

                    AccessibilityAnnotatorFirstRunBottomSheetViewHolder holder =
                            new AccessibilityAnnotatorFirstRunBottomSheetViewHolder(
                                    mActivityTestRule.getActivity());

                    PropertyModelChangeProcessor.create(
                            model,
                            holder,
                            AccessibilityAnnotatorFirstRunBottomSheetViewBinder::bind);

                    assertTextViewContent(holder.mTitle, TEST_TITLE);
                    assertTextViewContent(holder.mDescription, TEST_DESCRIPTION);
                    assertTextViewContent(holder.mCard1Text, TEST_CARD_1_TEXT);
                    assertTextViewContent(holder.mCard2Text, TEST_CARD_2_TEXT);
                    assertTextViewContent(holder.mPrimaryButton, TEST_PRIMARY_BUTTON_LABEL);
                    assertTextViewContent(holder.mSecondaryButton, TEST_SECONDARY_BUTTON_LABEL);
                });
    }

    private void assertTextViewContent(TextView textView, String expectedText) {
        Assert.assertEquals(expectedText, textView.getText().toString());
        Assert.assertEquals(View.VISIBLE, textView.getVisibility());
    }
}

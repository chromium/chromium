// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.autofill_ai;

import static org.hamcrest.MatcherAssert.assertThat;
import static org.hamcrest.Matchers.is;
import static org.hamcrest.Matchers.not;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoInteractions;

import android.graphics.Paint;
import android.text.SpannableString;
import android.text.style.ClickableSpan;
import android.view.Gravity;
import android.view.View;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.jni_zero.JniRawPtr;
import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.RuntimeEnvironment;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.components.autofill.autofill_ai.AutofillAiWalletNoticeFunnelEvents;
import org.chromium.components.autofill.autofill_ai.EntityAttributeUpdateType;
import org.chromium.components.autofill.payments.LegalMessageLine;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.test.util.modaldialog.FakeModalDialogManager;

import java.util.Collections;
import java.util.List;

/** Unit tests for {@link AutofillAiSaveUpdateEntityPrompt}. */
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures(ChromeFeatureList.AUTOFILL_AI_WITH_DATA_SCHEMA)
public class AutofillAiSaveUpdateEntityPromptTest {
    private static final long NATIVE_AUTOFILL_AI_SAVE_UPDATE_ENTITY_PROMPT_CONTROLLER = 100L;
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Mock private AutofillAiSaveUpdateEntityPromptController.Natives mPromptControllerJni;

    private TestActivity mActivity;
    private JniRawPtr<
                    AutofillAiSaveUpdateEntityPromptController
                            .NativeAutofillAiSaveUpdateEntityPromptController>
            mNativeController;
    private AutofillAiSaveUpdateEntityPromptController mPromptController;
    private AutofillAiSaveUpdateEntityPrompt mPrompt;
    private FakeModalDialogManager mModalDialogManager;

    @Before
    public void setUp() {
        mActivityScenarioRule.getScenario().onActivity(activity -> mActivity = activity);
        AutofillAiSaveUpdateEntityPromptControllerJni.setInstanceForTesting(mPromptControllerJni);
        mNativeController =
                JniRawPtr.createForTesting(NATIVE_AUTOFILL_AI_SAVE_UPDATE_ENTITY_PROMPT_CONTROLLER);
        mPromptController = AutofillAiSaveUpdateEntityPromptController.create(mNativeController);
        mModalDialogManager = new FakeModalDialogManager(ModalDialogType.APP);
        mPrompt =
                new AutofillAiSaveUpdateEntityPrompt(
                        mPromptController, mModalDialogManager, mActivity);
    }

    @After
    public void tearDown() {
        mPromptController.onNativeDestroyed();
    }

    @Test
    public void onNativeDestroyed_dropsSubsequentCalls() {
        mPromptController.onNativeDestroyed();

        mPromptController.onUserAccepted();
        mPromptController.onUserDeclined();
        mPromptController.onPromptDismissed();
        mPromptController.onWalletLinkClicked();

        verifyNoInteractions(mPromptControllerJni);
    }

    @Test
    public void userAccepted() {
        mPrompt.show();
        assertNotNull(mModalDialogManager.getShownDialogModel());

        mModalDialogManager.clickPositiveButton();
        assertNull(mModalDialogManager.getShownDialogModel());
        verify(mPromptControllerJni).onUserAccepted(eq(mNativeController));
        verify(mPromptControllerJni).onPromptDismissed(eq(mNativeController));
    }

    @Test
    public void userDeclined() {
        mPrompt.show();
        assertNotNull(mModalDialogManager.getShownDialogModel());

        mModalDialogManager.clickNegativeButton();
        assertNull(mModalDialogManager.getShownDialogModel());
        verify(mPromptControllerJni).onUserDeclined(eq(mNativeController));
        verify(mPromptControllerJni).onPromptDismissed(eq(mNativeController));
    }

    @Test
    public void promptDismissed() {
        mPrompt.show();
        assertNotNull(mModalDialogManager.getShownDialogModel());

        mPrompt.dismiss();
        assertNull(mModalDialogManager.getShownDialogModel());
        verify(mPromptControllerJni).onPromptDismissed(eq(mNativeController));
    }

    @Test
    public void showAfterDismiss() {
        mPrompt.show();
        assertNotNull(mModalDialogManager.getShownDialogModel());

        mPrompt.dismiss();
        assertNull(mModalDialogManager.getShownDialogModel());
        verify(mPromptControllerJni, times(1)).onPromptDismissed(eq(mNativeController));
        // No dialog should be shown after dismiss.
        mPrompt.show();
        assertNull(mModalDialogManager.getShownDialogModel());
    }

    @Test
    @DisableFeatures(ChromeFeatureList.AUTOFILL_AI_WALLET_PASS_BRANDING_2026)
    public void dialogStrings() {
        mPrompt.setDialogDetails(
                "title",
                "positive button text",
                "negative button text",
                /* isWalletableEntity= */ true);
        mPrompt.show();
        PropertyModel propertyModel = mModalDialogManager.getShownDialogModel();

        assertEquals("title", propertyModel.get(ModalDialogProperties.TITLE));
        assertEquals(
                "positive button text",
                propertyModel.get(ModalDialogProperties.POSITIVE_BUTTON_TEXT));
        assertEquals(
                "negative button text",
                propertyModel.get(ModalDialogProperties.NEGATIVE_BUTTON_TEXT));
        assertNotNull(propertyModel.get(ModalDialogProperties.TITLE_END_ICON));
        assertEquals(
                Gravity.NO_GRAVITY,
                propertyModel.get(ModalDialogProperties.TITLE_END_ICON_GRAVITY));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.AUTOFILL_AI_WALLET_PASS_BRANDING_2026)
    public void dialogStrings_branding2026Enabled() {
        mPrompt.setDialogDetails(
                "title",
                "positive button text",
                "negative button text",
                /* isWalletableEntity= */ true);
        mPrompt.show();
        PropertyModel propertyModel = mModalDialogManager.getShownDialogModel();

        assertNotNull(propertyModel.get(ModalDialogProperties.TITLE_END_ICON));
        assertEquals(
                Gravity.TOP,
                propertyModel.get(ModalDialogProperties.TITLE_END_ICON_GRAVITY));
    }

    @Test
    public void localSourceNotice() {
        mPrompt.setSourceNotice("Entity will be saved locally", /* insertWalletLink= */ false);
        mPrompt.show();

        View dialogView = mPrompt.getDialogViewForTesting();
        TextView sourceNoticeView = dialogView.findViewById(R.id.autofill_ai_entity_source_notice);
        assertEquals("Entity will be saved locally", sourceNoticeView.getText());
    }

    @Test
    public void emptyWalletNotice() {
        mPrompt.setSourceNotice("", /* insertWalletLink= */ true);
        mPrompt.show();

        View dialogView = mPrompt.getDialogViewForTesting();
        TextView sourceNoticeView = dialogView.findViewById(R.id.autofill_ai_entity_source_notice);
        assertEquals(View.GONE, sourceNoticeView.getVisibility());
    }

    @Test
    public void walletNotice() {
        String walletTitle =
                RuntimeEnvironment.getApplication()
                        .getString(R.string.autofill_google_wallet_title);
        String email = "alexpark@gmail.com";
        String sourceNotice =
                RuntimeEnvironment.getApplication()
                        .getString(R.string.autofill_ai_save_entity_in_wallet_source_notice)
                        .replace("$1", walletTitle)
                        .replace("$2", walletTitle)
                        .replace("$3", email);

        mPrompt.setSourceNotice(sourceNotice, /* insertWalletLink= */ true);
        mPrompt.show();

        View dialogView = mPrompt.getDialogViewForTesting();
        TextView sourceNoticeView = dialogView.findViewById(R.id.autofill_ai_entity_source_notice);
        assertEquals(View.VISIBLE, sourceNoticeView.getVisibility());
        assertEquals(
                sourceNotice.replace("<link>", "").replace("</link>", ""),
                sourceNoticeView.getText().toString());

        SpannableString spannableString = (SpannableString) sourceNoticeView.getText();
        ClickableSpan[] spans =
                spannableString.getSpans(0, spannableString.length(), ClickableSpan.class);
        assertThat(spans.length, is(1));
        spans[0].onClick(sourceNoticeView);
        verify(mPromptControllerJni).onWalletLinkClicked(eq(mNativeController));
    }

    @Test
    public void publicPassesNotice_default_viewIsGone() {
        mPrompt.show();

        View dialogView = mPrompt.getDialogViewForTesting();
        TextView publicPassesNoticeView =
                dialogView.findViewById(R.id.autofill_ai_public_passes_notice);
        assertEquals(View.GONE, publicPassesNoticeView.getVisibility());
    }

    @Test
    public void publicPassNotice_empty_viewIsGone() {
        mPrompt.setPublicPassesNotice(Collections.emptyList());
        mPrompt.show();

        View dialogView = mPrompt.getDialogViewForTesting();
        TextView publicPassNoticeView =
                dialogView.findViewById(R.id.autofill_ai_public_passes_notice);
        assertEquals(View.GONE, publicPassNoticeView.getVisibility());
    }

    @Test
    public void publicPassesNotice_nonEmpty_viewIsVisible() {
        LegalMessageLine line =
                new LegalMessageLine(
                        "Legal message with a link.",
                        List.of(new LegalMessageLine.Link(22, 26, "https://example.com")));
        mPrompt.setPublicPassesNotice(List.of(line));
        mPrompt.show();

        View dialogView = mPrompt.getDialogViewForTesting();
        TextView publicPassesNoticeView =
                dialogView.findViewById(R.id.autofill_ai_public_passes_notice);
        assertEquals(View.VISIBLE, publicPassesNoticeView.getVisibility());
        assertEquals("Legal message with a link.", publicPassesNoticeView.getText().toString());

        SpannableString spannableString = (SpannableString) publicPassesNoticeView.getText();
        ClickableSpan[] spans =
                spannableString.getSpans(0, spannableString.length(), ClickableSpan.class);
        assertThat(spans.length, is(1));
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        AutofillAiSaveUpdateEntityPrompt.WALLET_NOTICE_SAVE_FUNNEL_HISTOGRAM,
                        AutofillAiWalletNoticeFunnelEvents.LINK_CLICKED);
        spans[0].onClick(publicPassesNoticeView);
        histogramWatcher.assertExpected();
    }

    @Test
    public void noEntityAttributeUpdateDetails() {
        mPrompt.setEntityUpdateDetails(Collections.emptyList(), /* isUpdatePrompt= */ false);
        mPrompt.show();

        View dialogView = mPrompt.getDialogViewForTesting();
        LinearLayout attributeList = dialogView.findViewById(R.id.autofill_ai_attribute_infos);
        assertEquals(0, attributeList.getChildCount());
    }

    @Test
    public void entityAttributeUpdateDetailsInSavePrompt() {
        final EntityAttributeUpdateDetails passportNumber =
                new EntityAttributeUpdateDetails(
                        /* attributeName= */ "Passport number",
                        /* attributeValue= */ "AA1111",
                        /* oldAttributeValue= */ "",
                        /* updateType= */ EntityAttributeUpdateType.NEW_ENTITY_ATTRIBUTE_ADDED);
        final EntityAttributeUpdateDetails passportName =
                new EntityAttributeUpdateDetails(
                        /* attributeName= */ "Passport name",
                        /* attributeValue= */ "John Doe",
                        /* oldAttributeValue= */ "",
                        /* updateType= */ EntityAttributeUpdateType.NEW_ENTITY_ATTRIBUTE_ADDED);
        final EntityAttributeUpdateDetails passportExpirationDate =
                new EntityAttributeUpdateDetails(
                        /* attributeName= */ "Passport expiration date",
                        /* attributeValue= */ "12/12/2030",
                        /* oldAttributeValue= */ "",
                        /* updateType= */ EntityAttributeUpdateType.NEW_ENTITY_ATTRIBUTE_ADDED);
        List<EntityAttributeUpdateDetails> updateDetailsList =
                List.of(passportNumber, passportName, passportExpirationDate);

        mPrompt.setEntityUpdateDetails(updateDetailsList, /* isUpdatePrompt= */ false);
        mPrompt.show();

        View dialogView = mPrompt.getDialogViewForTesting();
        LinearLayout attributeList = dialogView.findViewById(R.id.autofill_ai_attribute_infos);
        assertEquals(3, attributeList.getChildCount());

        // Make sure that "New" badge is not added to the attribute value.
        assertAttributeNameAndValue(
                /* attributeInfo= */ attributeList.getChildAt(0),
                /* attributeName= */ "Passport number",
                /* attributeNameAxLabel= */ null,
                /* attributeValue= */ "AA1111",
                /* oldAttributeValue= */ "");
        assertAttributeNameAndValue(
                /* attributeInfo= */ attributeList.getChildAt(1),
                /* attributeName= */ "Passport name",
                /* attributeNameAxLabel= */ null,
                /* attributeValue= */ "John Doe",
                /* oldAttributeValue= */ "");
        assertAttributeNameAndValue(
                /* attributeInfo= */ attributeList.getChildAt(2),
                /* attributeName= */ "Passport expiration date",
                /* attributeNameAxLabel= */ null,
                /* attributeValue= */ "12/12/2030",
                /* oldAttributeValue= */ "");
    }

    @Test
    public void entityAttributeUpdateDetailsInUpdatePrompt() {
        final EntityAttributeUpdateDetails passportNumber =
                new EntityAttributeUpdateDetails(
                        /* attributeName= */ "Passport number",
                        /* attributeValue= */ "AA1111",
                        /* oldAttributeValue= */ "",
                        /* updateType= */ EntityAttributeUpdateType.NEW_ENTITY_ATTRIBUTE_ADDED);
        final EntityAttributeUpdateDetails passportName =
                new EntityAttributeUpdateDetails(
                        /* attributeName= */ "Passport name",
                        /* attributeValue= */ "John Doe",
                        /* oldAttributeValue= */ "Seb Doe",
                        /* updateType= */ EntityAttributeUpdateType.NEW_ENTITY_ATTRIBUTE_UPDATED);
        final EntityAttributeUpdateDetails passportExpirationDate =
                new EntityAttributeUpdateDetails(
                        /* attributeName= */ "Passport expiration date",
                        /* attributeValue= */ "12/12/2030",
                        /* oldAttributeValue= */ "",
                        /* updateType= */ EntityAttributeUpdateType.NEW_ENTITY_ATTRIBUTE_UNCHANGED);
        List<EntityAttributeUpdateDetails> updateDetailsList =
                List.of(passportNumber, passportName, passportExpirationDate);

        mPrompt.setEntityUpdateDetails(updateDetailsList, /* isUpdatePrompt= */ true);
        mPrompt.show();

        View dialogView = mPrompt.getDialogViewForTesting();
        LinearLayout attributeList = dialogView.findViewById(R.id.autofill_ai_attribute_infos);
        assertEquals(3, attributeList.getChildCount());

        // Make sure the "New" badge is added to the added attributes.
        assertAttributeNameAndValue(
                /* attributeInfo= */ attributeList.getChildAt(0),
                /* attributeName= */ "Passport number",
                /* attributeNameAxLabel= */ "Passport number, new",
                /* attributeValue= */ "AA1111  New",
                /* oldAttributeValue= */ "");
        assertAttributeNameAndValue(
                /* attributeInfo= */ attributeList.getChildAt(1),
                /* attributeName= */ "Passport name",
                /* attributeNameAxLabel= */ "Passport name, was Seb Doe",
                /* attributeValue= */ "John Doe",
                /* oldAttributeValue= */ "Seb Doe");
        assertAttributeNameAndValue(
                /* attributeInfo= */ attributeList.getChildAt(2),
                /* attributeName= */ "Passport expiration date",
                /* attributeNameAxLabel= */ null,
                /* attributeValue= */ "12/12/2030",
                /* oldAttributeValue= */ "");
    }

    private void assertAttributeNameAndValue(
            View attributeInfo,
            String attributeName,
            String attributeNameAxLabel,
            String attributeValue,
            String oldAttributeValue) {
        TextView nameTextView = attributeInfo.findViewById(R.id.attribute_name);
        assertEquals(attributeName, nameTextView.getText());
        assertThat(nameTextView.getPaintFlags() & Paint.STRIKE_THRU_TEXT_FLAG, is(0));
        assertEquals(attributeNameAxLabel, nameTextView.getContentDescription());
        TextView valueTextView = attributeInfo.findViewById(R.id.attribute_value);
        assertEquals(attributeValue, valueTextView.getText().toString());
        assertThat(valueTextView.getPaintFlags() & Paint.STRIKE_THRU_TEXT_FLAG, is(0));
        TextView oldValueTextView = attributeInfo.findViewById(R.id.old_attribute_value);
        if (oldAttributeValue.isEmpty()) {
            assertEquals(View.GONE, oldValueTextView.getVisibility());
        } else {
            assertEquals(View.VISIBLE, oldValueTextView.getVisibility());
            assertEquals(oldAttributeValue, oldValueTextView.getText());
            assertThat(oldValueTextView.getPaintFlags() & Paint.STRIKE_THRU_TEXT_FLAG, not(0));
        }
    }
}

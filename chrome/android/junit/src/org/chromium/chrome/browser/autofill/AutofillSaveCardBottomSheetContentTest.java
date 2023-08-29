// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill;

import static org.hamcrest.MatcherAssert.assertThat;
import static org.hamcrest.Matchers.notNullValue;
import static org.hamcrest.Matchers.nullValue;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.verify;

import android.content.Context;
import android.text.Spannable;
import android.text.style.ClickableSpan;
import android.view.View;
import android.widget.Button;
import android.widget.ImageView;
import android.widget.ScrollView;
import android.widget.TextView;

import androidx.annotation.DrawableRes;
import androidx.annotation.IdRes;

import com.google.common.collect.ImmutableList;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.RuntimeEnvironment;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.autofill.AutofillSaveCardBottomSheetContent.Delegate;
import org.chromium.components.autofill.payments.AutofillSaveCardUiInfo;
import org.chromium.components.autofill.payments.CardDetail;
import org.chromium.components.autofill.payments.LegalMessageLine;
import org.chromium.components.autofill.payments.LegalMessageLine.Link;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent.ContentPriority;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent.HeightMode;

import java.util.Arrays;
import java.util.Collections;
import java.util.List;

/**
 * Unit tests for {@link AutofillSaveCardBottomSheetContent}
 */
@RunWith(BaseRobolectricTestRunner.class)
public class AutofillSaveCardBottomSheetContentTest {
    @DrawableRes
    private static final int EXAMPLE_DRAWABLE_RES = R.drawable.arrow_up;

    private static final String HTTPS_EXAMPLE_COM = "https://example.com";

    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule();

    private Context mContext;

    private AutofillSaveCardBottomSheetContent mContent;

    @Mock
    private Delegate mDelegate;

    @Before
    public void setUp() {
        mContext = RuntimeEnvironment.getApplication().getApplicationContext();
        mContent = new AutofillSaveCardBottomSheetContent(mContext, mDelegate);
    }

    @Test
    public void testConfirmButtonClick_callsDelegateDidClickConfirm() {
        Button button =
                mContent.getContentView().findViewById(R.id.autofill_save_card_confirm_button);

        button.callOnClick();

        verify(mDelegate).didClickConfirm();
    }

    @Test
    public void testCancelButtonClick_callsDelegateDidClickCancel() {
        Button button =
                mContent.getContentView().findViewById(R.id.autofill_save_card_cancel_button);

        button.callOnClick();

        verify(mDelegate).didClickCancel();
    }

    @Test
    public void testSetUiInfo_setsAllViews() {
        AutofillSaveCardUiInfo uiInfo = new AutofillSaveCardUiInfo.Builder()
                                                .withLogoIcon(0)
                                                .withTitleText("Title Text")
                                                .withDescriptionText("Description Text")
                                                .withCardDetail(new CardDetail(EXAMPLE_DRAWABLE_RES,
                                                        "CardLabel Text", "CardSubLabel Text"))
                                                .withConfirmText("Confirm Text")
                                                .withCancelText("Cancel Text")
                                                .build();

        mContent.setUiInfo(uiInfo);
        View contentView = mContent.getContentView();

        ImageView logoImageView = contentView.findViewById(R.id.autofill_save_card_icon);
        assertEquals(View.GONE, logoImageView.getVisibility());
        assertEquals("Title Text", getTextViewText(R.id.autofill_save_card_title_text));
        assertEquals("Description Text", getTextViewText(R.id.autofill_save_card_description_text));
        ImageView issuerImageView =
                contentView.findViewById(R.id.autofill_save_card_credit_card_icon);
        assertThat(issuerImageView.getDrawable(), notNullValue());
        assertEquals("CardLabel Text", getTextViewText(R.id.autofill_save_card_credit_card_label));
        assertEquals(
                "CardSubLabel Text", getTextViewText(R.id.autofill_save_card_credit_card_sublabel));
        Button confirmButton = contentView.findViewById(R.id.autofill_save_card_confirm_button);
        assertEquals("Confirm Text", confirmButton.getText());
        Button cancelButton = contentView.findViewById(R.id.autofill_save_card_cancel_button);
        assertEquals("Cancel Text", cancelButton.getText());
    }

    private CharSequence getTextViewText(@IdRes int resourceId) {
        return mContent.getContentView().<TextView>findViewById(resourceId).getText();
    }

    @Test
    public void testSetLogoIconId_visiblySetsTheImage() {
        mContent.setUiInfo(defaultUiInfoBuilder().withLogoIcon(EXAMPLE_DRAWABLE_RES).build());

        ImageView imageView = mContent.getContentView().findViewById(R.id.autofill_save_card_icon);
        assertThat(imageView.getDrawable(), notNullValue());
        assertEquals(View.VISIBLE, imageView.getVisibility());
    }

    @Test
    public void testSetLegalMessage_setsUpSpannableText() {
        AutofillSaveCardUiInfo uiInfo =
                defaultUiInfoBuilder()
                        .withLegalMessageLines(ImmutableList.of(new LegalMessageLine(
                                "abc", Arrays.asList(new Link(0, 2, HTTPS_EXAMPLE_COM)))))
                        .build();

        mContent.setUiInfo(uiInfo);

        TextView view = mContent.getContentView().findViewById(R.id.legal_message);
        List<ClickableSpan> spans = getClickableSpans((Spannable) view.getText());
        assertEquals(1, spans.size());
        spans.get(0).onClick(view);
    }

    @Test
    public void testSetLegalMessage_setsUpDelegateCallback() {
        mContent.setUiInfo(
                defaultUiInfoBuilder()
                        .withLegalMessageLines(ImmutableList.of(new LegalMessageLine(
                                "abc", Arrays.asList(new Link(0, 2, HTTPS_EXAMPLE_COM)))))
                        .build());
        TextView view = mContent.getContentView().findViewById(R.id.legal_message);
        List<ClickableSpan> spans = getClickableSpans((Spannable) view.getText());

        spans.get(0).onClick(view);

        verify(mDelegate).didClickLegalMessageUrl(HTTPS_EXAMPLE_COM);
    }

    // BottomSheetContent interface tests follow:
    @Test
    public void testGetContentView_returnsViewContainingSubviews() {
        View view = mContent.getContentView();

        assertThat(view.findViewById(R.id.autofill_save_card_icon), notNullValue());
        assertThat(view.findViewById(R.id.autofill_credit_card_chip), notNullValue());
    }

    @Test
    public void testGetToolbarView_isNull() {
        assertThat(mContent.getToolbarView(), nullValue());
    }

    @Test
    public void testVerticalScrollOffset_returnsScrollViewOffset() {
        // Provide our scroll view's position for the bottom sheet know when to scroll itself.
        ScrollView scrollView =
                mContent.getContentView().findViewById(R.id.autofill_save_card_scroll_view);
        scrollView.setScrollY(1234);

        assertEquals(1234, mContent.getVerticalScrollOffset());
    }

    @Test
    public void testGetPriority_isHigh() {
        // Show this bottom sheet on high priority since it is triggered by a user action.
        assertEquals(ContentPriority.HIGH, mContent.getPriority());
    }

    @Test
    public void testSwipeToDismissEnabled_isEnabled() {
        assertTrue(mContent.swipeToDismissEnabled());
    }

    @Test
    public void testGetFullHeightRatio_isWrappingContent() {
        assertEquals((float) HeightMode.WRAP_CONTENT, mContent.getFullHeightRatio(), 0.f);
    }

    @Test
    public void testGetHalfHeightRatio_isDisabled() {
        // Half height mode is not supported by the save card bottom sheet view.
        assertEquals((float) HeightMode.DISABLED, mContent.getHalfHeightRatio(), 0.f);
    }

    @Test
    public void testHideOnScroll_isEnabled() {
        assertTrue(mContent.hideOnScroll());
    }

    @Test
    public void testGetPeekHeight() {
        assertEquals(HeightMode.DISABLED, mContent.getPeekHeight());
    }

    @Test
    public void testGetSheetContentDescriptionStringId() {
        // TODO(crbug.com/1454271): Implement save card bottom sheet.
        assertEquals(android.R.string.ok, mContent.getSheetContentDescriptionStringId());
    }

    @Test
    public void testGetSheetHalfHeightAccessibilityStringId() {
        // TODO(crbug.com/1454271): Implement save card bottom sheet.
        assertEquals(android.R.string.ok, mContent.getSheetHalfHeightAccessibilityStringId());
    }

    @Test
    public void testGetSheetFullHeightAccessibilityStringId() {
        // TODO(crbug.com/1454271): Implement save card bottom sheet.
        assertEquals(android.R.string.ok, mContent.getSheetFullHeightAccessibilityStringId());
    }

    @Test
    public void testGetSheetClosedAccessibilityStringId() {
        // TODO(crbug.com/1454271): Implement save card bottom sheet.
        assertEquals(android.R.string.ok, mContent.getSheetClosedAccessibilityStringId());
    }

    private List<ClickableSpan> getClickableSpans(Spannable text) {
        return Arrays.asList(text.getSpans(0, text.length(), ClickableSpan.class));
    }

    private static AutofillSaveCardUiInfo.Builder defaultUiInfoBuilder() {
        return new AutofillSaveCardUiInfo.Builder()
                .withIsForUpload(false)
                .withCardDetail(new CardDetail(/*iconId=*/0, /*label=*/"", /*subLabel=*/""))
                .withLegalMessageLines(Collections.EMPTY_LIST)
                .withTitleText("")
                .withConfirmText("")
                .withCancelText("")
                .withIsGooglePayBrandingEnabled(false)
                .withDescriptionText("");
    }
}

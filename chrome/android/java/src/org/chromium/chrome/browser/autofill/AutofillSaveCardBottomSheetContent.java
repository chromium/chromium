// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill;

import android.content.Context;
import android.text.method.LinkMovementMethod;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.Button;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.annotation.DrawableRes;
import androidx.annotation.IdRes;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.components.autofill.payments.AutofillSaveCardUiInfo;
import org.chromium.components.autofill.payments.LegalMessageLine;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;

import java.util.List;
import java.util.function.Consumer;

/** Implements the content for the autofill save card bottom sheet. */
/*package*/ class AutofillSaveCardBottomSheetContent implements BottomSheetContent {
    private final View mView;
    private final Delegate mDelegate;

    /** User actions delegated by this bottom sheet. */
    /*package*/ interface Delegate {
        /** Called when a legal message link is clicked from the legal message. */
        void didClickLegalMessageUrl(String url);
        /** Called when the bottom sheet is submitted. E.g. through a button click. */
        void didClickConfirm();
        /** Called when the bottom sheet is cancelled. E.g. through a button click. */
        void didClickCancel();
    }

    /**
     * Creates the BottomSheetContent and inflates the view given a delegate responding to actions.
     *
     * @param context The activity context of the window.
     * @param delegate User actions delegated to this object.
     */
    /*package*/ AutofillSaveCardBottomSheetContent(Context context, Delegate delegate) {
        mView = LayoutInflater.from(context).inflate(
                R.layout.autofill_save_card_bottom_sheet, /*root=*/null);
        mDelegate = delegate;
        setButtonDelegateAction(R.id.autofill_save_card_confirm_button, Delegate::didClickConfirm);
        setButtonDelegateAction(R.id.autofill_save_card_cancel_button, Delegate::didClickCancel);
        setLinkMovementMethod(R.id.legal_message);
    }

    private void setButtonDelegateAction(@IdRes int id, Consumer<Delegate> delegateAction) {
        Button button = mView.findViewById(id);
        button.setOnClickListener((View _view) -> delegateAction.accept(mDelegate));
    }

    private void setLinkMovementMethod(@IdRes int id) {
        TextView view = mView.findViewById(id);
        view.setMovementMethod(LinkMovementMethod.getInstance());
    }

    /** Returns the delegate set in the constructor. */
    @VisibleForTesting
    public Delegate getDelegate() {
        return mDelegate;
    }

    /**
     * Set the Icons and text from the given UiInfo.
     *
     * @param uiInfo Contains the UI resources to be applied to this bottom sheet content's view.
     */
    /*package*/ void setUiInfo(AutofillSaveCardUiInfo uiInfo) {
        // TODO(crbug.com/1454271): Implement local save card (isForUpload = true).
        setLogoIconId(uiInfo.getLogoIcon());
        mView.<ImageView>findViewById(R.id.autofill_save_card_credit_card_icon)
                .setImageResource(uiInfo.getCardDetail().issuerIconDrawableId);
        setTextViewText(R.id.autofill_save_card_credit_card_label, uiInfo.getCardDetail().label);
        setTextViewText(
                R.id.autofill_save_card_credit_card_sublabel, uiInfo.getCardDetail().subLabel);
        setLegalMessage(uiInfo.getLegalMessageLines());
        setTextViewText(R.id.autofill_save_card_title_text, uiInfo.getTitleText());
        mView.<Button>findViewById(R.id.autofill_save_card_confirm_button)
                .setText(uiInfo.getConfirmText());
        mView.<Button>findViewById(R.id.autofill_save_card_cancel_button)
                .setText(uiInfo.getCancelText());
        setTextViewText(R.id.autofill_save_card_description_text, uiInfo.getDescriptionText());
    }

    private void setLogoIconId(@DrawableRes int iconId) {
        ImageView imageView = mView.findViewById(R.id.autofill_save_card_icon);
        if (iconId == 0) {
            imageView.setVisibility(View.GONE);
            return;
        }
        imageView.setVisibility(View.VISIBLE);
        imageView.setImageResource(iconId);
    }

    private void setTextViewText(@IdRes int resourceId, CharSequence text) {
        TextView textView = mView.findViewById(resourceId);
        textView.setText(text);
    }

    private void setLegalMessage(List<LegalMessageLine> legalMessageLines) {
        setTextViewText(R.id.legal_message,
                AutofillUiUtils.getSpannableStringForLegalMessageLines(mView.getContext(),
                        legalMessageLines,
                        /*underlineLinks=*/true,
                        /*onClickCallback=*/mDelegate::didClickLegalMessageUrl));
    }

    // BottomSheetContent implementation follows:
    @Override
    public View getContentView() {
        return mView;
    }

    @Nullable
    @Override
    public View getToolbarView() {
        return null;
    }

    @Override
    public int getVerticalScrollOffset() {
        return mView.findViewById(R.id.autofill_save_card_scroll_view).getScrollY();
    }

    @Override
    public void destroy() {
        // In order to be able to know the reason for this bottom sheet being closed, the
        // BottomSheetObserver interface is used by our owning class instead.
    }

    @Override
    public int getPriority() {
        return ContentPriority.HIGH;
    }

    @Override
    public boolean swipeToDismissEnabled() {
        return true;
    }

    @Override
    public float getFullHeightRatio() {
        return HeightMode.WRAP_CONTENT;
    }

    @Override
    public float getHalfHeightRatio() {
        return HeightMode.DISABLED;
    }

    @Override
    public boolean hideOnScroll() {
        return true;
    }

    @Override
    public int getPeekHeight() {
        return HeightMode.DISABLED;
    }

    @Override
    public int getSheetContentDescriptionStringId() {
        // TODO(crbug.com/1454271): Implement save card bottom sheet.
        return android.R.string.ok;
    }

    @Override
    public int getSheetHalfHeightAccessibilityStringId() {
        // TODO(crbug.com/1454271): Implement save card bottom sheet.
        return android.R.string.ok;
    }

    @Override
    public int getSheetFullHeightAccessibilityStringId() {
        // TODO(crbug.com/1454271): Implement save card bottom sheet.
        return android.R.string.ok;
    }

    @Override
    public int getSheetClosedAccessibilityStringId() {
        // TODO(crbug.com/1454271): Implement save card bottom sheet.
        return android.R.string.ok;
    }
}

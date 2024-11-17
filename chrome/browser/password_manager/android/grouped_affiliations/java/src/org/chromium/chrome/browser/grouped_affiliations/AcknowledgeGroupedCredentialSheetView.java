// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.grouped_affiliations;

import android.content.Context;
import android.text.SpannableString;
import android.text.style.StyleSpan;
import android.view.View;
import android.widget.Button;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.password_manager.PasswordManagerResourceProviderFactory;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.ui.text.SpanApplier;

class AcknowledgeGroupedCredentialSheetView implements BottomSheetContent {
    private final View mContent;
    private final String mCurrentOrigin;
    private final String mCredentialOrigin;
    private final Callback<Boolean> mInterationCallback;

    public AcknowledgeGroupedCredentialSheetView(
            View content,
            String currentOrigin,
            String credentialOrigin,
            Callback<Boolean> interactionCallback) {
        mContent = content;
        mCurrentOrigin = currentOrigin;
        mCredentialOrigin = credentialOrigin;
        mInterationCallback = interactionCallback;
        mContent.setOnGenericMotionListener((v, e) -> true); // Filter background interaction.
        setHeaderIcon();
        setTitle();
        setDescription();
        setInteractionCallback();
    }

    private void setHeaderIcon() {
        ImageView sheetHeaderImage = mContent.findViewById(R.id.sheet_header_image);
        sheetHeaderImage.setImageDrawable(
                AppCompatResources.getDrawable(
                        mContent.getContext(),
                        PasswordManagerResourceProviderFactory.create().getPasswordManagerIcon()));
    }

    private void setTitle() {
        TextView titleView = mContent.findViewById(R.id.sheet_title);
        titleView.setText(
                mContent.getResources()
                        .getString(R.string.ack_grouped_cred_sheet_title, mCredentialOrigin));
    }

    private void setDescription() {
        TextView descView = mContent.findViewById(R.id.sheet_text);
        String fullString =
                mContent.getResources()
                        .getString(
                                R.string.ack_grouped_cred_sheet_desc,
                                mCredentialOrigin,
                                mCurrentOrigin);
        // There are 3 spans that should be bold, so applying it 3 times.
        SpannableString formattedString =
                SpanApplier.applySpans(
                        fullString,
                        new SpanApplier.SpanInfo(
                                "<b1>", "</b1>", new StyleSpan(android.graphics.Typeface.BOLD)),
                        new SpanApplier.SpanInfo(
                                "<b2>", "</b2>", new StyleSpan(android.graphics.Typeface.BOLD)),
                        new SpanApplier.SpanInfo(
                                "<b3>", "</b3>", new StyleSpan(android.graphics.Typeface.BOLD)));
        descView.setText(formattedString);
    }

    private void setInteractionCallback() {
        Button positiveButton = mContent.findViewById(R.id.confirmation_button);
        positiveButton.setOnClickListener(view -> mInterationCallback.onResult(true));
        Button negativeButton = mContent.findViewById(R.id.cancel_button);
        negativeButton.setOnClickListener(view -> mInterationCallback.onResult(false));
    }

    @Override
    public View getContentView() {
        return mContent;
    }

    @Nullable
    @Override
    public View getToolbarView() {
        return null;
    }

    @Override
    public int getVerticalScrollOffset() {
        return 0;
    }

    @Override
    public void destroy() {}

    @Override
    public int getPriority() {
        return BottomSheetContent.ContentPriority.HIGH;
    }

    @Override
    public boolean swipeToDismissEnabled() {
        return false;
    }

    @Override
    public @NonNull String getSheetContentDescription(Context context) {
        // TODO(crbug.com/372635361): Append web site to the title.
        return context.getString(R.string.ack_grouped_cred_sheet_title);
    }

    @Override
    public int getSheetHalfHeightAccessibilityStringId() {
        // Half-height is disabled so no need for an accessibility string.
        assert false : "This method should not be called";
        return 0;
    }

    @Override
    public int getSheetFullHeightAccessibilityStringId() {
        // TODO(crbug.com/372635361): Append web site to the title.
        return R.string.ack_grouped_cred_sheet_title;
    }

    @Override
    public int getSheetClosedAccessibilityStringId() {
        // TODO(crbug.com/372635361): Return  ack_grouped_cred_sheet_accepted or
        // ack_grouped_cred_sheet_rejected here depending on user choice.
        return R.string.ack_grouped_cred_sheet_accepted;
    }

    @Override
    public float getHalfHeightRatio() {
        return HeightMode.DISABLED;
    }

    @Override
    public float getFullHeightRatio() {
        return HeightMode.WRAP_CONTENT;
    }

    @Override
    public int getPeekHeight() {
        return HeightMode.DISABLED;
    }
}

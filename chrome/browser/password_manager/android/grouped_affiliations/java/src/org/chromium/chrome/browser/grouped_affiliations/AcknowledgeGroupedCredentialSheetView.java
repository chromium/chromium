// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.grouped_affiliations;

import android.content.Context;
import android.content.res.Resources;
import android.text.SpannableString;
import android.text.TextUtils;
import android.text.style.StyleSpan;
import android.view.View;
import android.widget.Button;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.annotation.StringRes;
import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.password_manager.PasswordManagerResourceProviderFactory;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.ui.text.SpanApplier;

@NullMarked
class AcknowledgeGroupedCredentialSheetView implements BottomSheetContent {
    private static final float URL_IN_TITLE_MAX_LINES = 1.5f;
    private final View mContent;
    private final String mCurrentHostname;
    private final String mCredentialHostname;
    private final Callback<Integer> mInterationCallback;

    public AcknowledgeGroupedCredentialSheetView(
            View content,
            String currentHostname,
            String credentialHostname,
            Callback<Integer> interactionCallback) {
        mContent = content;
        mCurrentHostname = currentHostname;
        mCredentialHostname = credentialHostname;
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
                        .getString(R.string.ack_grouped_cred_sheet_title, mCredentialHostname));
        titleView.post(
                () -> {
                    View sheetItemList = mContent.findViewById(R.id.sheet_item_list);
                    // Url will ellipsize to take max 1.5 lines in the title.
                    float maxWidth = URL_IN_TITLE_MAX_LINES * sheetItemList.getWidth();
                    CharSequence ellipsizedUrl =
                            TextUtils.ellipsize(
                                    mCredentialHostname,
                                    titleView.getPaint(),
                                    maxWidth,
                                    TextUtils.TruncateAt.START);
                    titleView.setText(
                            mContent.getResources()
                                    .getString(
                                            R.string.ack_grouped_cred_sheet_title, ellipsizedUrl));
                });
    }

    private void setDescription() {
        TextView descView = mContent.findViewById(R.id.sheet_text);
        String fullString =
                mContent.getResources()
                        .getString(
                                R.string.ack_grouped_cred_sheet_desc,
                                mCredentialHostname,
                                mCurrentHostname);
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
        positiveButton.setOnClickListener(
                view -> mInterationCallback.onResult(DismissReason.ACCEPT));
        Button negativeButton = mContent.findViewById(R.id.cancel_button);
        negativeButton.setOnClickListener(view -> mInterationCallback.onResult(DismissReason.BACK));
    }

    @Override
    public View getContentView() {
        return mContent;
    }

    @Override
    public @Nullable View getToolbarView() {
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
    public String getSheetContentDescription(Context context) {
        return context.getString(R.string.ack_grouped_cred_sheet_title, mCredentialHostname);
    }

    @Override
    public @StringRes int getSheetHalfHeightAccessibilityStringId() {
        // Half-height is disabled so no need for an accessibility string.
        assert false : "This method should not be called";
        return Resources.ID_NULL;
    }

    @Override
    public @StringRes int getSheetFullHeightAccessibilityStringId() {
        return R.string.ack_grouped_cred_sheet_opened;
    }

    @Override
    public @StringRes int getSheetClosedAccessibilityStringId() {
        return R.string.ack_grouped_cred_sheet_closed;
    }

    @Override
    public float getHalfHeightRatio() {
        return HeightMode.DISABLED;
    }

    @Override
    public float getFullHeightRatio() {
        return HeightMode.WRAP_CONTENT;
    }
}

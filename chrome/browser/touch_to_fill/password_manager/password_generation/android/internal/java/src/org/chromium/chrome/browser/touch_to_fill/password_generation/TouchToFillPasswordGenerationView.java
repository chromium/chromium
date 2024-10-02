// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touch_to_fill.password_generation;

import android.content.Context;
import android.graphics.Typeface;
import android.graphics.drawable.GradientDrawable;
import android.view.View;
import android.widget.Button;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.annotation.Nullable;
import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.password_manager.PasswordManagerResourceProviderFactory;
import org.chromium.chrome.browser.touch_to_fill.common.TouchToFillUtil;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;

/**
 * This class is responsible for rendering the password generation bottom sheet. It is a View in
 * this Model-View-Controller component and doesn't inherit but holds Android Views.
 */
class TouchToFillPasswordGenerationView implements BottomSheetContent {
    private final View mContent;
    private final Context mContext;
    private TextView mPasswordView;

    // Minimum password length that allows to label the password as strong in
    // the UI. Must stay in sync with kLengthSufficientForStrongLabel in
    // components/autofill/core/common/password_generation_util.h.
    private static final int LENGTH_SUFFICIENT_FOR_STRONG_LABEL = 12;

    TouchToFillPasswordGenerationView(Context context, View content) {
        mContext = context;
        mContent = content;

        mContent.setOnGenericMotionListener((v, e) -> true); // Filter background interaction.

        ImageView sheetHeaderImage = mContent.findViewById(R.id.touch_to_fill_sheet_header_image);
        sheetHeaderImage.setImageDrawable(
                AppCompatResources.getDrawable(
                        context,
                        PasswordManagerResourceProviderFactory.create().getPasswordManagerIcon()));
        mPasswordView = mContent.findViewById(R.id.password);
        TouchToFillUtil.addColorAndRippleToBackground(
                mPasswordView, (GradientDrawable) mPasswordView.getBackground(), mContext);
    }

    void setSheetTitle(String generatedPassword) {
        TextView sheetSubtitleView = mContent.findViewById(R.id.touch_to_fill_sheet_title);
        String sheetTitle =
                generatedPassword.length() >= LENGTH_SUFFICIENT_FOR_STRONG_LABEL
                        ? mContext.getString(R.string.password_generation_bottom_sheet_title)
                        : mContext.getString(
                                R.string.password_generation_bottom_sheet_title_without_strong);
        sheetSubtitleView.setText(sheetTitle);
    }

    void setSheetSubtitle(String accountEmail) {
        TextView sheetSubtitleView = mContent.findViewById(R.id.touch_to_fill_sheet_subtitle);
        String sheetSubtitle =
                accountEmail.isEmpty()
                        ? mContext.getString(
                                R.string.password_generation_bottom_sheet_subtitle_no_account)
                        : mContext.getString(
                                R.string.password_generation_bottom_sheet_subtitle, accountEmail);
        sheetSubtitleView.setText(sheetSubtitle);
    }

    void setGeneratedPassword(String generatedPassword) {
        mPasswordView.setTypeface(Typeface.MONOSPACE);
        mPasswordView.setText(generatedPassword);
        mPasswordView.setContentDescription(
                mContext.getString(
                        R.string.password_generation_bottom_sheet_use_password_button_content,
                        generatedPassword));
    }

    void setPasswordAcceptedCallback(Callback<String> callback) {
        Button passwordAcceptedButton = mContent.findViewById(R.id.use_password_button);
        mPasswordView.setOnClickListener(
                (v) -> callback.onResult(mPasswordView.getText().toString()));
        passwordAcceptedButton.setOnClickListener(
                (v) -> callback.onResult(mPasswordView.getText().toString()));
    }

    void setPasswordRejectedCallback(Runnable callback) {
        Button passwordRejectedButton = mContent.findViewById(R.id.reject_password_button);
        passwordRejectedButton.setOnClickListener((v) -> callback.run());
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
    public int getSheetContentDescriptionStringId() {
        return R.string.password_generation_bottom_sheet_content_description;
    }

    @Override
    public int getSheetHalfHeightAccessibilityStringId() {
        // Half-height is disabled so no need for an accessibility string.
        assert false : "This method should not be called";
        return 0;
    }

    @Override
    public int getSheetFullHeightAccessibilityStringId() {
        return R.string.password_generation_bottom_sheet_content_description;
    }

    @Override
    public int getSheetClosedAccessibilityStringId() {
        return R.string.password_generation_bottom_sheet_closed;
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

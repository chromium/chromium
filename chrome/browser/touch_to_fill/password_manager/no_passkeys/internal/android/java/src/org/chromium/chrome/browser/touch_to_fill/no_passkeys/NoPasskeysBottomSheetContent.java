// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touch_to_fill.no_passkeys;

import android.content.Context;
import android.graphics.Typeface;
import android.text.Spannable;
import android.text.SpannableString;
import android.text.style.StyleSpan;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.password_manager.PasswordManagerResourceProviderFactory;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.ui.base.LocalizationUtils;

/** Implements the content for the no passkeys bottom sheet. */
class NoPasskeysBottomSheetContent implements BottomSheetContent {
    private final Delegate mDelegate;
    private final Context mContext;
    private final String mOrigin;
    private View mContentView;

    /** User actions delegated by this bottom sheet. */
    interface Delegate {
        /** Called when the user acknowledged that there are no passkeys. */
        void onClickOk();

        /** Called when the user decides to starts the flow to check other devices for passkeys. */
        void onClickUseAnotherDevice();

        /**
         * Called when the sheet is hidden (but not due to temporary suppression).
         * @see BottomSheetContent#destroy()
         */
        void onDestroy();
    }

    /**
     * Creates the BottomSheetContent and inflates the view given a delegate responding to actions.
     *
     * @param context The activity context of the window.
     * @param origin A formatted {@link String} of the origin to display.
     * @param delegate A {@link Delegate} handling interaction with the sheet.
     */
    NoPasskeysBottomSheetContent(Context context, String origin, Delegate delegate) {
        mDelegate = delegate;
        mContext = context;
        mOrigin = origin;
    }

    private View createContentView() {
        View contentView =
                LayoutInflater.from(mContext).inflate(R.layout.no_passkeys_bottom_sheet, null);
        contentView.setOnGenericMotionListener((v, e) -> true); // Filter background interaction.
        contentView.setLayoutDirection(
                LocalizationUtils.isLayoutRtl()
                        ? View.LAYOUT_DIRECTION_RTL
                        : View.LAYOUT_DIRECTION_LTR);
        ImageView headerImage = contentView.findViewById(R.id.no_passkeys_sheet_header_image);
        headerImage.setImageResource(
                PasswordManagerResourceProviderFactory.create().getPasswordManagerIcon());
        contentView
                .findViewById(R.id.no_passkeys_ok_button)
                .setOnClickListener(v -> mDelegate.onClickOk());
        contentView
                .findViewById(R.id.no_passkeys_use_another_device_button)
                .setOnClickListener(v -> mDelegate.onClickUseAnotherDevice());

        String subtitleString = mContext.getString(R.string.no_passkeys_sheet_subtitle, mOrigin);

        SpannableString spannable = new SpannableString(subtitleString);
        int startIndex = subtitleString.indexOf(mOrigin);
        spannable.setSpan(
                new StyleSpan(Typeface.BOLD),
                startIndex,
                startIndex + mOrigin.length(),
                Spannable.SPAN_INCLUSIVE_EXCLUSIVE);

        TextView subtitle = contentView.findViewById(R.id.no_passkeys_sheet_subtitle);
        subtitle.setText(spannable);
        return contentView;
    }

    // BottomSheetContent implementation:
    @Override
    public View getContentView() {
        if (mContentView == null) {
            mContentView = createContentView();
        }
        return mContentView;
    }

    @Override
    public boolean hasCustomLifecycle() {
        return false;
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
    public void destroy() {
        mDelegate.onDestroy();
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
        return R.string.no_passkeys_sheet_content_description;
    }

    @Override
    public int getSheetHalfHeightAccessibilityStringId() {
        assert false;
        return 0;
    }

    @Override
    public int getSheetFullHeightAccessibilityStringId() {
        return R.string.no_passkeys_sheet_full_height;
    }

    @Override
    public int getSheetClosedAccessibilityStringId() {
        return R.string.no_passkeys_sheet_closed;
    }
}

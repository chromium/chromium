// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touch_to_fill.no_passkeys;

import android.content.Context;
import android.content.res.Resources;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.annotation.StringRes;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.password_manager.PasswordManagerResourceProviderFactory;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetType;
import org.chromium.components.browser_ui.bottomsheet.UserCriticalFeature;
import org.chromium.ui.base.LocalizationUtils;

/** Implements the content for the no passkeys bottom sheet. */
// TODO(crbug.com/581058945): Delete this class once AndroidAdaptiveUiTransitions launches.
@NullMarked
class NoPasskeysBottomSheetContent implements BottomSheetContent {
    private static final BottomSheetType BOTTOM_SHEET_TYPE =
            new BottomSheetType.Builder()
                    .setUserCritical(UserCriticalFeature.TOUCH_TO_FILL_NO_PASSKEYS)
                    .build();

    private final Delegate mDelegate;
    private final Context mContext;
    private final CharSequence mSubtitle;
    private @Nullable View mContentView;

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
     * @param subtitle The subtitle to display, with the origin emphasized.
     * @param delegate A {@link Delegate} handling interaction with the sheet.
     */
    NoPasskeysBottomSheetContent(Context context, CharSequence subtitle, Delegate delegate) {
        mDelegate = delegate;
        mContext = context;
        mSubtitle = subtitle;
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

        TextView subtitle = contentView.findViewById(R.id.no_passkeys_sheet_subtitle);
        subtitle.setText(mSubtitle);
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

    @Override
    public @Nullable View getToolbarView() {
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
    public BottomSheetType getSheetType() {
        return BOTTOM_SHEET_TYPE;
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
    public @StringRes int getSheetHalfHeightAccessibilityStringId() {
        assert false;
        return Resources.ID_NULL;
    }

    @Override
    public @StringRes int getSheetFullHeightAccessibilityStringId() {
        return R.string.no_passkeys_sheet_full_height;
    }

    @Override
    public @StringRes int getSheetClosedAccessibilityStringId() {
        return R.string.no_passkeys_sheet_closed;
    }
}

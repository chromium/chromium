// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.dom_distiller;

import android.content.res.ColorStateList;
import android.graphics.drawable.GradientDrawable;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.TextView;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.R;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** Binds the reader mode bottom sheet properties to the view. */
@NullMarked
public class ReaderModeBottomSheetViewBinder {
    /**
     * Binds PropertyKeys to View properties for the reader mode bottom sheet.
     *
     * @param model The PropertyModel for the View.
     * @param view The View to be bound.
     * @param key The key that's being bound.
     */
    @SuppressWarnings("UseTextAppearance")
    public static void bind(PropertyModel model, ReaderModeBottomSheetView view, PropertyKey key) {
        if (key == ReaderModeBottomSheetProperties.CONTENT_VIEW) {
            ViewGroup controlsContainer = view.findViewById(R.id.controls_container);
            controlsContainer.removeAllViews();
            View contentView = model.get(ReaderModeBottomSheetProperties.CONTENT_VIEW);
            controlsContainer.addView(contentView);
        } else if (key == ReaderModeBottomSheetProperties.BACKGROUND_COLOR) {
            ((GradientDrawable) view.getBackground().mutate())
                    .setColor(model.get(ReaderModeBottomSheetProperties.BACKGROUND_COLOR));
        } else if (key == ReaderModeBottomSheetProperties.SECONDARY_BACKGROUND_COLOR) {
            int color = model.get(ReaderModeBottomSheetProperties.SECONDARY_BACKGROUND_COLOR);
            View fontFamilyContainer = view.findViewById(R.id.font_family_container);
            View fontSizeContainer = view.findViewById(R.id.font_size_container);
            View themeContainer = view.findViewById(R.id.theme_container);

            fontFamilyContainer.getBackground().setTint(color);
            fontSizeContainer.getBackground().setTint(color);
            themeContainer.getBackground().setTint(color);
        } else if (key == ReaderModeBottomSheetProperties.PRIMARY_TEXT_COLOR) {
            int color = model.get(ReaderModeBottomSheetProperties.PRIMARY_TEXT_COLOR);
            TextView title = view.findViewById(R.id.title);
            title.setTextColor(color);
        } else if (key == ReaderModeBottomSheetProperties.SECONDARY_TEXT_COLOR) {
            int color = model.get(ReaderModeBottomSheetProperties.SECONDARY_TEXT_COLOR);
            TextView fontSizeLabelStart = view.findViewById(R.id.text_size_signifier_small);
            TextView fontSizeLabelEnd = view.findViewById(R.id.text_size_signifier_large);
            fontSizeLabelStart.setTextColor(color);
            fontSizeLabelEnd.setTextColor(color);
        } else if (key == ReaderModeBottomSheetProperties.ICON_TINT) {
            ColorStateList colorStateList = model.get(ReaderModeBottomSheetProperties.ICON_TINT);
            ImageView dragHandle = view.findViewById(R.id.drag_handle);
            TextView title = view.findViewById(R.id.title);
            dragHandle.setImageTintList(colorStateList);
            title.setCompoundDrawableTintList(colorStateList);
        }
    }
}

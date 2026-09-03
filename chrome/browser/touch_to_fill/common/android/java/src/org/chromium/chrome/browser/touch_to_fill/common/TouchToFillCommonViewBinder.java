// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touch_to_fill.common;

import static android.content.res.Resources.ID_NULL;

import static org.chromium.chrome.browser.touch_to_fill.common.TouchToFillCommonProperties.ButtonProperties.ON_CLICK_ACTION;
import static org.chromium.chrome.browser.touch_to_fill.common.TouchToFillCommonProperties.ButtonProperties.TEXT_ID;
import static org.chromium.chrome.browser.touch_to_fill.common.TouchToFillCommonProperties.HeaderProperties.IMAGE_DRAWABLE_ID;
import static org.chromium.chrome.browser.touch_to_fill.common.TouchToFillCommonProperties.HeaderProperties.SUBTITLE_BOTTOM_MARGIN;
import static org.chromium.chrome.browser.touch_to_fill.common.TouchToFillCommonProperties.HeaderProperties.SUBTITLE_ID;
import static org.chromium.chrome.browser.touch_to_fill.common.TouchToFillCommonProperties.HeaderProperties.TITLE_BOTTOM_MARGIN;
import static org.chromium.chrome.browser.touch_to_fill.common.TouchToFillCommonProperties.HeaderProperties.TITLE_ID;
import static org.chromium.chrome.browser.touch_to_fill.common.TouchToFillCommonProperties.HeaderProperties.TITLE_STRING;

import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.autofill.AutofillUiUtils;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Provides functions that map {@link TouchToFillCommonProperties} changes in a {@link
 * PropertyModel} to the suitable method in the common TouchToFill views.
 */
@NullMarked
public final class TouchToFillCommonViewBinder {
    /**
     * Factory used to create a new header.
     *
     * @param parent The parent {@link ViewGroup} of the new item.
     */
    public static View createHeaderItemView(ViewGroup parent) {
        return LayoutInflater.from(parent.getContext())
                .inflate(R.layout.touch_to_fill_header_item, parent, false);
    }

    /**
     * Called whenever a property in the given model changes. It updates the given view accordingly.
     *
     * @param model The observed {@link PropertyModel}. Its data need to be reflected in the view.
     * @param view The {@link View} of the header to update.
     * @param propertyKey The {@link PropertyKey} which changed.
     */
    public static void bindHeaderView(PropertyModel model, View view, PropertyKey propertyKey) {
        ImageView sheetHeaderImage = view.findViewById(R.id.branding_icon);
        TextView sheetHeaderTitle = view.findViewById(R.id.touch_to_fill_sheet_header_title);
        TextView sheetHeaderSubtitle = view.findViewById(R.id.touch_to_fill_sheet_header_subtitle);

        if (propertyKey == IMAGE_DRAWABLE_ID) {
            sheetHeaderImage.setImageDrawable(
                    AppCompatResources.getDrawable(
                            view.getContext(), model.get(IMAGE_DRAWABLE_ID)));
        } else if (propertyKey == TITLE_ID) {
            sheetHeaderTitle.setText(view.getContext().getString(model.get(TITLE_ID)));
        } else if (propertyKey == SUBTITLE_ID) {
            sheetHeaderSubtitle.setVisibility(View.VISIBLE);
            sheetHeaderSubtitle.setText(view.getContext().getString(model.get(SUBTITLE_ID)));
        } else if (propertyKey == TITLE_STRING) {
            sheetHeaderTitle.setText(model.get(TITLE_STRING));
        } else if (propertyKey == TITLE_BOTTOM_MARGIN) {
            ViewGroup.MarginLayoutParams layoutParams =
                    (ViewGroup.MarginLayoutParams) sheetHeaderTitle.getLayoutParams();
            layoutParams.bottomMargin =
                    view.getContext()
                            .getResources()
                            .getDimensionPixelSize(
                                    model.get(TITLE_BOTTOM_MARGIN) == ID_NULL
                                            ? R.dimen.ttf_sheet_default_header_title_bottom_margin
                                            : model.get(TITLE_BOTTOM_MARGIN));
            sheetHeaderTitle.setLayoutParams(layoutParams);
        } else if (propertyKey == SUBTITLE_BOTTOM_MARGIN) {
            if (model.get(SUBTITLE_BOTTOM_MARGIN) == ID_NULL) {
                return;
            }
            ViewGroup.MarginLayoutParams layoutParams =
                    (ViewGroup.MarginLayoutParams) sheetHeaderSubtitle.getLayoutParams();
            layoutParams.bottomMargin =
                    view.getContext()
                            .getResources()
                            .getDimensionPixelSize(
                                    model.get(SUBTITLE_BOTTOM_MARGIN) == ID_NULL
                                            ? R.dimen
                                                    .ttf_sheet_default_header_subtitle_bottom_margin
                                            : model.get(SUBTITLE_BOTTOM_MARGIN));
            sheetHeaderSubtitle.setLayoutParams(layoutParams);
        } else {
            assert false : "Unhandled update to property: " + propertyKey;
        }
    }

    /**
     * Factory used to create a new "Continue" or "Autofill" button that fills in data into the
     * focused field.
     *
     * @param parent The parent {@link ViewGroup} of the new item.
     */
    public static Button createFillButtonView(ViewGroup parent) {
        Button buttonView =
                (Button)
                        LayoutInflater.from(parent.getContext())
                                .inflate(R.layout.touch_to_fill_fill_button, parent, false);
        AutofillUiUtils.setFilterTouchForSecurity(buttonView);
        return buttonView;
    }

    /**
     * Factory used to create a new "Cancel" button that dismisses the current screen.
     *
     * @param parent The parent {@link ViewGroup} of the new item.
     */
    public static Button createTextButtonView(ViewGroup parent) {
        Button buttonView =
                (Button)
                        LayoutInflater.from(parent.getContext())
                                .inflate(R.layout.touch_to_fill_text_button, parent, false);
        AutofillUiUtils.setFilterTouchForSecurity(buttonView);
        return buttonView;
    }

    /**
     * Called whenever a property in the given model changes. It updates the given view accordingly.
     *
     * @param model The observed {@link PropertyModel}. Its data need to be reflected in the view.
     * @param button The {@link Button} from the bottom sheet to update.
     * @param propertyKey The {@link PropertyKey} which changed.
     */
    public static void bindButtonView(PropertyModel model, Button button, PropertyKey propertyKey) {
        if (propertyKey == TEXT_ID) {
            button.setText(model.get(TEXT_ID));
        } else if (propertyKey == ON_CLICK_ACTION) {
            assert model.get(ON_CLICK_ACTION) != null : "A button must have an action";
            button.setOnClickListener(_ -> model.get(ON_CLICK_ACTION).run());
        } else {
            assert false : "Unhandled update to property: " + propertyKey;
        }
    }

    private TouchToFillCommonViewBinder() {}
}

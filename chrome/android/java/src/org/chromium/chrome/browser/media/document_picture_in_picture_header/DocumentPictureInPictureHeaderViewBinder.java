// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.document_picture_in_picture_header;

import android.content.res.ColorStateList;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;

import androidx.core.graphics.Insets;
import androidx.core.widget.ImageViewCompat;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.R;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * View binder for the Document Picture-in-Picture (PiP) header.
 *
 * <p>This class is responsible for updating the header's views in response to property model
 * changes.
 */
@NullMarked
public class DocumentPictureInPictureHeaderViewBinder {
    public static void bind(PropertyModel model, View view, PropertyKey key) {
        if (key == DocumentPictureInPictureHeaderProperties.IS_SHOWN) {
            view.setVisibility(
                    model.get(DocumentPictureInPictureHeaderProperties.IS_SHOWN)
                            ? View.VISIBLE
                            : View.GONE);
        } else if (key == DocumentPictureInPictureHeaderProperties.HEADER_HEIGHT) {
            int headerHeight = model.get(DocumentPictureInPictureHeaderProperties.HEADER_HEIGHT);
            ViewGroup.LayoutParams layoutParams = view.getLayoutParams();
            layoutParams.height = headerHeight;
            view.setLayoutParams(layoutParams);
        } else if (key == DocumentPictureInPictureHeaderProperties.HEADER_SPACING) {
            Insets headerSpacing =
                    model.get(DocumentPictureInPictureHeaderProperties.HEADER_SPACING);
            view.setPadding(
                    headerSpacing.left,
                    headerSpacing.top,
                    headerSpacing.right,
                    headerSpacing.bottom);
        } else if (key == DocumentPictureInPictureHeaderProperties.BACKGROUND_COLOR) {
            view.setBackgroundColor(
                    model.get(DocumentPictureInPictureHeaderProperties.BACKGROUND_COLOR));
        } else if (key == DocumentPictureInPictureHeaderProperties.TINT_COLOR_LIST) {
            updateTintColorList(
                    view, model.get(DocumentPictureInPictureHeaderProperties.TINT_COLOR_LIST));
        } else if (key == DocumentPictureInPictureHeaderProperties.ON_BACK_TO_TAB_CLICK_LISTENER) {
            view.findViewById(R.id.document_picture_in_picture_header_back_to_tab)
                    .setOnClickListener(
                            model.get(
                                    DocumentPictureInPictureHeaderProperties
                                            .ON_BACK_TO_TAB_CLICK_LISTENER));
        } else if (key == DocumentPictureInPictureHeaderProperties.ON_LAYOUT_CHANGE_LISTENER) {
            view.addOnLayoutChangeListener(
                    model.get(DocumentPictureInPictureHeaderProperties.ON_LAYOUT_CHANGE_LISTENER));
        } else if (key == DocumentPictureInPictureHeaderProperties.NON_DRAGGABLE_AREAS) {
            view.setSystemGestureExclusionRects(
                    model.get(DocumentPictureInPictureHeaderProperties.NON_DRAGGABLE_AREAS));
        } else if (key == DocumentPictureInPictureHeaderProperties.IS_BACK_TO_TAB_SHOWN) {
            view.findViewById(R.id.document_picture_in_picture_header_back_to_tab)
                    .setVisibility(
                            model.get(DocumentPictureInPictureHeaderProperties.IS_BACK_TO_TAB_SHOWN)
                                    ? View.VISIBLE
                                    : View.GONE);
        }
    }

    private static void updateTintColorList(View view, ColorStateList tintColorList) {
        ImageView backToTab =
                view.findViewById(R.id.document_picture_in_picture_header_back_to_tab);

        ImageViewCompat.setImageTintList(backToTab, tintColorList);
    }
}

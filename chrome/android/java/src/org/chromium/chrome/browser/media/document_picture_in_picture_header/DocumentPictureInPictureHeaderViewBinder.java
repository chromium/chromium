// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.document_picture_in_picture_header;

import android.view.View;
import android.view.ViewGroup;

import androidx.core.graphics.Insets;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * View binder for the Document Picture-in-Picture (PiP) header.
 *
 * <p>This class is responsible for updating the header's views in response to property model
 * changes.
 */
@NullMarked
class DocumentPictureInPictureHeaderViewBinder {
    private DocumentPictureInPictureHeaderViewBinder() {}

    static void bind(
            PropertyModel model, DocumentPictureInPictureHeaderView view, PropertyKey key) {
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
            view.setTintColorList(
                    model.get(DocumentPictureInPictureHeaderProperties.TINT_COLOR_LIST));
        } else if (key == DocumentPictureInPictureHeaderProperties.ON_BACK_TO_TAB_CLICK_LISTENER) {
            view.setBackToTabClickListener(
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
            view.setBackToTabShown(
                    model.get(DocumentPictureInPictureHeaderProperties.IS_BACK_TO_TAB_SHOWN));
        } else if (key == DocumentPictureInPictureHeaderProperties.SECURITY_ICON) {
            view.setSecurityIconResource(
                    model.get(DocumentPictureInPictureHeaderProperties.SECURITY_ICON));
        } else if (key
                == DocumentPictureInPictureHeaderProperties
                        .SECURITY_ICON_CONTENT_DESCRIPTION_RES_ID) {
            view.setSecurityIconContentDescription(
                    model.get(
                            DocumentPictureInPictureHeaderProperties
                                    .SECURITY_ICON_CONTENT_DESCRIPTION_RES_ID));
        } else if (key
                == DocumentPictureInPictureHeaderProperties.ON_SECURITY_ICON_CLICK_LISTENER) {
            view.setSecurityIconClickListener(
                    model.get(
                            DocumentPictureInPictureHeaderProperties
                                    .ON_SECURITY_ICON_CLICK_LISTENER));
        } else if (key == DocumentPictureInPictureHeaderProperties.URL_STRING) {
            view.setUrl(model.get(DocumentPictureInPictureHeaderProperties.URL_STRING));
        } else if (key == DocumentPictureInPictureHeaderProperties.URL_ELLIPSIZE_BEHAVIOR) {
            view.setUrlEllipsizeBehavior(
                    model.get(DocumentPictureInPictureHeaderProperties.URL_ELLIPSIZE_BEHAVIOR));
        } else if (key == DocumentPictureInPictureHeaderProperties.BRANDED_COLOR_SCHEME) {
            view.setBrandedColorScheme(
                    model.get(DocumentPictureInPictureHeaderProperties.BRANDED_COLOR_SCHEME));
        } else if (key == DocumentPictureInPictureHeaderProperties.COMPONENT_SIZE) {
            view.setComponentSize(
                    model.get(DocumentPictureInPictureHeaderProperties.COMPONENT_SIZE));
        }
    }
}

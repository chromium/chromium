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
        }
    }
}

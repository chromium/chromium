// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.document_picture_in_picture_header;

import android.content.res.ColorStateList;
import android.text.TextUtils;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.annotation.StringRes;
import androidx.core.graphics.Insets;
import androidx.core.widget.ImageViewCompat;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.omnibox.styles.OmniboxResourceProvider;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
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
        } else if (key == DocumentPictureInPictureHeaderProperties.SECURITY_ICON) {
            ImageView icon =
                    view.findViewById(R.id.document_picture_in_picture_header_security_icon);
            icon.setImageResource(
                    model.get(DocumentPictureInPictureHeaderProperties.SECURITY_ICON));
        } else if (key
                == DocumentPictureInPictureHeaderProperties
                        .SECURITY_ICON_CONTENT_DESCRIPTION_RES_ID) {
            updateSecurityIconContentDescription(
                    view,
                    model.get(
                            DocumentPictureInPictureHeaderProperties
                                    .SECURITY_ICON_CONTENT_DESCRIPTION_RES_ID));
        } else if (key
                == DocumentPictureInPictureHeaderProperties.ON_SECURITY_ICON_CLICK_LISTENER) {
            view.findViewById(R.id.document_picture_in_picture_header_security_icon)
                    .setOnClickListener(
                            model.get(
                                    DocumentPictureInPictureHeaderProperties
                                            .ON_SECURITY_ICON_CLICK_LISTENER));
        } else if (key == DocumentPictureInPictureHeaderProperties.URL_STRING) {
            updateUrl(view, model.get(DocumentPictureInPictureHeaderProperties.URL_STRING));
        } else if (key == DocumentPictureInPictureHeaderProperties.URL_ELLIPSIZE_BEHAVIOR) {
            updateUrlEllipsizeBehavior(
                    view,
                    model.get(DocumentPictureInPictureHeaderProperties.URL_ELLIPSIZE_BEHAVIOR));
        } else if (key == DocumentPictureInPictureHeaderProperties.BRANDED_COLOR_SCHEME) {
            updateBrandedColorScheme(
                    view, model.get(DocumentPictureInPictureHeaderProperties.BRANDED_COLOR_SCHEME));
        } else if (key == DocumentPictureInPictureHeaderProperties.COMPONENT_SIZE) {
            int componentSize = model.get(DocumentPictureInPictureHeaderProperties.COMPONENT_SIZE);
            updateComponentSize(view, componentSize);
        }
    }

    private static void updateComponentSize(View view, int componentSize) {
        View backToTab = view.findViewById(R.id.document_picture_in_picture_header_back_to_tab);
        View securityIcon =
                view.findViewById(R.id.document_picture_in_picture_header_security_icon);
        View urlBar = view.findViewById(R.id.document_picture_in_picture_header_url_bar);

        ViewGroup.LayoutParams backToTabParams = backToTab.getLayoutParams();
        backToTabParams.width = componentSize;
        backToTabParams.height = componentSize;
        backToTab.setLayoutParams(backToTabParams);

        ViewGroup.LayoutParams securityIconParams = securityIcon.getLayoutParams();
        securityIconParams.width = componentSize;
        securityIconParams.height = componentSize;
        securityIcon.setLayoutParams(securityIconParams);

        ViewGroup.LayoutParams urlBarParams = urlBar.getLayoutParams();
        urlBarParams.height = componentSize;
        urlBar.setLayoutParams(urlBarParams);
    }

    private static void updateTintColorList(View view, ColorStateList tintColorList) {
        ImageView backToTab =
                view.findViewById(R.id.document_picture_in_picture_header_back_to_tab);
        ImageView securityIcon =
                view.findViewById(R.id.document_picture_in_picture_header_security_icon);

        ImageViewCompat.setImageTintList(backToTab, tintColorList);
        ImageViewCompat.setImageTintList(securityIcon, tintColorList);
    }

    private static void updateBrandedColorScheme(
            View view, @BrandedColorScheme int brandedColorScheme) {
        TextView urlBar = view.findViewById(R.id.document_picture_in_picture_header_url_bar);
        urlBar.setTextColor(
                OmniboxResourceProvider.getUrlBarPrimaryTextColor(
                        view.getContext(), brandedColorScheme));
    }

    private static void updateSecurityIconContentDescription(
            View view, @StringRes int descriptionResId) {
        ImageView securityIcon =
                view.findViewById(R.id.document_picture_in_picture_header_security_icon);

        if (descriptionResId != 0) {
            securityIcon.setContentDescription(view.getResources().getString(descriptionResId));
        }
    }

    private static void updateUrl(View view, String urlHost) {
        TextView urlBar = view.findViewById(R.id.document_picture_in_picture_header_url_bar);
        urlBar.setText(urlHost);
        urlBar.setTooltipText(urlHost);
    }

    private static void updateUrlEllipsizeBehavior(View view, TextUtils.TruncateAt behavior) {
        TextView urlBar = view.findViewById(R.id.document_picture_in_picture_header_url_bar);
        urlBar.setEllipsize(behavior);
    }
}

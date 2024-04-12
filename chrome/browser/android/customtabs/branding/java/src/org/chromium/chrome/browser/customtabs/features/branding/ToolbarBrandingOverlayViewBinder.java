// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features.branding;

import static org.chromium.chrome.browser.customtabs.features.branding.ToolbarBrandingOverlayProperties.COLOR_DATA;

import android.content.Context;
import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.omnibox.styles.OmniboxResourceProvider;
import org.chromium.chrome.browser.theme.ThemeUtils;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** View binder for the toolbar branding overlay. */
public class ToolbarBrandingOverlayViewBinder {
    public static void bind(PropertyModel model, View view, PropertyKey key) {
        if (key == COLOR_DATA) {
            var colorData = model.get(COLOR_DATA);
            Context context = view.getContext();
            view.setBackgroundColor(colorData.getBackground());
            ((ImageView) view.findViewById(R.id.branding_icon))
                    .setImageTintList(
                            ThemeUtils.getThemedToolbarIconTint(
                                    context, colorData.getBrandedColorScheme()));
            ((TextView) view.findViewById(R.id.branding_text))
                    .setTextColor(
                            OmniboxResourceProvider.getUrlBarPrimaryTextColor(
                                    context, colorData.getBrandedColorScheme()));
        }
    }
}

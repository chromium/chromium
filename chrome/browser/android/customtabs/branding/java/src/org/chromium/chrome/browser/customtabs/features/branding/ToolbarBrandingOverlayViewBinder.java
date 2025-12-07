// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features.branding;

import static org.chromium.chrome.browser.customtabs.features.branding.ToolbarBrandingOverlayProperties.COLOR_DATA;
import static org.chromium.chrome.browser.customtabs.features.branding.ToolbarBrandingOverlayProperties.HIDING_PROGRESS;

import android.content.Context;
import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.omnibox.styles.OmniboxResourceProvider;
import org.chromium.chrome.browser.theme.ThemeUtils;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** View binder for the toolbar branding overlay. */
@NullMarked
public class ToolbarBrandingOverlayViewBinder {
    private static final int MAX_DRAWABLE_LEVEL = 10_000;

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
        } else if (key == HIDING_PROGRESS) {
            float progress = model.get(HIDING_PROGRESS);
            view.setAlpha(1.f - progress);
            ImageView icon = view.findViewById(R.id.branding_icon);
            icon.getDrawable().setLevel(Math.round(progress * MAX_DRAWABLE_LEVEL));
        }
    }
}

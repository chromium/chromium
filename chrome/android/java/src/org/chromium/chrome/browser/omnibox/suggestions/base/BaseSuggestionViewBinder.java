// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.base;

import android.content.res.Resources;
import android.support.annotation.CallSuper;
import android.support.v4.view.ViewCompat;
import android.view.View;
import android.widget.ImageView;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.omnibox.suggestions.SuggestionCommonProperties;
import org.chromium.chrome.browser.ui.styles.ChromeColors;
import org.chromium.chrome.browser.ui.widget.RoundedCornerImageView;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor.ViewBinder;

/**
 * Binds base suggestion view properties.
 */
public class BaseSuggestionViewBinder
        implements ViewBinder<PropertyModel, BaseSuggestionView, PropertyKey> {
    @Override
    @CallSuper
    public void bind(PropertyModel model, BaseSuggestionView view, PropertyKey propertyKey) {
        if (BaseSuggestionViewProperties.SUGGESTION_DELEGATE == propertyKey) {
            view.setDelegate(model.get(BaseSuggestionViewProperties.SUGGESTION_DELEGATE));
        } else if (BaseSuggestionViewProperties.ICON == propertyKey) {
            updateSuggestionIcon(model, view);
        } else if (BaseSuggestionViewProperties.ACTION_ICON == propertyKey) {
            updateActionIcon(model, view);
        } else if (SuggestionCommonProperties.LAYOUT_DIRECTION == propertyKey) {
            ViewCompat.setLayoutDirection(
                    view, model.get(SuggestionCommonProperties.LAYOUT_DIRECTION));
        } else if (SuggestionCommonProperties.USE_DARK_COLORS == propertyKey) {
            updateSuggestionIcon(model, view);
            updateActionIcon(model, view);
        }
    }

    /** Returns which color scheme should be used to tint drawables. */
    private static boolean isDarkMode(PropertyModel model) {
        return model.get(SuggestionCommonProperties.USE_DARK_COLORS);
    }

    /** Update attributes of decorated suggestion icon. */
    private static void updateSuggestionIcon(PropertyModel model, BaseSuggestionView baseView) {
        final RoundedCornerImageView view = baseView.getSuggestionImageView();
        final SuggestionDrawableState sds = model.get(BaseSuggestionViewProperties.ICON);

        if (sds != null) {
            final Resources res = view.getContext().getResources();
            final int paddingStart = res.getDimensionPixelSize(sds.isLarge
                            ? R.dimen.omnibox_suggestion_36dp_icon_margin_start
                            : R.dimen.omnibox_suggestion_24dp_icon_margin_start);
            final int paddingEnd = res.getDimensionPixelSize(sds.isLarge
                            ? R.dimen.omnibox_suggestion_36dp_icon_margin_end
                            : R.dimen.omnibox_suggestion_24dp_icon_margin_end);
            final int edgeSize = res.getDimensionPixelSize(sds.isLarge
                            ? R.dimen.omnibox_suggestion_36dp_icon_size
                            : R.dimen.omnibox_suggestion_24dp_icon_size);

            view.setPadding(paddingStart, 0, paddingEnd, 0);
            view.setMinimumHeight(edgeSize);

            // TODO(ender): move logic applying corner rounding to updateIcon when action images use
            // RoundedCornerImageView too.
            RoundedCornerImageView rciv = (RoundedCornerImageView) view;
            int radius = sds.useRoundedCorners
                    ? res.getDimensionPixelSize(R.dimen.default_rounded_corner_radius)
                    : 0;
            rciv.setRoundedCorners(radius, radius, radius, radius);
        }

        updateIcon(view, sds, isDarkMode(model));
    }

    /** Update attributes of decorated suggestion icon. */
    private static void updateActionIcon(PropertyModel model, BaseSuggestionView baseView) {
        final ImageView view = baseView.getActionImageView();
        final SuggestionDrawableState sds = model.get(BaseSuggestionViewProperties.ACTION_ICON);
        updateIcon(view, sds, isDarkMode(model));
    }

    /** Update image view using supplied drawable state object */
    private static void updateIcon(
            ImageView view, SuggestionDrawableState sds, boolean useDarkColors) {
        final Resources res = view.getContext().getResources();

        view.setVisibility(sds == null ? View.GONE : View.VISIBLE);
        if (sds == null) {
            // Release any drawable that is still attached to this view to reclaim memory.
            view.setImageDrawable(null);
            return;
        }

        view.setImageDrawable(sds.drawable);
        if (sds.allowTint) {
            ApiCompatibilityUtils.setImageTintList(
                    view, ChromeColors.getIconTint(view.getContext(), !useDarkColors));
        }
    }
}

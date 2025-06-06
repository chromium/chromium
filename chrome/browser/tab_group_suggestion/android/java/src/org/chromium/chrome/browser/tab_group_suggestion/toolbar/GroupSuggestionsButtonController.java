// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_group_suggestion.toolbar;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.view.View;

import org.chromium.base.supplier.Supplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab_group_suggestion.R;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarButtonVariant;
import org.chromium.chrome.browser.toolbar.optional_button.BaseButtonDataProvider;

@NullMarked
public class GroupSuggestionsButtonController extends BaseButtonDataProvider {

    public GroupSuggestionsButtonController(
            Supplier<@Nullable Tab> activeTabSupplier, Context context, Drawable buttonDrawable) {
        super(
                activeTabSupplier,
                /* modalDialogManager= */ null,
                buttonDrawable,
                /* TODO(salg): Replace placeholder strings. */
                /* contentDescription= */ context.getString(
                        R.string.data_sharing_shared_tab_groups_activity, 2),
                /* actionChipLabelResId= */ R.string.data_sharing_shared_tab_groups_activity,
                /* supportsTinting= */ true,
                /* iphCommandBuilder= */ null,
                AdaptiveToolbarButtonVariant.TAB_GROUPING,
                /* tooltipTextResId= */ R.string.data_sharing_shared_tab_groups_activity);
    }

    @Override
    public void onClick(View view) {
        // TODO(salg): Implement tab grouping.
    }
}

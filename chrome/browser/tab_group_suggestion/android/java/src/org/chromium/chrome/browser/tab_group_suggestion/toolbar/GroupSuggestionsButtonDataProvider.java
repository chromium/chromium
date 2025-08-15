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
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarButtonVariant;
import org.chromium.chrome.browser.toolbar.optional_button.BaseButtonDataProvider;

/** Defines the UI details and click handler for the tab grouping toolbar button. */
@NullMarked
public class GroupSuggestionsButtonDataProvider extends BaseButtonDataProvider {
    private final Supplier<GroupSuggestionsButtonController>
            mGroupSuggestionsButtonControllerSupplier;
    private final Supplier<TabModelSelector> mTabModelSelectorSupplier;

    public GroupSuggestionsButtonDataProvider(
            Supplier<@Nullable Tab> activeTabSupplier,
            Context context,
            Drawable buttonDrawable,
            Supplier<GroupSuggestionsButtonController> groupSuggestionsButtonControllerSupplier,
            Supplier<TabModelSelector> tabModelSelectorSupplier) {
        super(
                activeTabSupplier,
                /* modalDialogManager= */ null,
                buttonDrawable,
                /* contentDescription= */ context.getString(
                        R.string.tab_group_suggestion_action_chip_label),
                /* actionChipLabelResId= */ R.string.tab_group_suggestion_action_chip_label,
                /* supportsTinting= */ true,
                /* iphCommandBuilder= */ null,
                AdaptiveToolbarButtonVariant.TAB_GROUPING,
                /* tooltipTextResId= */ R.string.tab_group_suggestion_action_chip_label);
        mGroupSuggestionsButtonControllerSupplier = groupSuggestionsButtonControllerSupplier;
        mTabModelSelectorSupplier = tabModelSelectorSupplier;
    }

    @Override
    protected boolean shouldShowButton(@Nullable Tab tab) {
        if (!super.shouldShowButton(tab)) {
            return false;
        }

        // Don't show button if the tab is already grouped (e.g. after clicking the button).
        return tab.getTabGroupId() == null;
    }

    @Override
    public void onClick(View view) {
        if (!mActiveTabSupplier.hasValue()
                || !mGroupSuggestionsButtonControllerSupplier.hasValue()
                || !mTabModelSelectorSupplier.hasValue()) {
            return;
        }

        mGroupSuggestionsButtonControllerSupplier
                .get()
                .onButtonClicked(
                        mActiveTabSupplier.get(),
                        mTabModelSelectorSupplier
                                .get()
                                .getTabGroupModelFilterProvider()
                                .getTabGroupModelFilter(/* isIncognito= */ false));
        notifyObservers(false);
    }
}

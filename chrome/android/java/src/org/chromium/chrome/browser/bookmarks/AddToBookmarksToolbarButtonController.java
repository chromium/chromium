// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import android.content.res.Resources;
import android.graphics.drawable.Drawable;
import android.view.View;

import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.toolbar.BaseButtonDataProvider;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarButtonVariant;
import org.chromium.chrome.browser.user_education.IPHCommandBuilder;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;

/**
 *  Defines a toolbar button to add the current web page to bookmarks.
 */
public class AddToBookmarksToolbarButtonController extends BaseButtonDataProvider {
    private final Supplier<TabBookmarker> mTabBookmarkerSupplier;
    private final Supplier<Tracker> mTrackerSupplier;

    /**
     * Creates a new instance of {@code AddToBookmarksToolbarButtonController}
     * @param activeTabSupplier Supplier for the current active tab.
     * @param buttonDrawable Drawable for the button icon.
     * @param contentDescription String for the button's content description.
     * @param tabBookmarkerSupplier Supplier of a {@code TabBookmarker} instance.
     * @param trackerSupplier Supplier for the current profile tracker. Used for IPH.
     */
    public AddToBookmarksToolbarButtonController(Supplier<Tab> activeTabSupplier,
            Drawable buttonDrawable, String contentDescription,
            Supplier<TabBookmarker> tabBookmarkerSupplier, Supplier<Tracker> trackerSupplier) {
        super(activeTabSupplier, /* modalDialogManager = */ null, buttonDrawable,
                contentDescription,
                /* actionChipLabelResId = */ Resources.ID_NULL, /* supportsTinting = */ true,
                /* iphCommandBuilder = */ null, AdaptiveToolbarButtonVariant.ADD_TO_BOOKMARKS);
        mTabBookmarkerSupplier = tabBookmarkerSupplier;
        mTrackerSupplier = trackerSupplier;
    }

    @Override
    protected IPHCommandBuilder getIphCommandBuilder(Tab tab) {
        return new IPHCommandBuilder(tab.getContext().getResources(),
                FeatureConstants
                        .ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_CUSTOMIZATION_ADD_TO_BOOKMARKS_FEATURE,
                /* stringId = */ R.string.adaptive_toolbar_button_add_to_bookmarks_iph,
                /* accessibilityStringId = */
                R.string.adaptive_toolbar_button_add_to_bookmarks_iph);
    }

    @Override
    public void onClick(View view) {
        if (!mTabBookmarkerSupplier.hasValue() || !mActiveTabSupplier.hasValue()) return;

        if (mTrackerSupplier.hasValue()) {
            mTrackerSupplier.get().notifyEvent(
                    EventConstants.ADAPTIVE_TOOLBAR_CUSTOMIZATION_ADD_TO_BOOKMARKS_OPENED);
        }

        RecordUserAction.record("MobileTopToolbarAddToBookmarksButton");
        mTabBookmarkerSupplier.get().addOrEditBookmark(mActiveTabSupplier.get());
    }
}

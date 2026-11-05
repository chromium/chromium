// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.graphics.Rect;
import android.view.View;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.user_education.IphCommand;
import org.chromium.chrome.browser.user_education.IphCommandBuilder;
import org.chromium.chrome.browser.user_education.UserEducationHelper;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.feature_engagement.FeatureConstants;

/**
 * Controller to manage when an In-Product Help (IPH) callout bubble for selecting tabs in the
 * {@link TabListEditorCoordinator} is shown.
 */
@NullMarked
class TabListEditorIphController {
    static final String RESOLVED_ANCHOR_VIEW_HISTOGRAM =
            "Android.TabListEditorIphController.ResolvedAnchorView";

    private final UserEducationHelper mUserEducationHelper;
    private final TabListEditorLayout mLayout;

    /**
     * @param userEducationHelper The {@link UserEducationHelper} to show user education events.
     * @param layout The {@link TabListEditorLayout} hosting the tab views.
     */
    TabListEditorIphController(
            UserEducationHelper userEducationHelper, TabListEditorLayout layout) {
        mUserEducationHelper = userEducationHelper;
        mLayout = layout;
    }

    /** Requests showing the select-tabs IPH on the first visible tab after layout. */
    void maybeShowSelectTabsIph() {
        mLayout.runOnFirstTabViewAvailable(this::maybeShowIph);
    }

    private void maybeShowIph(@Nullable View tabView) {
        View titleView = tabView != null ? tabView.findViewById(R.id.tab_title) : null;
        if (titleView == null || !titleView.isShown()) {
            RecordHistogram.recordBooleanHistogram(RESOLVED_ANCHOR_VIEW_HISTOGRAM, false);
            return;
        }
        RecordHistogram.recordBooleanHistogram(RESOLVED_ANCHOR_VIEW_HISTOGRAM, true);

        IphCommand command =
                new IphCommandBuilder(
                                titleView.getResources(),
                                FeatureConstants.TAB_PICKER_SELECT_TABS_FEATURE,
                                /* stringId= */ R.string.tabpicker_select_tabs_iph,
                                /* accessibilityStringId= */ R.string.tabpicker_select_tabs_iph)
                        .setAnchorView(titleView)
                        .setInsetRect(new Rect())
                        .build();

        mUserEducationHelper.requestShowIph(command);
    }

    /** Dismisses the IPH bubble if currently showing. */
    void destroy() {
        mUserEducationHelper.dismissTextBubble();
    }
}

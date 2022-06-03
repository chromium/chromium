// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.drawable.LevelListDrawable;
import android.util.AttributeSet;
import android.view.View;
import android.widget.ImageView;
import android.widget.RelativeLayout;
import android.widget.TextView;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.ntp.ForeignSessionHelper.ForeignSession;
import org.chromium.components.browser_ui.widget.TintedDrawable;

/**
 * Header view shown above each group of items on the Recent Tabs page. Shows the name of the
 * group (e.g. "Recently closed" or "Jim's Laptop"), an icon, last synced time, and a button to
 * expand or collapse the group.
 */
public class RecentTabsGroupView extends RelativeLayout {

    /** Drawable levels for the device type icon and the expand/collapse arrow. */
    private static final int DRAWABLE_LEVEL_COLLAPSED = 0;
    private static final int DRAWABLE_LEVEL_EXPANDED = 1;

    private RecentTabsGroupView mRow;
    private ImageView mExpandCollapseIcon;
    private TextView mDeviceLabel;
    private TextView mTimeLabel;

    /**
     * Constructor for inflating from XML.
     *
     * @param context The context this view will work in.
     * @param attrs The attribute set for this view.
     */
    public RecentTabsGroupView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    public void onFinishInflate() {
        super.onFinishInflate();
        mRow = getRootView().findViewById(R.id.recent_tabs_group_view);
        mTimeLabel = (TextView) findViewById(R.id.time_label);
        mDeviceLabel = (TextView) findViewById(R.id.device_label);
        mExpandCollapseIcon = (ImageView) findViewById(R.id.expand_collapse_icon);

        // Create drawable for expand/collapse arrow.
        LevelListDrawable collapseIcon = new LevelListDrawable();
        collapseIcon.addLevel(DRAWABLE_LEVEL_COLLAPSED, DRAWABLE_LEVEL_COLLAPSED,
                TintedDrawable.constructTintedDrawable(
                        getContext(), R.drawable.ic_expand_more_black_24dp));
        TintedDrawable collapse = TintedDrawable.constructTintedDrawable(
                getContext(), R.drawable.ic_expand_less_black_24dp);
        collapseIcon.addLevel(DRAWABLE_LEVEL_EXPANDED, DRAWABLE_LEVEL_EXPANDED, collapse);
        mExpandCollapseIcon.setImageDrawable(collapseIcon);
    }

    /**
     * Configures the view for a foreign session.
     *
     * @param session The session to configure the view for.
     * @param isExpanded Whether the view is expanded or collapsed.
     */
    public void configureForForeignSession(ForeignSession session, boolean isExpanded) {
        mDeviceLabel.setText(session.name);
        mTimeLabel.setVisibility(View.VISIBLE);
        mTimeLabel.setText(getTimeString(session));
        setGroupViewHeight(true);
        configureExpandedCollapsed(isExpanded);
    }

    /**
     * Configures the view for the recently closed tabs group.
     *
     * @param isExpanded Whether the view is expanded or collapsed.
     */
    public void configureForRecentlyClosedTabs(boolean isExpanded) {
        mDeviceLabel.setText(R.string.recently_closed);
        mTimeLabel.setVisibility(View.GONE);
        setGroupViewHeight(false);
        configureExpandedCollapsed(isExpanded);
    }

    /**
     * Configures the view for the promo.
     *
     * @param isExpanded Whether the view is expanded or collapsed.
     */
    public void configureForPromo(boolean isExpanded) {
        mDeviceLabel.setText(R.string.ntp_recent_tabs_sync_promo_title);
        mTimeLabel.setVisibility(View.GONE);
        setGroupViewHeight(false);
        configureExpandedCollapsed(isExpanded);
    }

    private void configureExpandedCollapsed(boolean isExpanded) {
        String description =
                getResources().getString(isExpanded ? R.string.accessibility_collapse_section_header
                                                    : R.string.accessibility_expand_section_header);
        mExpandCollapseIcon.setContentDescription(description);

        int level = isExpanded ? DRAWABLE_LEVEL_EXPANDED : DRAWABLE_LEVEL_COLLAPSED;
        mExpandCollapseIcon.getDrawable().setLevel(level);
    }

    private void setGroupViewHeight(boolean isTimeLabelVisible) {
        mRow.getLayoutParams().height = getResources().getDimensionPixelOffset(isTimeLabelVisible
                        ? R.dimen.recent_tabs_foreign_session_group_item_height
                        : R.dimen.recent_tabs_default_group_item_height);
    }

    private CharSequence getTimeString(ForeignSession session) {
        long timeDeltaMs = System.currentTimeMillis() - session.modifiedTime;
        if (timeDeltaMs < 0) timeDeltaMs = 0;

        int daysElapsed = (int) (timeDeltaMs / (24L * 60L * 60L * 1000L));
        int hoursElapsed = (int) (timeDeltaMs / (60L * 60L * 1000L));
        int minutesElapsed = (int) (timeDeltaMs / (60L * 1000L));

        Resources res = getResources();
        String relativeTime;
        if (daysElapsed > 0L) {
            relativeTime = res.getQuantityString(R.plurals.n_days_ago, daysElapsed, daysElapsed);
        } else if (hoursElapsed > 0L) {
            relativeTime = res.getQuantityString(R.plurals.n_hours_ago, hoursElapsed, hoursElapsed);
        } else if (minutesElapsed > 0L) {
            relativeTime = res.getQuantityString(R.plurals.n_minutes_ago, minutesElapsed,
                    minutesElapsed);
        } else {
            relativeTime = res.getString(R.string.just_now);
        }

        return getResources().getString(R.string.ntp_recent_tabs_last_synced, relativeTime);
    }
}

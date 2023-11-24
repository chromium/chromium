// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.pwd_migration;

import android.content.Context;
import android.util.AttributeSet;
import android.view.View;
import android.widget.RelativeLayout;
import android.widget.ScrollView;

/** The content of the PasswordMigrationWarning that needs to be scrollable. */
public class ScrollablePasswordMigrationWarningContent extends RelativeLayout {
    private ScrollView mScrollView;

    public ScrollablePasswordMigrationWarningContent(Context context) {
        this(context, null);
    }

    public ScrollablePasswordMigrationWarningContent(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    /**
     * Called to report to the {@link BottomSheet} how much the {@link ScrollView} on the sheet has
     * been scrolled.
     *
     * @return The vertical scroll offset of the sheet content.
     */
    int getVerticalScrollOffset() {
        View v = findViewById(R.id.touch_to_fill_sheet_header_image);
        return v == null ? 0 : -(v.getTop() - mScrollView.getPaddingTop());
    }

    @Override
    public void onFinishInflate() {
        super.onFinishInflate();
        mScrollView = findViewById(R.id.pwd_migration_warning_scroll_view);
    }
}

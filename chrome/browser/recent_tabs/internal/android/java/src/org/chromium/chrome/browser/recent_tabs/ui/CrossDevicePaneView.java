// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.recent_tabs.ui;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.util.AttributeSet;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.ListView;
import android.widget.TextView;

import androidx.annotation.Nullable;
import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.chrome.browser.recent_tabs.R;
import org.chromium.ui.base.DeviceFormFactor;

/** Conditionally displays empty state for cross device pane. */
public class CrossDevicePaneView extends FrameLayout {
    private View mEmptyStateContainer;
    private ListView mListView;

    /** Constructor for inflation. */
    public CrossDevicePaneView(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        Context context = getContext();
        mEmptyStateContainer = findViewById(R.id.empty_state_container);
        mListView = findViewById(R.id.cross_device_list_view);

        setupEmptyState(context);
    }

    public void setEmptyStateVisible(boolean visible) {
        mEmptyStateContainer.setVisibility(visible ? View.VISIBLE : View.INVISIBLE);
        mListView.setVisibility(visible ? View.INVISIBLE : View.VISIBLE);
    }

    /** Return the cross device list view. */
    public ListView getListView() {
        return mListView;
    }

    private void setupEmptyState(Context context) {
        ImageView emptyStateIllustration = findViewById(R.id.empty_state_icon);
        int emptyViewImageResId =
                DeviceFormFactor.isNonMultiDisplayContextOnTablet(context)
                        ? R.drawable.tablet_recent_tab_empty_state_illustration
                        : R.drawable.phone_recent_tab_empty_state_illustration;
        Drawable illustration = AppCompatResources.getDrawable(context, emptyViewImageResId);
        emptyStateIllustration.setImageDrawable(illustration);

        TextView emptyStateHeading = findViewById(R.id.empty_state_text_title);
        emptyStateHeading.setText(R.string.recent_tabs_no_tabs_empty_state);
        TextView emptyStateSubheading = findViewById(R.id.empty_state_text_description);
        emptyStateSubheading.setText(R.string.recent_tabs_sign_in_on_other_devices);
    }
}

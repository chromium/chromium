// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.graphics.drawable.Drawable;
import android.util.AttributeSet;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.annotation.Px;
import androidx.appcompat.content.res.AppCompatResources;
import androidx.core.util.Consumer;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;
import androidx.recyclerview.widget.RecyclerView.OnScrollListener;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.browser_ui.widget.displaystyle.UiConfig;
import org.chromium.components.browser_ui.widget.selectable_list.SelectableListLayout;

/** Conditionally displays empty state for the tab group pane. */
@NullMarked
public class TabGroupListView extends FrameLayout {
    private RecyclerView mRecyclerView;
    private View mEmptyStateContainer;
    private TextView mEmptyStateSubheading;
    private UiConfig mUiConfig;
    // Effectively final once set.
    private boolean mEnableContainment;

    /** Constructor for inflation. */
    public TabGroupListView(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        Context context = getContext();
        mRecyclerView = findViewById(R.id.tab_group_list_recycler_view);
        mRecyclerView.setLayoutManager(new LinearLayoutManager(context));

        mEmptyStateContainer = findViewById(R.id.empty_state_container);

        ImageView emptyStateIllustration = findViewById(R.id.empty_state_icon);
        Drawable illustration =
                AppCompatResources.getDrawable(
                        context, R.drawable.tab_group_list_empty_state_illustration);
        emptyStateIllustration.setImageDrawable(illustration);

        TextView emptyStateHeading = findViewById(R.id.empty_state_text_title);
        emptyStateHeading.setText(R.string.tab_groups_empty_header);
        mEmptyStateSubheading = findViewById(R.id.empty_state_text_description);

        mUiConfig = new UiConfig(this);
        mUiConfig.addObserver(this::onDisplayStyleChanged);
    }

    void setRecyclerViewAdapter(RecyclerView.Adapter adapter) {
        mRecyclerView.setAdapter(adapter);
    }

    void setOnIsScrolledChanged(Consumer<Boolean> onIsScrolledMaybeChanged) {
        // Layout listener is for when items are removed that stops the view from being scrollable.
        mRecyclerView.addOnLayoutChangeListener(
                (view, i, i1, i2, i3, i4, i5, i6, i7) ->
                        onIsScrolledMaybeChanged.accept(
                                mRecyclerView.computeVerticalScrollOffset() != 0));
        mRecyclerView.addOnScrollListener(
                new OnScrollListener() {
                    @Override
                    public void onScrolled(RecyclerView recyclerView, int dx, int dy) {
                        onIsScrolledMaybeChanged.accept(
                                mRecyclerView.computeVerticalScrollOffset() != 0);
                    }
                });
    }

    void setEnableContainment(boolean enabled) {
        mEnableContainment = enabled;

        if (enabled) {
            mRecyclerView.addItemDecoration(new TabGroupListItemDecoration(getContext()));
        }
        onDisplayStyleChanged(mUiConfig.getCurrentDisplayStyle());
    }

    void setEmptyStateVisible(boolean visible) {
        mEmptyStateContainer.setVisibility(visible ? View.VISIBLE : View.INVISIBLE);
        mRecyclerView.setVisibility(visible ? View.INVISIBLE : View.VISIBLE);
    }

    void setSyncEnabled(boolean enabled) {
        mEmptyStateSubheading.setText(
                enabled
                        ? R.string.tab_groups_empty_state_description
                        : R.string.tab_groups_empty_state_description_no_sync);
    }

    View getRecyclerView() {
        return mRecyclerView;
    }

    private void onDisplayStyleChanged(UiConfig.DisplayStyle newDisplayStyle) {
        Resources res = getResources();
        int padding =
                SelectableListLayout.getPaddingForDisplayStyle(newDisplayStyle, mRecyclerView, res);
        if (mEnableContainment) {
            final @Px int minPadding =
                    res.getDimensionPixelSize(R.dimen.tab_group_recycler_view_min_padding);
            padding = Math.max(padding, minPadding);
        }
        mRecyclerView.setPaddingRelative(
                padding, mRecyclerView.getPaddingTop(), padding, mRecyclerView.getPaddingBottom());
    }

    @Override
    protected void onConfigurationChanged(Configuration newConfig) {
        if (mUiConfig != null) mUiConfig.updateDisplayStyle();
    }
}

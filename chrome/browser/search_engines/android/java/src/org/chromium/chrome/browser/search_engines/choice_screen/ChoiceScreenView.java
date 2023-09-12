// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.search_engines.choice_screen;

import android.content.Context;
import android.util.AttributeSet;
import android.view.View;
import android.view.ViewTreeObserver;
import android.widget.LinearLayout;

import androidx.annotation.Nullable;
import androidx.recyclerview.widget.DividerItemDecoration;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.chrome.browser.search_engines.R;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;

/** The {@link View} responsible for rendering the search engine choice screen. */
class ChoiceScreenView extends LinearLayout {
    private RecyclerView mRecyclerView;
    private View mDivider;

    public ChoiceScreenView(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        LinearLayoutManager layoutManager = new LinearLayoutManager(getContext());
        mRecyclerView = findViewById(R.id.choice_screen_list);
        mRecyclerView.setLayoutManager(layoutManager);
        mRecyclerView.addItemDecoration(
                new DividerItemDecoration(getContext(), layoutManager.getOrientation()));

        mDivider = findViewById(R.id.choice_screen_divider);

        // TODO(b/294837151): To be also called on configuration change and when the height of the
        // RecyclerView changes.
        scheduleDividerVisibilityUpdate();
    }

    void setItemsAdapter(@Nullable SimpleRecyclerViewAdapter adapter) {
        mRecyclerView.setAdapter(adapter);
    }

    private void scheduleDividerVisibilityUpdate() {
        mRecyclerView.getViewTreeObserver().addOnGlobalLayoutListener(
                new ViewTreeObserver.OnGlobalLayoutListener() {
                    @Override
                    public void onGlobalLayout() {
                        // At this point the layout is complete, the dimensions of recyclerView and
                        // of child views are known.
                        mRecyclerView.getViewTreeObserver().removeOnGlobalLayoutListener(this);

                        boolean isListScrollable = mRecyclerView.canScrollVertically(1)
                                || mRecyclerView.canScrollVertically(-1);
                        mDivider.setVisibility(isListScrollable ? View.VISIBLE : View.INVISIBLE);
                    }
                });
    }
}

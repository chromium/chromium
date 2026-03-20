// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.search_engines.settings.common;

import static org.chromium.build.NullUtil.assertNonNull;

import android.content.Context;
import android.content.res.TypedArray;
import android.graphics.Canvas;
import android.graphics.Rect;
import android.graphics.drawable.Drawable;
import android.util.AttributeSet;
import android.view.View;

import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.build.annotations.NullMarked;

/** Custom RecyclerView that displays a list of items with dividers. */
@NullMarked
public class PreferenceListRecyclerView extends RecyclerView {
    public PreferenceListRecyclerView(Context context, AttributeSet attrs) {
        super(context, attrs);
        LinearLayoutManager layoutManager = new LinearLayoutManager(context);
        setLayoutManager(layoutManager);
        addItemDecoration(new PreferenceListDividerItemDecoration(context));
    }

    private static class PreferenceListDividerItemDecoration extends ItemDecoration {
        private static final int[] ATTRS = new int[] {android.R.attr.listDivider};
        private final Drawable mDivider;
        private final int mDividerHeight;

        PreferenceListDividerItemDecoration(Context context) {
            final TypedArray a = context.obtainStyledAttributes(ATTRS);
            mDivider = assertNonNull(a.getDrawable(0));
            mDividerHeight = mDivider.getIntrinsicHeight();
            a.recycle();
        }

        @Override
        public void getItemOffsets(Rect outRect, View view, RecyclerView parent, State state) {
            int position = parent.getChildAdapterPosition(view);
            if (position == state.getItemCount() - 1) {
                outRect.bottom = 0;
            } else {
                outRect.bottom = mDividerHeight;
            }
        }

        @Override
        public void onDraw(Canvas c, RecyclerView parent, State state) {
            int left = parent.getPaddingLeft();
            int right = parent.getWidth() - parent.getPaddingRight();

            for (int i = 0; i < parent.getChildCount(); i++) {
                View child = parent.getChildAt(i);
                int position = parent.getChildAdapterPosition(child);

                if (position == state.getItemCount() - 1) {
                    continue;
                }

                LayoutParams params = (LayoutParams) child.getLayoutParams();
                int top = child.getBottom() + params.bottomMargin;
                int bottom = top + mDividerHeight;
                mDivider.setBounds(left, top, right, bottom);
                mDivider.draw(c);
            }
        }
    }
}

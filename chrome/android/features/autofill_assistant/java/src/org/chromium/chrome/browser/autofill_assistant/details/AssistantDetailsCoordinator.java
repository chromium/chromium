// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant.details;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Rect;
import android.graphics.drawable.Drawable;
import android.support.annotation.VisibleForTesting;
import android.view.View;
import android.view.ViewGroup;

import androidx.appcompat.content.res.AppCompatResources;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.chrome.autofill_assistant.R;
import org.chromium.chrome.browser.image_fetcher.ImageFetcher;

/**
 * Coordinator responsible for showing details.
 */
public class AssistantDetailsCoordinator {
    private final RecyclerView mView;
    private final AssistantDetailsAdapter mAdapter;

    public AssistantDetailsCoordinator(
            Context context, AssistantDetailsModel model, ImageFetcher imageFetcher) {
        mView = new RecyclerView(context);
        mView.setLayoutParams(new ViewGroup.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));
        mView.setLayoutManager(new LinearLayoutManager(
                context, LinearLayoutManager.VERTICAL, /* reverseLayout= */ false));
        mView.setBackgroundResource(R.drawable.autofill_assistant_details_bg);
        mView.addItemDecoration(new DetailsItemDecoration(context));
        mAdapter = new AssistantDetailsAdapter(context, imageFetcher);
        mView.setAdapter(mAdapter);

        // Listen to the model and set the details on the adapter when they change.
        model.addObserver((source, propertyKey) -> {
            if (propertyKey == AssistantDetailsModel.DETAILS) {
                mAdapter.setDetails(model.get(AssistantDetailsModel.DETAILS));
            }
        });

        // Observe the details and make this coordinator's view visible only if the details contain
        // at least one item. That way its margins (set by the AssistantBottomBar Coordinator) will
        // be considered during layout only if there is at least one details to show.
        mView.setVisibility(View.GONE);
        model.addObserver((source, propertyKey) -> {
            if (propertyKey == AssistantDetailsModel.DETAILS) {
                int visibility = model.get(AssistantDetailsModel.DETAILS).size() > 0 ? View.VISIBLE
                                                                                     : View.GONE;
                if (mView.getVisibility() != visibility) {
                    mView.setVisibility(visibility);
                }
            }
        });
    }

    public RecyclerView getView() {
        return mView;
    }

    @VisibleForTesting
    public boolean isRunningPlaceholdersAnimationForTesting() {
        return mAdapter.isRunningPlaceholdersAnimation();
    }

    /** A divider that is drawn after each details, except the last one. */
    private static class DetailsItemDecoration extends RecyclerView.ItemDecoration {
        private final Drawable mDrawable;
        private final Rect mBounds = new Rect();

        public DetailsItemDecoration(Context context) {
            mDrawable = AppCompatResources.getDrawable(
                    context, R.drawable.autofill_assistant_details_list_divider);
        }

        @Override
        public void onDrawOver(Canvas canvas, RecyclerView parent, RecyclerView.State state) {
            // Note: this implementation is inspired from DividerItemDecoration#drawVertical.
            int left = parent.getPaddingLeft();
            int right = parent.getWidth() - parent.getPaddingRight();
            int childCount = parent.getChildCount();

            // Draw a divider after each child, except the last one.
            for (int i = 0; i < childCount - 1; ++i) {
                View child = parent.getChildAt(i);
                parent.getDecoratedBoundsWithMargins(child, this.mBounds);
                int bottom = this.mBounds.bottom + Math.round(child.getTranslationY());
                int top = bottom - this.mDrawable.getIntrinsicHeight();
                this.mDrawable.setBounds(left, top, right, bottom);
                this.mDrawable.draw(canvas);
            }
        }
    }
}

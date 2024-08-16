// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.TextView;

import org.chromium.base.Callback;
import org.chromium.chrome.tab_ui.R;
import org.chromium.ui.modelutil.ListObservable;
import org.chromium.ui.modelutil.ListObservable.ListObserver;

/**
 * Empty coordinator that is responsible for showing an empty state view in tab switcher when we are
 * in no tab state.
 */
// @TODO(crbug.com/40910476) Add instrumentation test for TabListEmptyCoordinator class.
class TabListEmptyCoordinator {
    private ViewGroup mRootView;
    private View mEmptyView;
    private TextView mEmptyStateHeading;
    private TextView mEmptyStateSubheading;
    private ImageView mImageView;
    private Context mContext;
    private TabListModel mModel;
    private ListObserver<Void> mListObserver;
    private Callback<Runnable> mRunOnItemAnimatorFinished;
    private boolean mIsTabSwitcherShowing;
    private boolean mIsListObserverAttached;

    public TabListEmptyCoordinator(
            ViewGroup rootView, TabListModel model, Callback<Runnable> runOnItemAnimatorFinished) {
        mRootView = rootView;
        mContext = rootView.getContext();
        mRunOnItemAnimatorFinished = runOnItemAnimatorFinished;

        // Observe TabListModel to determine when to add / remove empty state view.
        mModel = model;
        mListObserver =
                new ListObserver<Void>() {
                    @Override
                    public void onItemRangeInserted(ListObservable source, int index, int count) {
                        updateEmptyView();
                    }

                    @Override
                    public void onItemRangeRemoved(ListObservable source, int index, int count) {
                        updateEmptyView();
                    }
                };
    }

    public void initializeEmptyStateView(
            int imageResId, int emptyHeadingStringResId, int emptySubheadingStringResId) {
        if (mEmptyView != null) {
            return;
        }
        // Initialize empty state resource.
        mEmptyView =
                (ViewGroup)
                        android.view.LayoutInflater.from(mContext)
                                .inflate(R.layout.empty_state_view, null);
        mEmptyStateHeading = mEmptyView.findViewById(R.id.empty_state_text_title);
        mEmptyStateSubheading = mEmptyView.findViewById(R.id.empty_state_text_description);
        mImageView = mEmptyView.findViewById(R.id.empty_state_icon);

        // Set empty state properties.
        setEmptyStateImageRes(imageResId);
        setEmptyStateViewText(emptyHeadingStringResId, emptySubheadingStringResId);
    }

    private void setEmptyStateViewText(
            int emptyHeadingStringResId, int emptySubheadingStringResId) {
        mEmptyStateHeading.setText(emptyHeadingStringResId);
        mEmptyStateSubheading.setText(emptySubheadingStringResId);
    }

    private void setEmptyStateImageRes(int imageResId) {
        mImageView.setImageResource(imageResId);
    }

    private boolean isEmptyViewAttached() {
        return mEmptyView != null && mEmptyView.getParent() != null;
    }

    private boolean isInEmptyState() {
        return mModel.size() == 0 && mIsTabSwitcherShowing;
    }

    private void updateEmptyView() {
        if (isEmptyViewAttached()) {
            if (isInEmptyState()) {
                mRunOnItemAnimatorFinished.onResult(
                        () -> {
                            // Re-check requirements since this is now async.
                            if (isEmptyViewAttached() && isInEmptyState()) {
                                setEmptyViewVisibility(View.VISIBLE);
                            }
                        });
            } else {
                setEmptyViewVisibility(View.GONE);
            }
        }
    }

    public void setIsTabSwitcherShowing(boolean isShowing) {
        mIsTabSwitcherShowing = isShowing;
        if (mIsTabSwitcherShowing) {
            attachListObserver();
            updateEmptyView();
        } else {
            updateEmptyView();
            removeListObserver();
        }
    }

    public void attachListObserver() {
        if (mListObserver != null && !getIsListObserverAttached()) {
            mModel.addObserver(mListObserver);
            mIsListObserverAttached = true;
        }
    }

    public void removeListObserver() {
        if (mListObserver != null && getIsListObserverAttached()) {
            mModel.removeObserver(mListObserver);
            mIsListObserverAttached = false;
        }
    }

    public void attachEmptyView() {
        if (mEmptyView != null && mEmptyView.getParent() == null) {
            mRootView.addView(mEmptyView);
        }
        setEmptyViewVisibility(View.GONE);
    }

    public void destroyEmptyView() {
        if (mEmptyView != null && mEmptyView.getParent() != null) {
            mRootView.removeView(mEmptyView);
        }
        mEmptyView = null;
    }

    public void setEmptyViewVisibility(int isVisible) {
        mEmptyView.setVisibility(isVisible);
    }

    private boolean getIsListObserverAttached() {
        return mIsListObserverAttached;
    }
}

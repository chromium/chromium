// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.annotation.DrawableRes;
import androidx.annotation.StringRes;

import org.chromium.base.Callback;
import org.chromium.build.annotations.EnsuresNonNullIf;
import org.chromium.build.annotations.Initializer;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.tab_ui.R;
import org.chromium.ui.modelutil.ListObservable;
import org.chromium.ui.modelutil.ListObservable.ListObserver;

/**
 * Empty coordinator that is responsible for showing an empty state view in tab switcher when we are
 * in no tab state.
 */
// @TODO(crbug.com/40910476) Add instrumentation test for TabListEmptyCoordinator class.
@NullMarked
class TabListEmptyCoordinator {
    public final long ILLUSTRATION_ANIMATION_DURATION_MS = 700L;

    private final ViewGroup mRootView;
    private final Context mContext;
    private final TabListModel mModel;
    private final ListObserver<Void> mListObserver;
    private final Callback<Runnable> mRunOnItemAnimatorFinished;

    private @Nullable View mEmptyView;
    private TextView mEmptyStateHeading;
    private TextView mEmptyStateSubheading;
    private ImageView mImageView;
    private boolean mIsTabSwitcherShowing;
    private boolean mIsListObserverAttached;
    private @Nullable TabListEmptyIllustrationAnimationManager mIllustrationAnimationManager;

    public TabListEmptyCoordinator(
            ViewGroup rootView, TabListModel model, Callback<Runnable> runOnItemAnimatorFinished) {
        mRootView = rootView;
        mContext = rootView.getContext();
        mRunOnItemAnimatorFinished = runOnItemAnimatorFinished;

        // Observe TabListModel to determine when to add / remove empty state view.
        mModel = model;
        mListObserver =
                new ListObserver<>() {
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

    @Initializer
    public void initializeEmptyStateView(
            @DrawableRes int imageResId,
            @StringRes int emptyHeadingStringResId,
            @StringRes int emptySubheadingStringResId) {
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

        mIllustrationAnimationManager = tryGetAnimationManager(imageResId);
        transformIllustrationIfPresent();
    }

    private @Nullable TabListEmptyIllustrationAnimationManager tryGetAnimationManager(
            @DrawableRes int imageResId) {
        return isDrawableForPhones(imageResId)
                        && ChromeFeatureList.sEmptyTabListAnimationKillSwitch.isEnabled()
                ? new PhoneTabListEmptyIllustrationAnimationManager(
                        mImageView, mEmptyStateHeading, mEmptyStateSubheading)
                : null;
    }

    private void setEmptyStateViewText(
            int emptyHeadingStringResId, int emptySubheadingStringResId) {
        mEmptyStateHeading.setText(emptyHeadingStringResId);
        mEmptyStateSubheading.setText(emptySubheadingStringResId);
    }

    private void setEmptyStateImageRes(int imageResId) {
        mImageView.setImageResource(imageResId);
    }

    @EnsuresNonNullIf("mEmptyView")
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
                                if (mIllustrationAnimationManager != null) {
                                    mIllustrationAnimationManager.animate(
                                            ILLUSTRATION_ANIMATION_DURATION_MS);
                                }
                                setEmptyViewVisibility(View.VISIBLE);
                            }
                        });
            } else {
                setEmptyViewVisibility(View.GONE);
                transformIllustrationIfPresent();
            }
        }
    }

    private void transformIllustrationIfPresent() {
        if (mIllustrationAnimationManager != null) {
            mIllustrationAnimationManager.initialTransformation();
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
            setEmptyViewVisibility(View.GONE);
        }
    }

    public void destroyEmptyView() {
        if (mEmptyView != null && mEmptyView.getParent() != null) {
            mRootView.removeView(mEmptyView);
        }
        mEmptyView = null;
    }

    private void setEmptyViewVisibility(int isVisible) {
        assumeNonNull(mEmptyView).setVisibility(isVisible);
    }

    private boolean getIsListObserverAttached() {
        return mIsListObserverAttached;
    }

    private boolean isDrawableForPhones(@DrawableRes int drawableResId) {
        return drawableResId == R.drawable.phone_tab_switcher_empty_state_illustration;
    }
}

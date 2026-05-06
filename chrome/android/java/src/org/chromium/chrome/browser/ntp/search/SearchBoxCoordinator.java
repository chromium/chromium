// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp.search;

import android.content.Context;
import android.graphics.Point;
import android.graphics.Rect;
import android.text.TextWatcher;
import android.view.View;
import android.view.View.OnDragListener;
import android.view.ViewGroup;
import android.view.ViewStub;

import androidx.annotation.Px;
import androidx.annotation.StyleRes;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.feed.FeedSurfaceScrollDelegate;
import org.chromium.chrome.browser.lens.LensEntryPoint;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.ntp.NewTabPageManager;
import org.chromium.chrome.browser.omnibox.status.StatusProperties.StatusIconResource;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.ViewUtils;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.function.Supplier;

/**
 * This class is responsible for reacting to events from the outside world, interacting with other
 * coordinators, running most of the business logic associated with the fake search box component,
 * and updating the model accordingly.
 */
@NullMarked
public class SearchBoxCoordinator implements NtpSearchBox {
    private final PropertyModel mModel;
    private final SearchBoxContainerView mView;
    private final SearchBoxMediator mMediator;
    private final boolean mIsIncognito;
    private final WindowAndroid mWindowAndroid;

    public SearchBoxCoordinator(
            Context context,
            ViewStub viewStub,
            boolean isTablet,
            ActivityLifecycleDispatcher activityLifecycleDispatcher,
            boolean isIncognito,
            WindowAndroid windowAndroid,
            NewTabPageManager newTabPageManager,
            Profile profile) {
        mModel = new PropertyModel(SearchBoxProperties.ALL_KEYS);
        mView = ViewUtils.replace(viewStub, R.layout.fake_search_box_layout);
        mMediator =
                new SearchBoxMediator(
                        context,
                        mModel,
                        mView,
                        isTablet,
                        activityLifecycleDispatcher,
                        newTabPageManager,
                        isIncognito,
                        windowAndroid,
                        profile);
        mIsIncognito = isIncognito;
        mWindowAndroid = windowAndroid;
    }

    @Override
    public View getView() {
        return mView;
    }

    @Override
    public void destroy() {
        mMediator.onDestroy();
    }

    @Override
    public void setAlpha(float alpha) {
        mModel.set(SearchBoxProperties.ALPHA, alpha);
    }

    @Override
    public void setSearchText(String text) {
        mModel.set(SearchBoxProperties.SEARCH_TEXT, text);
    }

    @Override
    public void setSearchBoxDragListener(OnDragListener listener) {
        mMediator.setSearchBoxDragListener(listener);
    }

    @Override
    public void setSearchBoxTextWatcher(TextWatcher textWatcher) {
        mModel.set(SearchBoxProperties.SEARCH_BOX_TEXT_WATCHER, textWatcher);
    }

    @Override
    public void setVoiceSearchButtonVisibility(boolean visible) {
        mModel.set(SearchBoxProperties.VOICE_SEARCH_VISIBILITY, visible);
    }

    @Override
    public void setLensButtonVisibility(boolean visible) {
        mModel.set(SearchBoxProperties.LENS_VISIBILITY, visible);
    }

    @Override
    public boolean isLensEnabled(@LensEntryPoint int lensEntryPoint) {
        return mMediator.isLensEnabled(
                lensEntryPoint, mIsIncognito, DeviceFormFactor.isWindowOnTablet(mWindowAndroid));
    }

    @Override
    public void setHeight(@Px int height) {
        mMediator.setHeight(height);
    }

    @Override
    public void setTopMargin(@Px int topMargin) {
        mMediator.setTopMargin(topMargin);
    }

    @Override
    public void setEndPadding(@Px int endPadding) {
        mMediator.setEndPadding(endPadding);
    }

    @Override
    public void setSearchBoxTextAppearance(@StyleRes int resId) {
        mMediator.setSearchBoxTextAppearance(resId);
    }

    @Override
    public void enableSearchBoxEditText(boolean enabled) {
        mMediator.enableSearchBoxEditText(enabled);
    }

    @Override
    public void setSearchBoxHintText(@Nullable String hint) {
        mMediator.setSearchBoxHintText(hint);
    }

    @Override
    public void setSearchEngineIcon(@Nullable StatusIconResource icon) {
        mMediator.setSearchEngineIcon(icon);
    }

    @Override
    public void applyWhiteBackground(boolean apply) {
        mMediator.applyWhiteBackground(apply);
    }

    @Override
    public float getToolbarTransitionPercentage(
            FeedSurfaceScrollDelegate scrollDelegate,
            @Nullable Supplier<Integer> tabStripHeightSupplier,
            @Px int currentNtpFakeSearchBoxTransitionStartOffset) {
        return mMediator.getToolbarTransitionPercentage(
                scrollDelegate,
                tabStripHeightSupplier,
                currentNtpFakeSearchBoxTransitionStartOffset);
    }

    @Override
    public void getSearchBoxBounds(
            Rect bounds,
            Point translation,
            View parentView,
            FeedSurfaceScrollDelegate scrollDelegate,
            @Px int searchBoxBoundsVerticalInset) {
        mMediator.getSearchBoxBounds(
                bounds, translation, parentView, scrollDelegate, searchBoxBoundsVerticalInset);
    }

    @Override
    public void setLayoutWidth(@Px int widthPx) {
        ViewGroup.MarginLayoutParams marginLayoutParams =
                (ViewGroup.MarginLayoutParams) mView.getLayoutParams();
        if (marginLayoutParams.width != widthPx
                || marginLayoutParams.leftMargin != 0
                || marginLayoutParams.rightMargin != 0) {
            marginLayoutParams.width = widthPx;
            marginLayoutParams.leftMargin = 0;
            marginLayoutParams.rightMargin = 0;
        }
    }
}

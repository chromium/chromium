// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp.search;

import android.content.Context;
import android.text.TextWatcher;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.View.OnDragListener;
import android.view.ViewGroup;

import androidx.annotation.StyleRes;

import org.chromium.build.annotations.Initializer;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.lens.LensEntryPoint;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * This class is responsible for reacting to events from the outside world, interacting with other
 * coordinators, running most of the business logic associated with the fake search box component,
 * and updating the model accordingly.
 */
@NullMarked
public class SearchBoxCoordinator {
    private final PropertyModel mModel;
    private final ViewGroup mView;
    private final SearchBoxMediator mMediator;
    private boolean mIsIncognito;
    private WindowAndroid mWindowAndroid;

    /** Constructor. */
    public SearchBoxCoordinator(Context context, ViewGroup parent) {
        mModel = new PropertyModel(SearchBoxProperties.ALL_KEYS);
        mView = parent.findViewById(R.id.search_box);
        mMediator = new SearchBoxMediator(context, mModel, mView);
    }

    @Initializer
    public void initialize(
            ActivityLifecycleDispatcher activityLifecycleDispatcher,
            boolean isIncognito,
            WindowAndroid windowAndroid) {
        mMediator.initialize(activityLifecycleDispatcher);
        mIsIncognito = isIncognito;
        mWindowAndroid = windowAndroid;
    }

    public View getView() {
        return mView;
    }

    public void destroy() {
        mMediator.onDestroy();
    }

    public void setAlpha(float alpha) {
        mModel.set(SearchBoxProperties.ALPHA, alpha);
    }

    public void setVisibility(boolean visible) {
        mModel.set(SearchBoxProperties.VISIBILITY, visible);
    }

    public void setSearchText(String text) {
        mModel.set(SearchBoxProperties.SEARCH_TEXT, text);
    }

    public void setSearchBoxClickListener(OnClickListener listener) {
        mMediator.setSearchBoxClickListener(listener);
    }

    public void setSearchBoxDragListener(OnDragListener listener) {
        mMediator.setSearchBoxDragListener(listener);
    }

    public void setSearchBoxTextWatcher(TextWatcher textWatcher) {
        mModel.set(SearchBoxProperties.SEARCH_BOX_TEXT_WATCHER, textWatcher);
    }

    public void setVoiceSearchButtonVisibility(boolean visible) {
        mModel.set(SearchBoxProperties.VOICE_SEARCH_VISIBILITY, visible);
    }

    public void addVoiceSearchButtonClickListener(OnClickListener listener) {
        mMediator.addVoiceSearchButtonClickListener(listener);
    }

    public void setLensButtonVisibility(boolean visible) {
        mModel.set(SearchBoxProperties.LENS_VISIBILITY, visible);
    }

    public void addLensButtonClickListener(OnClickListener listener) {
        mMediator.addLensButtonClickListener(listener);
    }

    public void setComposeplateButtonVisibility(boolean visible) {
        mModel.set(SearchBoxProperties.COMPOSEPLATE_BUTTON_VISIBILITY, visible);
    }

    public void setComposeplateButtonClickListener(OnClickListener listener) {
        mMediator.setComposeplateButtonClickListener(listener);
    }

    public void setComposeplateButtonIconRawResId(int iconRawResId) {
        mMediator.setComposeplateButtonIconRawResId(iconRawResId);
    }

    public boolean isLensEnabled(@LensEntryPoint int lensEntryPoint) {
        return mMediator.isLensEnabled(
                lensEntryPoint, mIsIncognito, DeviceFormFactor.isWindowOnTablet(mWindowAndroid));
    }

    public void startLens(@LensEntryPoint int lensEntryPoint) {
        mMediator.startLens(lensEntryPoint, mWindowAndroid, mIsIncognito);
    }

    public void setHeight(int height) {
        mMediator.setHeight(height);
    }

    public void setTopMargin(int topMargin) {
        mMediator.setTopMargin(topMargin);
    }

    public void setEndPadding(int endPadding) {
        mMediator.setEndPadding(endPadding);
    }

    public void setStartPadding(int startPadding) {
        mMediator.setStartPadding(startPadding);
    }

    public void setSearchBoxTextAppearance(@StyleRes int resId) {
        mMediator.setSearchBoxTextAppearance(resId);
    }

    public void enableSearchBoxEditText(boolean enabled) {
        mMediator.enableSearchBoxEditText(enabled);
    }

    public void setSearchBoxHintText(@Nullable String hint) {
        mMediator.setSearchBoxHintText(hint);
    }

    public void applyWhiteBackgroundWithShadow(boolean apply) {
        mMediator.applyWhiteBackgroundWithShadow(apply);
    }
}

// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp.search;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.text.TextWatcher;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.View.OnDragListener;
import android.view.ViewGroup;

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

    public View getVoiceSearchButton() {
        return mView.findViewById(R.id.voice_search_button);
    }

    public void destroy() {
        mMediator.onDestroy();
    }

    public void setAlpha(float alpha) {
        mModel.set(SearchBoxProperties.ALPHA, alpha);
    }

    public void setBackground(Drawable background) {
        mModel.set(SearchBoxProperties.BACKGROUND, background);
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

    public boolean isLensEnabled(@LensEntryPoint int lensEntryPoint) {
        return mMediator.isLensEnabled(
                lensEntryPoint, mIsIncognito, DeviceFormFactor.isWindowOnTablet(mWindowAndroid));
    }

    public void startLens(@LensEntryPoint int lensEntryPoint) {
        mMediator.startLens(lensEntryPoint, mWindowAndroid, mIsIncognito);
    }

    public void setIncognitoMode(boolean isIncognito) {
        mIsIncognito = isIncognito;
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

    public void setTextViewTranslationX(float translationX) {
        mMediator.setTextViewTranslationX(translationX);
    }

    public void setSearchTextSize(float textSize) {
        mModel.set(SearchBoxProperties.SEARCH_BOX_TEXT_SIZE, textSize);
    }

    public boolean getIncognitoModeForTesting() {
        return mIsIncognito;
    }
}

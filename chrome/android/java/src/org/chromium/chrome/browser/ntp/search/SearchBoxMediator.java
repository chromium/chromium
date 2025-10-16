// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp.search;

import android.content.Context;
import android.content.res.ColorStateList;
import android.graphics.drawable.Drawable;
import android.view.View.OnClickListener;
import android.view.View.OnDragListener;
import android.view.ViewGroup;

import androidx.annotation.StyleRes;
import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.build.annotations.MonotonicNonNull;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.lens.LensController;
import org.chromium.chrome.browser.lens.LensEntryPoint;
import org.chromium.chrome.browser.lens.LensIntentParams;
import org.chromium.chrome.browser.lens.LensQueryParams;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.DestroyObserver;
import org.chromium.chrome.browser.lifecycle.NativeInitObserver;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

import java.util.ArrayList;
import java.util.List;

@NullMarked
class SearchBoxMediator implements DestroyObserver, NativeInitObserver {
    private final Context mContext;
    private final PropertyModel mModel;
    private final ViewGroup mView;
    private final List<OnClickListener> mVoiceSearchClickListeners = new ArrayList<>();
    private final List<OnClickListener> mLensClickListeners = new ArrayList<>();
    private @MonotonicNonNull OnClickListener mComposeplateButtonClickListener;
    private @Nullable ActivityLifecycleDispatcher mActivityLifecycleDispatcher;

    /** Constructor. */
    SearchBoxMediator(Context context, PropertyModel model, ViewGroup view) {
        mContext = context;
        mModel = model;
        mView = view;
        PropertyModelChangeProcessor.create(mModel, mView, new SearchBoxViewBinder());
    }

    /**
     * Initializes the SearchBoxContainerView with the given params. This must be called for
     * classes that use the SearchBoxContainerView.
     *
     * @param activityLifecycleDispatcher Used to register for lifecycle events.
     */
    void initialize(ActivityLifecycleDispatcher activityLifecycleDispatcher) {
        assert mActivityLifecycleDispatcher == null;
        mActivityLifecycleDispatcher = activityLifecycleDispatcher;
        mActivityLifecycleDispatcher.register(this);
        if (mActivityLifecycleDispatcher.isNativeInitializationFinished()) {
            onFinishNativeInitialization();
        }
    }

    @Override
    public void onDestroy() {
        if (mActivityLifecycleDispatcher != null) {
            mActivityLifecycleDispatcher.unregister(this);
            mActivityLifecycleDispatcher = null;
        }

        mModel.set(SearchBoxProperties.LENS_CLICK_CALLBACK, null);
        mModel.set(SearchBoxProperties.VOICE_SEARCH_CLICK_CALLBACK, null);
        mModel.set(SearchBoxProperties.COMPOSEPLATE_BUTTON_CLICK_CALLBACK, null);
        mModel.set(SearchBoxProperties.VOICE_SEARCH_DRAWABLE, null);
        mModel.set(SearchBoxProperties.SEARCH_BOX_CLICK_CALLBACK, null);
        mModel.set(SearchBoxProperties.SEARCH_BOX_DRAG_CALLBACK, null);
        mModel.set(SearchBoxProperties.SEARCH_BOX_TEXT_WATCHER, null);

        mLensClickListeners.clear();
        mVoiceSearchClickListeners.clear();
    }

    @Override
    public void onFinishNativeInitialization() {
        Drawable drawable = AppCompatResources.getDrawable(mContext, R.drawable.ic_mic_white_24dp);
        mModel.set(SearchBoxProperties.VOICE_SEARCH_DRAWABLE, drawable);

        ColorStateList colorStateList = NtpCustomizationUtils.getSearchBoxIconColorTint(mContext);
        mModel.set(SearchBoxProperties.VOICE_SEARCH_COLOR_STATE_LIST, colorStateList);
    }

    /** Called to set a click listener for the search box. */
    void setSearchBoxClickListener(OnClickListener listener) {
        mModel.set(SearchBoxProperties.SEARCH_BOX_CLICK_CALLBACK, v -> listener.onClick(v));
    }

    /** Called to set a drag listener for the search box. */
    void setSearchBoxDragListener(OnDragListener listener) {
        mModel.set(SearchBoxProperties.SEARCH_BOX_DRAG_CALLBACK, listener);
    }

    /** Called to add a click listener for the voice search button. */
    void addVoiceSearchButtonClickListener(OnClickListener listener) {
        boolean hasExistingListeners = !mVoiceSearchClickListeners.isEmpty();
        mVoiceSearchClickListeners.add(listener);
        if (hasExistingListeners) return;
        mModel.set(
                SearchBoxProperties.VOICE_SEARCH_CLICK_CALLBACK,
                v -> {
                    for (OnClickListener clickListener : mVoiceSearchClickListeners) {
                        clickListener.onClick(v);
                    }
                });
    }

    /** Called to add a click listener for the voice search button. */
    void addLensButtonClickListener(OnClickListener listener) {
        boolean hasExistingListeners = !mLensClickListeners.isEmpty();
        mLensClickListeners.add(listener);
        if (hasExistingListeners) return;
        mModel.set(
                SearchBoxProperties.LENS_CLICK_CALLBACK,
                v -> {
                    for (OnClickListener clickListener : mLensClickListeners) {
                        clickListener.onClick(v);
                    }
                });
    }

    /** Called to set a click listener for the composeplate button. */
    void setComposeplateButtonClickListener(OnClickListener listener) {
        assert mComposeplateButtonClickListener == null;

        mComposeplateButtonClickListener = listener;
        mModel.set(
                SearchBoxProperties.COMPOSEPLATE_BUTTON_CLICK_CALLBACK,
                v -> {
                    mComposeplateButtonClickListener.onClick(v);
                });
    }

    void setComposeplateButtonIconRawResId(int iconRawResId) {
        mModel.set(SearchBoxProperties.COMPOSEPLATE_BUTTON_ICON_RAW_RES_ID, iconRawResId);
    }

    /**
     * Launch the Lens app.
     *
     * @param lensEntryPoint A {@link LensEntryPoint}.
     * @param windowAndroid A {@link WindowAndroid} instance.
     * @param isIncognito Whether the request is from a Incognito tab.
     */
    void startLens(
            @LensEntryPoint int lensEntryPoint, WindowAndroid windowAndroid, boolean isIncognito) {
        LensController.getInstance()
                .startLens(
                        windowAndroid,
                        new LensIntentParams.Builder(lensEntryPoint, isIncognito).build());
    }

    /**
     * Check whether the Lens is enabled for an entry point.
     * @param lensEntryPoint A {@link LensEntryPoint}.
     * @param isIncognito Whether the request is from a Incognito tab.
     * @param isTablet Whether the request is from a tablet.
     * @return Whether the Lens is currently enabled.
     */
    boolean isLensEnabled(
            @LensEntryPoint int lensEntryPoint, boolean isIncognito, boolean isTablet) {
        return LensController.getInstance()
                .isLensEnabled(
                        new LensQueryParams.Builder(lensEntryPoint, isIncognito, isTablet).build());
    }

    void setHeight(int height) {
        mModel.set(SearchBoxProperties.SEARCH_BOX_HEIGHT, height);
    }

    void setTopMargin(int topMargin) {
        mModel.set(SearchBoxProperties.SEARCH_BOX_TOP_MARGIN, topMargin);
    }

    void setEndPadding(int endPadding) {
        mModel.set(SearchBoxProperties.SEARCH_BOX_END_PADDING, endPadding);
    }

    void setStartPadding(int startPadding) {
        mModel.set(SearchBoxProperties.SEARCH_BOX_START_PADDING, startPadding);
    }

    void setSearchBoxTextAppearance(@StyleRes int resId) {
        mModel.set(SearchBoxProperties.SEARCH_BOX_TEXT_STYLE_RES_ID, resId);
    }

    void enableSearchBoxEditText(boolean enabled) {
        mModel.set(SearchBoxProperties.ENABLE_SEARCH_BOX_EDIT_TEXT, enabled);
    }

    void setSearchBoxHintText(@Nullable String hint) {
        mModel.set(SearchBoxProperties.SEARCH_BOX_HINT_TEXT, hint);
    }

    void applyWhiteBackgroundWithShadow(boolean apply) {
        mModel.set(SearchBoxProperties.APPLY_WHITE_BACKGROUND_WITH_SHADOW, apply);
    }
}

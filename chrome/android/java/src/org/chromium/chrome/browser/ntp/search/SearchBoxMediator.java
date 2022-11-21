// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp.search;

import android.content.Context;
import android.content.res.ColorStateList;
import android.graphics.Bitmap;
import android.graphics.drawable.Drawable;
import android.view.View.OnClickListener;
import android.view.ViewGroup;

import androidx.core.graphics.drawable.RoundedBitmapDrawable;

import org.chromium.chrome.browser.gsa.GSAState;
import org.chromium.chrome.browser.lens.LensController;
import org.chromium.chrome.browser.lens.LensEntryPoint;
import org.chromium.chrome.browser.lens.LensIntentParams;
import org.chromium.chrome.browser.lens.LensQueryParams;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.DestroyObserver;
import org.chromium.chrome.browser.lifecycle.NativeInitObserver;
import org.chromium.chrome.browser.omnibox.voice.AssistantVoiceSearchService;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.components.externalauth.ExternalAuthUtils;
import org.chromium.components.signin.AccountManagerFacadeProvider;
import org.chromium.ui.base.ViewUtils;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

import java.util.ArrayList;
import java.util.List;

class SearchBoxMediator
        implements DestroyObserver, NativeInitObserver, AssistantVoiceSearchService.Observer {
    private final Context mContext;
    private final PropertyModel mModel;
    private final ViewGroup mView;
    private final List<OnClickListener> mVoiceSearchClickListeners = new ArrayList<>();
    private final List<OnClickListener> mLensClickListeners = new ArrayList<>();
    private ActivityLifecycleDispatcher mActivityLifecycleDispatcher;
    private AssistantVoiceSearchService mAssistantVoiceSearchService;

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
        if (mAssistantVoiceSearchService != null) {
            mAssistantVoiceSearchService.destroy();
            mAssistantVoiceSearchService = null;
        }

        if (mActivityLifecycleDispatcher != null) {
            mActivityLifecycleDispatcher.unregister(this);
            mActivityLifecycleDispatcher = null;
        }
    }

    @Override
    public void onFinishNativeInitialization() {
        mAssistantVoiceSearchService = new AssistantVoiceSearchService(mContext,
                ExternalAuthUtils.getInstance(), TemplateUrlServiceFactory.get(),
                GSAState.getInstance(), this, SharedPreferencesManager.getInstance(),
                IdentityServicesProvider.get().getIdentityManager(
                        Profile.getLastUsedRegularProfile()),
                AccountManagerFacadeProvider.getInstance());
        onAssistantVoiceSearchServiceChanged();
    }

    @Override
    public void onAssistantVoiceSearchServiceChanged() {
        // Potential race condition between destroy and the observer, see crbug.com/1055274.
        if (mAssistantVoiceSearchService == null) return;

        Drawable drawable = mAssistantVoiceSearchService.getCurrentMicDrawable();
        mModel.set(SearchBoxProperties.VOICE_SEARCH_DRAWABLE, drawable);

        ColorStateList colorStateList = mAssistantVoiceSearchService.getButtonColorStateList(
                BrandedColorScheme.APP_DEFAULT, mContext);
        mModel.set(SearchBoxProperties.VOICE_SEARCH_COLOR_STATE_LIST, colorStateList);
    }

    /** Called to set a click listener for the search box. */
    void setSearchBoxClickListener(OnClickListener listener) {
        mModel.set(SearchBoxProperties.SEARCH_BOX_CLICK_CALLBACK, v -> listener.onClick(v));
    }

    /**
     * Called to add a click listener for the voice search button.
     */
    void addVoiceSearchButtonClickListener(OnClickListener listener) {
        boolean hasExistingListeners = !mVoiceSearchClickListeners.isEmpty();
        mVoiceSearchClickListeners.add(listener);
        if (hasExistingListeners) return;
        mModel.set(SearchBoxProperties.VOICE_SEARCH_CLICK_CALLBACK, v -> {
            for (OnClickListener clickListener : mVoiceSearchClickListeners) {
                clickListener.onClick(v);
            }
        });
    }

    /**
     * Called to add a click listener for the voice search button.
     */
    void addLensButtonClickListener(OnClickListener listener) {
        boolean hasExistingListeners = !mLensClickListeners.isEmpty();
        mLensClickListeners.add(listener);
        if (hasExistingListeners) return;
        mModel.set(SearchBoxProperties.LENS_CLICK_CALLBACK, v -> {
            for (OnClickListener clickListener : mLensClickListeners) {
                clickListener.onClick(v);
            }
        });
    }

    /**
     * Launch the Lens app.
     * @param lensEntryPoint A {@link LensEntryPoint}.
     * @param windowAndroid A {@link WindowAndroid} instance.
     * @param isIncognito Whether the request is from a Incognito tab.
     */
    void startLens(
            @LensEntryPoint int lensEntryPoint, WindowAndroid windowAndroid, boolean isIncognito) {
        LensController.getInstance().startLens(
                windowAndroid, new LensIntentParams.Builder(lensEntryPoint, isIncognito).build());
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
        return LensController.getInstance().isLensEnabled(
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

    void setTextViewTranslationX(float translationX) {
        mModel.set(SearchBoxProperties.SEARCH_TEXT_TRANSLATION_X, translationX);
    }

    void setButtonsHeight(int height) {
        mModel.set(SearchBoxProperties.BUTTONS_HEIGHT, height);
    }

    void setButtonsWidth(int width) {
        mModel.set(SearchBoxProperties.BUTTONS_WIDTH, width);
    }

    void setLensButtonLeftMargin(int leftMargin) {
        mModel.set(SearchBoxProperties.LENS_BUTTON_LEFT_MARGIN, leftMargin);
    }

    private Drawable getRoundedDrawable(Bitmap bitmap) {
        if (bitmap == null) return null;
        RoundedBitmapDrawable roundedBitmapDrawable =
                ViewUtils.createRoundedBitmapDrawable(mContext.getResources(), bitmap, 0);
        roundedBitmapDrawable.setCircular(true);
        return roundedBitmapDrawable;
    }
}

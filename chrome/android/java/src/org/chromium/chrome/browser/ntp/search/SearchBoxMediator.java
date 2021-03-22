// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp.search;

import android.content.Context;
import android.content.res.ColorStateList;
import android.graphics.Bitmap;
import android.graphics.drawable.Drawable;
import android.text.TextUtils;
import android.util.Pair;
import android.view.View.OnClickListener;
import android.view.ViewGroup;

import androidx.annotation.ColorInt;
import androidx.core.graphics.drawable.RoundedBitmapDrawable;

import org.chromium.chrome.browser.AppHooks;
import org.chromium.chrome.browser.gsa.GSAState;
import org.chromium.chrome.browser.lens.LensEntryPoint;
import org.chromium.chrome.browser.lens.LensIntentParams;
import org.chromium.chrome.browser.lens.LensQueryParams;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.Destroyable;
import org.chromium.chrome.browser.lifecycle.NativeInitObserver;
import org.chromium.chrome.browser.omnibox.voice.AssistantVoiceSearchService;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.components.externalauth.ExternalAuthUtils;
import org.chromium.components.signin.AccountManagerFacadeProvider;
import org.chromium.ui.base.ViewUtils;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

import java.util.ArrayList;
import java.util.List;

class SearchBoxMediator
        implements Destroyable, NativeInitObserver, AssistantVoiceSearchService.Observer {
    private final Context mContext;
    private final PropertyModel mModel;
    private final ViewGroup mView;
    private final List<OnClickListener> mVoiceSearchClickListeners = new ArrayList<>();
    private final List<OnClickListener> mLensClickListeners = new ArrayList<>();
    private ActivityLifecycleDispatcher mActivityLifecycleDispatcher;
    private AssistantVoiceSearchService mAssistantVoiceSearchService;
    private SearchBoxChipDelegate mChipDelegate;

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
    public void destroy() {
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
                GSAState.getInstance(mContext), this, SharedPreferencesManager.getInstance(),
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

        final @ColorInt int primaryColor = ChromeColors.getDefaultThemeColor(
                mContext.getResources(), false /* forceDarkBgColor= */);
        ColorStateList colorStateList =
                mAssistantVoiceSearchService.getMicButtonColorStateList(primaryColor, mContext);
        mModel.set(SearchBoxProperties.VOICE_SEARCH_COLOR_STATE_LIST, colorStateList);
    }

    /** Called to set a click listener for the search box. */
    void setSearchBoxClickListener(OnClickListener listener) {
        mModel.set(SearchBoxProperties.SEARCH_BOX_CLICK_CALLBACK, v -> {
            boolean isChipVisible = mModel.get(SearchBoxProperties.CHIP_VISIBILITY);
            if (isChipVisible) {
                String chipText = mModel.get(SearchBoxProperties.CHIP_TEXT);
                mModel.set(SearchBoxProperties.SEARCH_TEXT, Pair.create(chipText, true));
                mChipDelegate.onCancelClicked();
            }
            listener.onClick(v);
        });
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
     * Called to set or clear a chip on the search box.
     * @param chipText The text to be shown on the chip.
     */
    void setChipText(String chipText) {
        boolean chipVisible = !TextUtils.isEmpty(chipText);
        mModel.set(SearchBoxProperties.CHIP_VISIBILITY, chipVisible);
        mModel.set(SearchBoxProperties.SEARCH_HINT_VISIBILITY, !chipVisible);
        mModel.set(SearchBoxProperties.CHIP_TEXT, chipText);
    }

    /**
     * Called to set a delegate for handling the interactions between the chip and the embedder.
     * @param chipDelegate A {@link SearchBoxChipDelegate}.
     */
    void setChipDelegate(SearchBoxChipDelegate chipDelegate) {
        mChipDelegate = chipDelegate;
        mModel.set(SearchBoxProperties.CHIP_CLICK_CALLBACK, v -> chipDelegate.onChipClicked());
        mModel.set(SearchBoxProperties.CHIP_CANCEL_CALLBACK, v -> chipDelegate.onCancelClicked());
        mChipDelegate.getChipIcon(bitmap -> {
            mModel.set(SearchBoxProperties.CHIP_DRAWABLE, getRoundedDrawable(bitmap));
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
        AppHooks.get().getLensController().startLens(
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
        return AppHooks.get().getLensController().isLensEnabled(
                new LensQueryParams.Builder(lensEntryPoint, isIncognito, isTablet).build());
    }

    private Drawable getRoundedDrawable(Bitmap bitmap) {
        if (bitmap == null) return null;
        RoundedBitmapDrawable roundedBitmapDrawable =
                ViewUtils.createRoundedBitmapDrawable(mContext.getResources(), bitmap, 0);
        roundedBitmapDrawable.setCircular(true);
        return roundedBitmapDrawable;
    }
}

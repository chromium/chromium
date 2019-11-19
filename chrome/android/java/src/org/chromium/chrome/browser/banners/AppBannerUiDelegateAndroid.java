// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.banners;

import android.graphics.Bitmap;
import android.util.Pair;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.ShortcutHelper;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.webapps.addtohomescreen.AddToHomescreenDialogView;
import org.chromium.chrome.browser.webapps.addtohomescreen.AddToHomescreenProperties;
import org.chromium.chrome.browser.webapps.addtohomescreen.AddToHomescreenViewBinder;
import org.chromium.chrome.browser.webapps.addtohomescreen.AddToHomescreenViewDelegate;
import org.chromium.chrome.browser.webapps.addtohomescreen.AppType;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * Handles the promotion and installation of an app specified by the current web page. This object
 * is created by and owned by the native AppBannerUiDelegate.
 */
@JNINamespace("banners")
public class AppBannerUiDelegateAndroid implements AddToHomescreenViewDelegate {
    private static final String TAG = "AppBannerUi";

    /** Pointer to the native AppBannerUiDelegateAndroid. */
    private long mNativePointer;

    private Tab mTab;

    private PropertyModel mViewModel;

    private boolean mAddedToHomescreen;

    private AppBannerUiDelegateAndroid(long nativePtr, Tab tab) {
        mNativePointer = nativePtr;
        mTab = tab;
    }

    @Override
    public void onAddToHomescreen(String title) {
        mAddedToHomescreen = true;
        // The title is ignored for app banners as we respect the developer-provided title.
        if (mNativePointer != 0) {
            AppBannerUiDelegateAndroidJni.get().addToHomescreen(
                    mNativePointer, AppBannerUiDelegateAndroid.this);
        }
    }

    @Override
    public boolean onAppDetailsRequested() {
        if (mNativePointer != 0) {
            return AppBannerUiDelegateAndroidJni.get().showNativeAppDetails(
                    mNativePointer, AppBannerUiDelegateAndroid.this);
        }
        return false;
    }

    @Override
    public void onViewDismissed() {
        if (!mAddedToHomescreen && mNativePointer != 0) {
            AppBannerUiDelegateAndroidJni.get().onUiCancelled(
                    mNativePointer, AppBannerUiDelegateAndroid.this);
        }

        mViewModel = null;
        mAddedToHomescreen = false;
    }

    @CalledByNative
    private void destroy() {
        mNativePointer = 0;
        mAddedToHomescreen = false;
    }

    @CalledByNative
    private void showAppDetails(AppData appData) {
        mTab.getWindowAndroid().showIntent(appData.detailsIntent(), null, null);
    }

    /**
     * Build a view model.
     *
     * @return An instance of {@link PropertyModel}.
     */
    private PropertyModel createViewModel() {
        PropertyModel model = new PropertyModel.Builder(AddToHomescreenProperties.ALL_KEYS).build();
        PropertyModelChangeProcessor.create(model,
                new AddToHomescreenDialogView(mTab.getActivity(),
                        mTab.getActivity().getModalDialogManager(),
                        AppBannerManager.getHomescreenLanguageOption(), this),
                AddToHomescreenViewBinder::bind);
        return model;
    }

    @CalledByNative
    private boolean showNativeAppDialog(Bitmap iconBitmap, AppData appData) {
        mViewModel = createViewModel();
        mViewModel.set(AddToHomescreenProperties.TITLE, appData.title());
        mViewModel.set(AddToHomescreenProperties.ICON, new Pair<>(iconBitmap, false));
        mViewModel.set(AddToHomescreenProperties.TYPE, AppType.NATIVE);
        mViewModel.set(AddToHomescreenProperties.CAN_SUBMIT, true);
        mViewModel.set(AddToHomescreenProperties.NATIVE_APP_RATING, appData.rating());
        mViewModel.set(
                AddToHomescreenProperties.NATIVE_INSTALL_BUTTON_TEXT, appData.installButtonText());
        return true;
    }

    @CalledByNative
    private boolean showWebAppDialog(
            String title, Bitmap iconBitmap, String url, boolean maskable) {
        mViewModel = createViewModel();
        mViewModel.set(AddToHomescreenProperties.TITLE, title);
        mViewModel.set(AddToHomescreenProperties.TYPE, AppType.WEBAPK);
        mViewModel.set(AddToHomescreenProperties.CAN_SUBMIT, true);
        mViewModel.set(AddToHomescreenProperties.URL, url);

        // Because web standards specify the circular mask's radius to be 40% of icon dimension,
        // while Android specifies it to be around 30%, we can't simply let Android mask it. We'll
        // need to mask the bitmap ourselves and pass it as a non-maskable icon.
        boolean shouldMaskIcon = maskable && ShortcutHelper.doesAndroidSupportMaskableIcons();
        if (shouldMaskIcon) {
            iconBitmap = ShortcutHelper.createHomeScreenIconFromWebIcon(iconBitmap, shouldMaskIcon);
        }
        mViewModel.set(AddToHomescreenProperties.ICON, new Pair<>(iconBitmap, shouldMaskIcon));
        return true;
    }

    @CalledByNative
    private static AppBannerUiDelegateAndroid create(long nativePtr, Tab tab) {
        return new AppBannerUiDelegateAndroid(nativePtr, tab);
    }

    @NativeMethods
    interface Natives {
        void addToHomescreen(
                long nativeAppBannerUiDelegateAndroid, AppBannerUiDelegateAndroid caller);

        void onUiCancelled(
                long nativeAppBannerUiDelegateAndroid, AppBannerUiDelegateAndroid caller);

        boolean showNativeAppDetails(
                long nativeAppBannerUiDelegateAndroid, AppBannerUiDelegateAndroid caller);
    }
}

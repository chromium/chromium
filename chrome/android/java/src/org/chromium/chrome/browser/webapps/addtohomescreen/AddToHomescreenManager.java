// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps.addtohomescreen;

import android.graphics.Bitmap;
import android.text.TextUtils;
import android.util.Pair;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.banners.AppBannerManager;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * Manages the add to home screen process. Coordinates the native-side data fetching, and owns
 * a dialog prompting the user to confirm the action (and potentially supply a title).
 */
public class AddToHomescreenManager implements AddToHomescreenViewDelegate {
    protected final ChromeActivity mActivity;
    protected final Tab mTab;

    protected PropertyModel mViewModel;
    private long mNativeAddToHomescreenManager;

    public AddToHomescreenManager(ChromeActivity activity, Tab tab) {
        mActivity = activity;
        mTab = tab;
    }

    /**
     * Starts the add to home screen process. Creates the C++ AddToHomescreenManager, which fetches
     * the data needed for add to home screen, and informs this object when data is available and
     * when the dialog can be shown.
     */
    public void start() {
        // Don't start if we've already started or if there is no visible URL to add.
        if (mNativeAddToHomescreenManager != 0 || TextUtils.isEmpty(mTab.getUrl())) return;

        mNativeAddToHomescreenManager = AddToHomescreenManagerJni.get().initializeAndStart(
                AddToHomescreenManager.this, mTab.getWebContents());
    }

    /**
     * Puts the object in a state where it is safe to be destroyed.
     */
    public void destroy() {
        mViewModel = null;
        if (mNativeAddToHomescreenManager == 0) return;

        AddToHomescreenManagerJni.get().destroy(
                mNativeAddToHomescreenManager, AddToHomescreenManager.this);
        mNativeAddToHomescreenManager = 0;
    }

    @Override
    public void onAddToHomescreen(String title) {
        assert mNativeAddToHomescreenManager != 0;

        AddToHomescreenManagerJni.get().addToHomescreen(
                mNativeAddToHomescreenManager, AddToHomescreenManager.this, title);
    }

    @Override
    public boolean onAppDetailsRequested() {
        return false;
    }

    @Override
    public void onViewDismissed() {
        destroy();
    }

    /**
     * Shows alert to prompt user for name of home screen shortcut.
     */
    @CalledByNative
    public void showDialog() {
        mViewModel = new PropertyModel.Builder(AddToHomescreenProperties.ALL_KEYS).build();
        PropertyModelChangeProcessor.create(mViewModel,
                new AddToHomescreenDialogView(mActivity, mActivity.getModalDialogManager(),
                        AppBannerManager.getHomescreenLanguageOption(), this),
                AddToHomescreenViewBinder::bind);
    }

    @CalledByNative
    private void onUserTitleAvailable(String title, String url, boolean isWebapp) {
        // Users may edit the title of bookmark shortcuts, but we respect web app names and do not
        // let users change them.
        mViewModel.set(AddToHomescreenProperties.TITLE, title);
        mViewModel.set(AddToHomescreenProperties.URL, url);
        mViewModel.set(
                AddToHomescreenProperties.TYPE, isWebapp ? AppType.WEBAPK : AppType.SHORTCUT);
    }

    @CalledByNative
    private void onIconAvailable(Bitmap icon, boolean iconAdaptable) {
        mViewModel.set(AddToHomescreenProperties.ICON, new Pair<>(icon, iconAdaptable));
        mViewModel.set(AddToHomescreenProperties.CAN_SUBMIT, true);
    }

    @NativeMethods
    interface Natives {
        long initializeAndStart(AddToHomescreenManager caller, WebContents webContents);
        void addToHomescreen(long nativeAddToHomescreenManager, AddToHomescreenManager caller,
                String userRequestedTitle);
        void destroy(long nativeAddToHomescreenManager, AddToHomescreenManager caller);
    }
}

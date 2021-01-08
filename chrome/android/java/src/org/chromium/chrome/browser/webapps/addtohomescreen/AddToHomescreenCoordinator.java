// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps.addtohomescreen;

import android.app.Activity;
import android.content.Context;
import android.os.Bundle;

import androidx.annotation.StringRes;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.banners.AppBannerManager;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.webapps.AddToHomescreenProperties;
import org.chromium.chrome.browser.webapps.AddToHomescreenViewDelegate;
import org.chromium.chrome.browser.webapps.PwaBottomSheetController;
import org.chromium.chrome.browser.webapps.PwaBottomSheetControllerProvider;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * This class is responsible for setting up and coordinating everything related to the
 * add-to-homescreen UI component.
 *
 * The {@link #showForAppMenu} method is used to show the add-to-homescreen UI when the user
 * chooses the "Add to Home screen" option from the app menu.
 */
@JNINamespace("webapps")
public class AddToHomescreenCoordinator {
    @VisibleForTesting
    Context mActivityContext;
    @VisibleForTesting
    ModalDialogManager mModalDialogManager;
    private WindowAndroid mWindowAndroid;
    // May be null during tests.
    private Tab mTab;

    @VisibleForTesting
    AddToHomescreenCoordinator(Tab tab, Context activityContext, WindowAndroid windowAndroid,
            ModalDialogManager modalDialogManager) {
        mActivityContext = activityContext;
        mWindowAndroid = windowAndroid;
        mModalDialogManager = modalDialogManager;
        mTab = tab;
    }

    /**
     * Starts and shows the add-to-homescreen UI component for the given {@link WebContents}.
     * @return whether add-to-homescreen UI was started successfully.
     */
    public static boolean showForAppMenu(Tab tab, Context activityContext,
            WindowAndroid windowAndroid, ModalDialogManager modalDialogManager,
            WebContents webContents, Bundle menuItemData) {
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.PWA_INSTALL_USE_BOTTOMSHEET)) {
            PwaBottomSheetController controller =
                    PwaBottomSheetControllerProvider.from(windowAndroid);
            if (controller != null) {
                controller.requestOrExpandBottomSheetInstaller(webContents);
                return true;
            }
        }

        @StringRes
        int titleId = menuItemData.getInt(AppBannerManager.MENU_TITLE_KEY);
        return new AddToHomescreenCoordinator(
                tab, activityContext, windowAndroid, modalDialogManager)
                .showForAppMenu(webContents, titleId);
    }

    @VisibleForTesting
    boolean showForAppMenu(WebContents webContents, @StringRes int titleId) {
        // Don't start if there is no visible URL to add.
        if (webContents == null || webContents.getVisibleUrl().isEmpty()) {
            return false;
        }

        buildMediatorAndShowDialog().startForAppMenu(webContents, titleId);
        return true;
    }

    /**
     * Constructs all MVC components on request from the C++ side.
     * @param tab The current {@link Tab}. Used for accessing activity {@link Context} and
     * {@link ModalDialogManager}.
     * @return A C++ pointer to the associated add_to_homescreen_mediator.cc object. This will be
     * used by add_to_homescreen_coordinator.cc to complete the initialization of the mediator.
     */
    @CalledByNative
    private static long initMvcAndReturnMediator(Tab tab) {
        WindowAndroid windowAndroid = tab.getWindowAndroid();
        if (windowAndroid == null) return 0;

        Activity activity = windowAndroid.getActivity().get();
        if (activity == null || !(activity instanceof ChromeActivity)) return 0;

        ModalDialogManager modalDialogManager = ((ChromeActivity) activity).getModalDialogManager();
        if (modalDialogManager == null) return 0;

        AddToHomescreenCoordinator coordinator =
                new AddToHomescreenCoordinator(tab, activity, windowAndroid, modalDialogManager);
        return coordinator.buildMediatorAndShowDialog().getNativeMediator();
    }

    /**
     * Constructs all MVC components. {@link AddToHomescreenDialogView} is shown as soon as it's
     * constructed.
     * @return The instance of {@link AddToHomescreenMediator} that was constructed.
     */
    private AddToHomescreenMediator buildMediatorAndShowDialog() {
        PropertyModel model = new PropertyModel.Builder(AddToHomescreenProperties.ALL_KEYS).build();
        AddToHomescreenMediator addToHomescreenMediator =
                new AddToHomescreenMediator(model, mWindowAndroid);
        PropertyModelChangeProcessor.create(model,
                initView(AppBannerManager.getHomescreenLanguageOption(
                                 mTab == null ? null : mTab.getWebContents()),
                        addToHomescreenMediator),
                AddToHomescreenViewBinder::bind);
        return addToHomescreenMediator;
    }

    /**
     * Creates and returns a {@link AddToHomescreenDialogView}.
     * Extracted into a separate method for easier testing.
     */
    @VisibleForTesting
    protected AddToHomescreenDialogView initView(AppBannerManager.InstallStringPair installStrings,
            AddToHomescreenViewDelegate delegate) {
        return new AddToHomescreenDialogView(
                mActivityContext, mModalDialogManager, installStrings, delegate);
    }
}

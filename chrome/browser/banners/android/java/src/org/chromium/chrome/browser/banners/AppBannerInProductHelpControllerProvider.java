// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.banners;

import org.chromium.base.UnownedUserDataKey;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.content_public.browser.Visibility;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;

/**
 * This class manages the details associated with binding a {@link AppBannerInProductHelpController}
 * to user data on a {@link WindowAndroid}.
 */
public class AppBannerInProductHelpControllerProvider {
    /** The key used to bind the controller to the unowned data host. */
    private static final UnownedUserDataKey<AppBannerInProductHelpController> KEY =
            new UnownedUserDataKey<>(AppBannerInProductHelpController.class);

    /**
     * Get the shared {@link AppBannerInProductHelpController} from the provided {@link
     * WindowAndroid}.
     * @param windowAndroid The window to pull the controller from.
     * @return A shared instance of a {@link AppBannerInProductHelpController}.
     */
    private static AppBannerInProductHelpController from(WindowAndroid windowAndroid) {
        return KEY.retrieveDataFromHost(windowAndroid.getUnownedUserDataHost());
    }

    static void attach(WindowAndroid windowAndroid, AppBannerInProductHelpController controller) {
        KEY.attachToHost(windowAndroid.getUnownedUserDataHost(), controller);
    }

    static void detach(AppBannerInProductHelpController controller) {
        KEY.detachFromAllHosts(controller);
    }

    /**
     * Request to show the in-product help for installing a PWA.
     * @param webContents The current WebContents.
     * @return An error message, if unsuccessful. Blank if the request was made.
     */
    @CalledByNative
    private static String showInProductHelp(WebContents webContents) {
        // Consult the tracker to see if the IPH can be shown.
        final Tracker tracker =
                TrackerFactory.getTrackerForProfile(Profile.fromWebContents(webContents));
        if (!tracker.wouldTriggerHelpUI(FeatureConstants.PWA_INSTALL_AVAILABLE_FEATURE)) {
            // Tracker replied that the request to show will not be honored. Return whether the
            // limit of how often to show has been exceeded.
            return "Trigger state: "
                    + tracker.getTriggerState(FeatureConstants.PWA_INSTALL_AVAILABLE_FEATURE);
        }

        if (webContents.getVisibility() != Visibility.VISIBLE) return "Not visible";
        WindowAndroid window = webContents.getTopLevelNativeWindow();
        if (window == null) return "No window";
        AppBannerInProductHelpController controller = from(window);
        if (controller == null) return "No controller";
        controller.requestInProductHelp();
        return "";
    }
}

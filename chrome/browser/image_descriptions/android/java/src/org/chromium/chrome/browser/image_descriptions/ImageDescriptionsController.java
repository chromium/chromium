// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.image_descriptions;

import android.content.Context;

import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.util.ChromeAccessibilityUtil;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.content_public.browser.ContentFeatureList;
import org.chromium.ui.modaldialog.ModalDialogManager;

/**
 * Singleton class to control the Image Descriptions feature. This class can be used to initiate the
 * user flow, to turn the feature on/off and to update settings as needed.
 */
public class ImageDescriptionsController {
    // We display a "Don't ask again" choice if the user has selected the Just Once option 3 times.
    public static final int DONT_ASK_AGAIN_DISPLAY_LIMIT = 3;

    // Static instance of this singleton, lazily initialized during first getInstance() call.
    private static ImageDescriptionsController sInstance;

    private ImageDescriptionsDialog.Delegate mDelegate;

    /**
     * Method to return the private instance of this singleton, lazily initialized.
     * @return      ImageDescriptionController instance
     */
    public static ImageDescriptionsController getInstance() {
        if (sInstance == null) {
            sInstance = new ImageDescriptionsController();
        }

        return sInstance;
    }

    /**
     * Private constructor to prevent unwanted construction/initialization
     */
    private ImageDescriptionsController() {
        this.mDelegate = defaultDelegate();
    }

    /**
     * Creates a default ImageDescriptionsDialog.Delegate implementation, used in production.
     * @return      Default ImageDescriptionsDialog.Delegate delegate.
     */
    private ImageDescriptionsDialog.Delegate defaultDelegate() {
        return new ImageDescriptionsDialog.Delegate() {
            @Override
            public void enableImageDescriptions(boolean onlyOnWifi) {
                getPrefService().setBoolean(Pref.ACCESSIBILITY_IMAGE_LABELS_ENABLED_ANDROID, true);
                getPrefService().setBoolean(
                        Pref.ACCESSIBILITY_IMAGE_LABELS_ONLY_ON_WIFI, onlyOnWifi);
                // TODO (mschillaci@) - Use JNI to enable descriptions in native code with AXMode
            }

            @Override
            public void getImageDescriptionsJustOnce(boolean dontAskAgain) {
                // User selected "Just Once", update counter and "Don't ask again" preference as
                // needed.
                getSharedPrefs().incrementInt(
                        ChromePreferenceKeys.IMAGE_DESCRIPTIONS_JUST_ONCE_COUNT);
                getSharedPrefs().writeBoolean(
                        ChromePreferenceKeys.IMAGE_DESCRIPTIONS_DONT_ASK_AGAIN, dontAskAgain);

                // TODO (mschillaci@) - Use JNI to enable descriptions once with AXActionData. Will
                //                      need a Tab so that we can get web_contents.
            }
        };
    }

    /**
     * Set the ImageDescriptionsDialog.Delegate delegate one time, used for testing purposes.
     * @param delegate      The new ImageDescriptionsDialog.Delegate delegate to use.
     */
    @VisibleForTesting
    public void setDelegateForTesting(ImageDescriptionsDialog.Delegate delegate) {
        this.mDelegate = delegate;
    }

    /**
     * Handle user selecting menu item and the potential creation of the image descriptions dialog.
     */
    public void onImageDescriptionsMenuItemSelected(
            Context context, ModalDialogManager modalDialogManager) {
        // If descriptions are enabled, then the menu item option was to stop descriptions. If the
        // user has the don't ask again option enabled, immediately do a "just once" fetch. In all
        // other cases, show the dialog to prompt the user.
        if (imageDescriptionsEnabled()) {
            disableImageDescriptions();
        } else if (dontAskAgainEnabled()) {
            getImageDescriptionsJustOnce(true);
        } else {
            ImageDescriptionsDialog prompt = new ImageDescriptionsDialog(
                    context, modalDialogManager, mDelegate, shouldShowDontAskAgainOption());
            prompt.show();
        }
    }

    protected void disableImageDescriptions() {
        getPrefService().setBoolean(Pref.ACCESSIBILITY_IMAGE_LABELS_ENABLED_ANDROID, false);
        // TODO (mschillaci@) - Potentially remove AXMode through JNI to turn off descriptions?
    }

    protected boolean dontAskAgainEnabled() {
        return getSharedPrefs().readBoolean(
                ChromePreferenceKeys.IMAGE_DESCRIPTIONS_DONT_ASK_AGAIN, false);
    }

    protected boolean shouldShowDontAskAgainOption() {
        return getSharedPrefs().readInt(ChromePreferenceKeys.IMAGE_DESCRIPTIONS_JUST_ONCE_COUNT)
                >= DONT_ASK_AGAIN_DISPLAY_LIMIT;
    }

    public boolean shouldShowImageDescriptionsMenuItem() {
        // TODO (mschillaci@) - Expand this to check touch exploration rather than accessibility
        return ContentFeatureList.isEnabled(ContentFeatureList.EXPERIMENTAL_ACCESSIBILITY_LABELS)
                && ChromeAccessibilityUtil.get().isAccessibilityEnabled();
    }

    public boolean imageDescriptionsEnabled() {
        return getPrefService().getBoolean(Pref.ACCESSIBILITY_IMAGE_LABELS_ENABLED_ANDROID);
    }

    public boolean onlyOnWifiEnabled() {
        return getPrefService().getBoolean(Pref.ACCESSIBILITY_IMAGE_LABELS_ONLY_ON_WIFI);
    }

    // Pass-through methods to our delegate.

    protected void enableImageDescriptions(boolean onlyOnWifi) {
        mDelegate.enableImageDescriptions(onlyOnWifi);
    }

    protected void getImageDescriptionsJustOnce(boolean dontAskAgain) {
        mDelegate.getImageDescriptionsJustOnce(dontAskAgain);
    }

    /**
     * Helper method to return PrefService for last used regular profile.
     * @return PrefService
     */
    private PrefService getPrefService() {
        // TODO (mschillaci@) - Use the correct profile here for Incognito mode etc.
        return UserPrefs.get(Profile.getLastUsedRegularProfile());
    }

    /**
     * Helper method to return SharedPreferencesManager instance.
     * @return SharedPreferencesManager
     */
    private SharedPreferencesManager getSharedPrefs() {
        return SharedPreferencesManager.getInstance();
    }
}
// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.image_descriptions;

import android.content.Context;

import org.jni_zero.NativeMethods;

import org.chromium.base.ResettersForTesting;
import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.device.DeviceConditions;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.content_public.browser.WebContents;
import org.chromium.net.ConnectionType;
import org.chromium.ui.accessibility.AccessibilityState;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.widget.Toast;

/**
 * Singleton class to control the Image Descriptions feature. This class can be used to initiate the
 * user flow, to turn the feature on/off and to update settings as needed.
 */
public class ImageDescriptionsController {
    // We display a "Don't ask again" choice if the user has selected the Just Once option 3 times.
    public static final int DONT_ASK_AGAIN_DISPLAY_LIMIT = 3;

    // Static instance of this singleton, lazily initialized during first getInstance() call.
    private static ImageDescriptionsController sInstance;

    private ImageDescriptionsControllerDelegate mDelegate;

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

    /** Private constructor to prevent unwanted construction/initialization */
    private ImageDescriptionsController() {
        this.mDelegate = defaultDelegate();
    }

    /**
     * Creates a default ImageDescriptionsControllerDelegate implementation, used in production.
     * @return      Default ImageDescriptionsControllerDelegate delegate.
     */
    private ImageDescriptionsControllerDelegate defaultDelegate() {
        return new ImageDescriptionsControllerDelegate() {
            @Override
            public void enableImageDescriptions(Profile profile) {
                getPrefService(profile)
                        .setBoolean(Pref.ACCESSIBILITY_IMAGE_LABELS_ENABLED_ANDROID, true);
            }

            @Override
            public void disableImageDescriptions(Profile profile) {
                getPrefService(profile)
                        .setBoolean(Pref.ACCESSIBILITY_IMAGE_LABELS_ENABLED_ANDROID, false);
            }

            @Override
            public void setOnlyOnWifiRequirement(boolean onlyOnWifi, Profile profile) {
                getPrefService(profile)
                        .setBoolean(Pref.ACCESSIBILITY_IMAGE_LABELS_ONLY_ON_WIFI, onlyOnWifi);
            }

            @Override
            public void getImageDescriptionsJustOnce(
                    boolean dontAskAgain, WebContents webContents) {
                // User selected "Just Once", update counter and "Don't ask again" preference as
                // needed.
                getSharedPrefs()
                        .incrementInt(ChromePreferenceKeys.IMAGE_DESCRIPTIONS_JUST_ONCE_COUNT);
                getSharedPrefs()
                        .writeBoolean(
                                ChromePreferenceKeys.IMAGE_DESCRIPTIONS_DONT_ASK_AGAIN,
                                dontAskAgain);

                ImageDescriptionsControllerJni.get().getImageDescriptionsOnce(webContents);
            }
        };
    }

    /**
     * Set the ImageDescriptionsControllerDelegate delegate one time, used for testing purposes.
     * @param delegate      The new ImageDescriptionsControllerDelegate delegate to use.
     */
    public void setDelegateForTesting(ImageDescriptionsControllerDelegate delegate) {
        var oldValue = this.mDelegate;
        this.mDelegate = delegate;
        ResettersForTesting.register(() -> this.mDelegate = oldValue);
    }

    /**
     * Get the current ImageDescriptionsControllerDelegate in use for this instance.
     * @return mDelegate    The current ImageDescriptionsControllerDelegate in use.
     */
    public ImageDescriptionsControllerDelegate getDelegate() {
        return mDelegate;
    }

    /**
     * Handle user selecting menu item and the potential creation of the image descriptions dialog.
     */
    public void onImageDescriptionsMenuItemSelected(
            Context context, ModalDialogManager modalDialogManager, WebContents webContents) {
        Profile profile = Profile.fromWebContents(webContents).getOriginalProfile();
        boolean enabledBeforeMenuItemSelected = imageDescriptionsEnabled(profile);

        if (enabledBeforeMenuItemSelected) {
            // If descriptions are enabled, and the user has selected "only on wifi", and we
            // currently do not have a wifi connection, then do a "just once" fetch.
            if (onlyOnWifiEnabled(profile)
                    && DeviceConditions.getCurrentNetConnectionType(context)
                            != ConnectionType.CONNECTION_WIFI) {
                mDelegate.getImageDescriptionsJustOnce(false, webContents);
                Toast.makeText(
                                context,
                                R.string.image_descriptions_toast_just_once,
                                Toast.LENGTH_LONG)
                        .show();
            } else {
                // Otherwise, user has elected to stop descriptions.
                mDelegate.disableImageDescriptions(profile);
                Toast.makeText(context, R.string.image_descriptions_toast_off, Toast.LENGTH_LONG)
                        .show();
            }
        } else {
            // If descriptions are not enabled, and the user has selected "don't ask again", then do
            // a "just once" fetch. In all other cases, show the dialog to prompt the user.
            if (dontAskAgainEnabled()) {
                mDelegate.getImageDescriptionsJustOnce(true, webContents);
                Toast.makeText(
                                context,
                                R.string.image_descriptions_toast_just_once,
                                Toast.LENGTH_LONG)
                        .show();
            } else {
                ImageDescriptionsDialog prompt =
                        new ImageDescriptionsDialog(
                                context,
                                modalDialogManager,
                                getDelegate(),
                                shouldShowDontAskAgainOption(),
                                webContents);
                prompt.show();
            }
        }
    }

    protected boolean dontAskAgainEnabled() {
        return getSharedPrefs()
                .readBoolean(ChromePreferenceKeys.IMAGE_DESCRIPTIONS_DONT_ASK_AGAIN, false);
    }

    protected boolean shouldShowDontAskAgainOption() {
        return getSharedPrefs().readInt(ChromePreferenceKeys.IMAGE_DESCRIPTIONS_JUST_ONCE_COUNT)
                >= DONT_ASK_AGAIN_DISPLAY_LIMIT;
    }

    public boolean shouldShowImageDescriptionsMenuItem() {
        return AccessibilityState.isScreenReaderEnabled();
    }

    public boolean imageDescriptionsEnabled(Profile profile) {
        return getPrefService(profile).getBoolean(Pref.ACCESSIBILITY_IMAGE_LABELS_ENABLED_ANDROID);
    }

    public boolean onlyOnWifiEnabled(Profile profile) {
        return getPrefService(profile).getBoolean(Pref.ACCESSIBILITY_IMAGE_LABELS_ONLY_ON_WIFI);
    }

    /**
     * Helper method to return PrefService for last used regular profile.
     * @return PrefService
     */
    private PrefService getPrefService(Profile profile) {
        return UserPrefs.get(profile);
    }

    /**
     * Helper method to return SharedPreferencesManager instance.
     * @return ChromeSharedPreferences
     */
    private SharedPreferencesManager getSharedPrefs() {
        return ChromeSharedPreferences.getInstance();
    }

    @NativeMethods
    interface Natives {
        void getImageDescriptionsOnce(WebContents webContents);
    }
}

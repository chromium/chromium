// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr;

import android.content.pm.PackageManager;
import android.content.res.Resources;

import androidx.annotation.NonNull;

import org.chromium.base.Log;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.ui.base.PermissionCallback;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Provides a consent dialog shown before entering an immersive AR session.
 *
 * <p>For the duration of the session, the site will get ARCore world understanding
 * data such as hit tests or plane detection, and also camera movement tracking via
 * 6DoF poses. The browser process separately receives the camera image and composites
 * that image with the application-drawn AR image.</p>
 *
 * <p>This is different from typical camera permission usage since the web page
 * will NOT get access to camera images. The user consent is only valid for
 * the duration of one session and is not persistent.</p>
 *
 * <p>The browser needs Android-level camera access for using ARCore, this
 * is requested if needed after the user has granted consent for the AR session.</p>
 */
@JNINamespace("vr")
public class ArConsentDialog implements ModalDialogProperties.Controller {
    private static final String TAG = "ArConsentDialog";
    private static final boolean DEBUG_LOGS = false;

    private ModalDialogManager mModalDialogManager;
    private long mNativeArConsentDialog;
    private WindowAndroid mWindowAndroid;

    @CalledByNative
    private static ArConsentDialog showDialog(long instance, @NonNull final Tab tab) {
        ArConsentDialog dialog = new ArConsentDialog(instance);
        dialog.show(tab.getActivity());
        return dialog;
    }

    private ArConsentDialog(long arConsentDialog) {
        mNativeArConsentDialog = arConsentDialog;
    }

    public void show(ChromeActivity activity) {
        mWindowAndroid = activity.getWindowAndroid();
        Resources resources = activity.getResources();
        PropertyModel model = new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                                      .with(ModalDialogProperties.CONTROLLER, this)
                                      .with(ModalDialogProperties.TITLE, resources,
                                              R.string.ar_immersive_mode_consent_title)
                                      .with(ModalDialogProperties.MESSAGE, resources,
                                              R.string.ar_immersive_mode_consent_message)
                                      .with(ModalDialogProperties.POSITIVE_BUTTON_TEXT, resources,
                                              R.string.ar_immersive_mode_consent_button)
                                      .with(ModalDialogProperties.NEGATIVE_BUTTON_TEXT, resources,
                                              R.string.cancel)
                                      .with(ModalDialogProperties.CANCEL_ON_TOUCH_OUTSIDE, true)
                                      .build();
        mModalDialogManager = activity.getModalDialogManager();
        mModalDialogManager.showDialog(model, ModalDialogManager.ModalDialogType.TAB);
    }

    @Override
    public void onClick(PropertyModel model, int buttonType) {
        if (buttonType == ModalDialogProperties.ButtonType.NEGATIVE) {
            mModalDialogManager.dismissDialog(model, DialogDismissalCause.NEGATIVE_BUTTON_CLICKED);
        } else {
            mModalDialogManager.dismissDialog(model, DialogDismissalCause.POSITIVE_BUTTON_CLICKED);
        }
    }

    @Override
    public void onDismiss(PropertyModel model, int dismissalCause) {
        if (dismissalCause == DialogDismissalCause.POSITIVE_BUTTON_CLICKED) {
            onConsentGranted();
        } else {
            consentDenied();
        }
    }

    private void onConsentGranted() {
        if (DEBUG_LOGS) Log.i(TAG, "onConsentGranted");

        if (!mWindowAndroid.hasPermission(android.Manifest.permission.CAMERA)) {
            // The user has agreed to proceed with the AR session, but the browser
            // application doesn't have the prerequisite Android-level camera permission
            // needed for using ARCore internally. Show the system permission prompt.
            requestCameraPermission();
            return;
        }
        consentGranted();
    }

    private void requestCameraPermission() {
        PermissionCallback callback = new PermissionCallback() {
            @Override
            public void onRequestPermissionsResult(String[] permissions, int[] grantResults) {
                if (DEBUG_LOGS) Log.i(TAG, "onRequestPermissionsResult");
                // If request is cancelled, the result arrays are empty.
                if (grantResults.length > 0
                        && grantResults[0] == PackageManager.PERMISSION_GRANTED) {
                    if (DEBUG_LOGS) Log.i(TAG, "onRequestPermissionsResult=granted");
                    consentGranted();
                } else {
                    // Didn't get permission :-(
                    if (DEBUG_LOGS) Log.i(TAG, "onRequestPermissionsResult=denied");
                    consentDenied();
                }
            }
        };

        mWindowAndroid.requestPermissions(
                new String[] {android.Manifest.permission.CAMERA}, callback);
    }

    private void consentGranted() {
        if (DEBUG_LOGS) Log.i(TAG, "consentGranted");
        // We have user consent to start the session.
        ArConsentDialogJni.get().onUserConsentResult(mNativeArConsentDialog, true);
    }

    private void consentDenied() {
        if (DEBUG_LOGS) Log.i(TAG, "consentDenied");
        ArConsentDialogJni.get().onUserConsentResult(mNativeArConsentDialog, false);
    }

    @NativeMethods
    /* package */ interface Natives {
        void onUserConsentResult(long nativeArCoreConsentPrompt, boolean allowed);
    }
}

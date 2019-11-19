// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr;

import android.content.res.Resources;

import androidx.annotation.NonNull;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.url_formatter.UrlFormatter;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsObserver;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Provides a consent dialog shown before entering an immersive VR session.
 */
@JNINamespace("vr")
public class VrConsentDialog
        extends WebContentsObserver implements ModalDialogProperties.Controller {
    @NativeMethods
    /* package */ interface VrConsentUiHelperImpl {
        void onUserConsentResult(long nativeGvrConsentHelperImpl, boolean allowed);
    }

    private ModalDialogManager mModalDialogManager;
    private VrConsentListener mListener;
    private long mNativeInstance;
    private ConsentFlowMetrics mMetrics;
    private String mUrl;
    private @XrConsentPromptLevel int mConsentLevel;

    private VrConsentDialog(
            long instance, WebContents webContents, @XrConsentPromptLevel int consentLevel) {
        super(webContents);
        mNativeInstance = instance;
        mMetrics = new ConsentFlowMetrics(webContents);
        mUrl = webContents.getLastCommittedUrl();
        mConsentLevel = consentLevel;
    }

    @CalledByNative
    private static VrConsentDialog promptForUserConsent(
            long instance, final Tab tab, @XrConsentPromptLevel int consentLevel) {
        VrConsentDialog dialog = new VrConsentDialog(instance, tab.getWebContents(), consentLevel);
        dialog.show(tab.getActivity(), new VrConsentListener() {
            @Override
            public void onUserConsent(boolean allowed) {
                dialog.onUserGesture(allowed);
            }
        });
        return dialog;
    }

    @VisibleForTesting
    protected void onUserGesture(boolean allowed) {
        VrConsentDialogJni.get().onUserConsentResult(mNativeInstance, allowed);
    }

    @Override
    public void didStartNavigation(NavigationHandle navigationHandle) {
        mModalDialogManager.dismissAllDialogs(DialogDismissalCause.UNKNOWN);
        onUserGesture(false);
    }

    public void show(@NonNull ChromeActivity activity, @NonNull VrConsentListener listener) {
        mListener = listener;

        Resources resources = activity.getResources();

        String dialogTitle = resources.getString(R.string.xr_consent_dialog_title,
                UrlFormatter.formatUrlForSecurityDisplayOmitScheme(mUrl));

        String dialogBody = resources.getString(R.string.xr_consent_dialog_description_default);
        switch (mConsentLevel) {
            case XrConsentPromptLevel.VR_FLOOR_PLAN:
                dialogBody += resources.getString(
                                      R.string.xr_consent_dialog_description_physical_features)
                        + resources.getString(R.string.xr_consent_dialog_description_floor_plan);
                break;
            case XrConsentPromptLevel.VR_FEATURES:
                dialogBody += resources.getString(
                        R.string.xr_consent_dialog_description_physical_features);
                break;
            case XrConsentPromptLevel.DEFAULT:
            default:
                dialogBody = "";
                break;
        }

        PropertyModel model = new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                                      .with(ModalDialogProperties.CONTROLLER, this)
                                      .with(ModalDialogProperties.TITLE, dialogTitle)
                                      .with(ModalDialogProperties.MESSAGE, dialogBody)
                                      .with(ModalDialogProperties.POSITIVE_BUTTON_TEXT, resources,
                                              R.string.xr_consent_dialog_button_allow_and_enter_vr)
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
        if (dismissalCause == DialogDismissalCause.UNKNOWN) return;

        if (dismissalCause == DialogDismissalCause.POSITIVE_BUTTON_CLICKED) {
            mListener.onUserConsent(true);
            mMetrics.logUserAction(ConsentDialogAction.USER_ALLOWED);
            mMetrics.logConsentFlowDurationWhenConsentGranted();
        } else if (dismissalCause == DialogDismissalCause.NEGATIVE_BUTTON_CLICKED) {
            mListener.onUserConsent(false);
            mMetrics.logUserAction(ConsentDialogAction.USER_DENIED);
            mMetrics.logConsentFlowDurationWhenConsentNotGranted();
        } else {
            mListener.onUserConsent(false);
            mMetrics.logUserAction(ConsentDialogAction.USER_ABORTED_CONSENT_FLOW);
            mMetrics.logConsentFlowDurationWhenUserAborted();
        }
        mMetrics.onDialogClosedWithConsent(
                mUrl, dismissalCause == DialogDismissalCause.POSITIVE_BUTTON_CLICKED);
    }
}

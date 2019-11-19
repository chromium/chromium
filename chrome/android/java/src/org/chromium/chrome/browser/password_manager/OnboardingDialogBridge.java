// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import static org.chromium.chrome.browser.ChromeFeatureList.PASSWORD_MANAGER_ONBOARDING_ANDROID;
import static org.chromium.chrome.browser.ChromeFeatureList.getFieldTrialParamByFeature;

import android.content.res.Resources;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;

/** JNI call glue between the native password manager onboarding class and Java objects. */
public class OnboardingDialogBridge {
    private long mNativeOnboardingDialogView;
    private final PasswordManagerDialogCoordinator mOnboardingDialog;
    private final Resources mResources;
    /** Experiment parameter. */
    private static final String EXPERIMENT_PARAMETER = "story";
    /** Story centered on password safety and leak detection. */
    private static final String ILLUSTRATION_01 = "safety";
    /** Story centered on availability on multiple devices. */
    private static final String ILLUSTRATION_02 = "access";

    private OnboardingDialogBridge(WindowAndroid windowAndroid, long nativeOnboardingDialogView) {
        mNativeOnboardingDialogView = nativeOnboardingDialogView;
        // TODO(crbug.com/983445): Get rid of this in favor of passing in direct dependencies.
        ChromeActivity activity = (ChromeActivity) windowAndroid.getActivity().get();
        mOnboardingDialog = new PasswordManagerDialogCoordinator(windowAndroid.getContext().get(),
                activity.getModalDialogManager(), activity.findViewById(android.R.id.content),
                activity.getFullscreenManager(), activity.getControlContainerHeightResource());
        mResources = activity.getResources();
    }

    @CalledByNative
    public static OnboardingDialogBridge create(WindowAndroid windowAndroid, long nativeDialog) {
        return new OnboardingDialogBridge(windowAndroid, nativeDialog);
    }

    /**
     * Choose the illustration shown based on an experiment parameter.
     * By default we show the story centered on not having to remember your password.
     */
    private int getDrawableResourceFromFeature() {
        String story = getFieldTrialParamByFeature(
                PASSWORD_MANAGER_ONBOARDING_ANDROID, EXPERIMENT_PARAMETER);
        switch (story) {
            case ILLUSTRATION_01:
                return R.drawable.password_manager_onboarding_illustration01;
            case ILLUSTRATION_02:
                return R.drawable.password_manager_onboarding_illustration02;
            default:
                return R.drawable.password_manager_onboarding_illustration03;
        }
    }

    @CalledByNative
    public void showDialog(String onboardingTitle, String onboardingDetails) {
        mOnboardingDialog.showDialog(onboardingTitle, onboardingDetails,
                getDrawableResourceFromFeature(), mResources.getString(R.string.continue_button),
                mResources.getString(R.string.not_now), this::onClick, true,
                ModalDialogManager.ModalDialogType.TAB);
    }

    @CalledByNative
    private void destroy() {
        mNativeOnboardingDialogView = 0;
        mOnboardingDialog.dismissDialog(DialogDismissalCause.DISMISSED_BY_NATIVE);
    }

    private void onClick(@DialogDismissalCause int dismissalCause) {
        if (mNativeOnboardingDialogView == 0) return;
        switch (dismissalCause) {
            case DialogDismissalCause.POSITIVE_BUTTON_CLICKED:
                OnboardingDialogBridgeJni.get().onboardingAccepted(
                        mNativeOnboardingDialogView, OnboardingDialogBridge.this);
                return;
            case DialogDismissalCause.NEGATIVE_BUTTON_CLICKED:
                OnboardingDialogBridgeJni.get().onboardingRejected(
                        mNativeOnboardingDialogView, OnboardingDialogBridge.this);
                return;

            default:
                OnboardingDialogBridgeJni.get().onboardingAborted(
                        mNativeOnboardingDialogView, OnboardingDialogBridge.this);
        }
    }

    @NativeMethods
    interface Natives {
        void onboardingAccepted(long nativeOnboardingDialogView, OnboardingDialogBridge caller);
        void onboardingRejected(long nativeOnboardingDialogView, OnboardingDialogBridge caller);
        void onboardingAborted(long nativeOnboardingDialogView, OnboardingDialogBridge caller);
    }
}

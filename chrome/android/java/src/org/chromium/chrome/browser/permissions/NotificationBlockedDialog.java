// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.permissions;

import android.content.Context;
import android.os.Bundle;
import android.text.SpannableStringBuilder;
import android.text.Spanned;
import android.text.method.LinkMovementMethod;
import android.view.ContextThemeWrapper;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.settings.SettingsLauncherImpl;
import org.chromium.components.browser_ui.settings.SettingsLauncher;
import org.chromium.components.browser_ui.site_settings.SingleCategorySettings;
import org.chromium.components.browser_ui.site_settings.SiteSettingsCategory;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modaldialog.ModalDialogProperties.ButtonType;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.text.NoUnderlineClickableSpan;

/**
 * Dialog triggered by the user clicking on the "manage" button in the Messages 2.0 flavor of quiet
 * permission prompt for notifications.
 */
public class NotificationBlockedDialog implements ModalDialogProperties.Controller {
    private final ModalDialogManager mModalDialogManager;
    private final Context mContext;
    private long mNativeDialogController;
    private PropertyModel mPropertyModel;

    @CalledByNative
    private static NotificationBlockedDialog create(
            long nativeDialogController, @NonNull WindowAndroid windowAndroid) {
        return new NotificationBlockedDialog(nativeDialogController, windowAndroid);
    }

    public NotificationBlockedDialog(long nativeDialogController, WindowAndroid windowAndroid) {
        mNativeDialogController = nativeDialogController;
        mContext = windowAndroid.getActivity().get();

        mModalDialogManager = windowAndroid.getModalDialogManager();
    }

    @CalledByNative
    void show(String title, String content, String positiveButtonLabel, String negativeButtonLabel,
            @Nullable String learnMoreText) {
        SpannableStringBuilder fullString = new SpannableStringBuilder();

        TextView message = new TextView(
                new ContextThemeWrapper(mContext, R.style.NotificationBlockedDialogContent));
        fullString.append(content);
        if (learnMoreText != null) {
            fullString.append(" ");
            int start = fullString.length();
            fullString.append(learnMoreText);
            fullString.setSpan(new NoUnderlineClickableSpan(mContext, (v) -> {
                NotificationBlockedDialogJni.get().onLearnMoreClicked(mNativeDialogController);
            }), start, fullString.length(), Spanned.SPAN_EXCLUSIVE_EXCLUSIVE);
        }

        message.setText(fullString);
        message.setMovementMethod(LinkMovementMethod.getInstance());

        mPropertyModel =
                new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                        .with(ModalDialogProperties.CONTROLLER, this)
                        .with(ModalDialogProperties.TITLE, title)
                        .with(ModalDialogProperties.CUSTOM_VIEW, message)
                        .with(ModalDialogProperties.POSITIVE_BUTTON_TEXT, positiveButtonLabel)
                        .with(ModalDialogProperties.NEGATIVE_BUTTON_TEXT, negativeButtonLabel)
                        .with(ModalDialogProperties.CANCEL_ON_TOUCH_OUTSIDE, true)
                        .build();
        mModalDialogManager.showDialog(mPropertyModel, ModalDialogType.APP);
    }

    @Override
    public void onClick(PropertyModel model, @ButtonType int buttonType) {
        if (buttonType == ButtonType.POSITIVE) {
            NotificationBlockedDialogJni.get().onPrimaryButtonClicked(mNativeDialogController);
            mModalDialogManager.dismissDialog(
                    mPropertyModel, DialogDismissalCause.POSITIVE_BUTTON_CLICKED);
        } else if (buttonType == ButtonType.NEGATIVE) {
            NotificationBlockedDialogJni.get().onNegativeButtonClicked(mNativeDialogController);
            mModalDialogManager.dismissDialog(
                    mPropertyModel, DialogDismissalCause.NEGATIVE_BUTTON_CLICKED);
        }
    }

    @Override
    public void onDismiss(PropertyModel model, int dismissalCause) {
        NotificationBlockedDialogJni.get().onDialogDismissed(mNativeDialogController);
        mNativeDialogController = 0;
    }

    @CalledByNative
    private void dismissDialog() {
        mModalDialogManager.dismissDialog(mPropertyModel, DialogDismissalCause.DISMISSED_BY_NATIVE);
    }

    @CalledByNative
    private void showSettings() {
        Bundle fragmentArguments = new Bundle();
        fragmentArguments.putString(SingleCategorySettings.EXTRA_CATEGORY,
                SiteSettingsCategory.preferenceKey(SiteSettingsCategory.Type.NOTIFICATIONS));
        SettingsLauncher settingsLauncher = new SettingsLauncherImpl();
        settingsLauncher.launchSettingsActivity(
                mContext, SingleCategorySettings.class, fragmentArguments);
    }

    @NativeMethods
    interface Natives {
        void onPrimaryButtonClicked(long nativeNotificationBlockedDialogController);
        void onNegativeButtonClicked(long nativeNotificationBlockedDialogController);
        void onLearnMoreClicked(long nativeNotificationBlockedDialogController);
        void onDialogDismissed(long nativeNotificationBlockedDialogController);
    }
}

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.safe_browsing;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.TextView;

import androidx.annotation.UiThread;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.ThreadUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.UiUtils;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.text.ChromeClickableSpan;
import org.chromium.ui.text.SpanApplier;
import org.chromium.ui.text.SpanApplier.SpanInfo;
import org.chromium.ui.widget.ButtonCompat;
import org.chromium.ui.widget.ChromeImageButton;

/** JNI call glue between the native and Java for suspicious site dialogs. */
@JNINamespace("safe_browsing")
@NullMarked
public class SafeBrowsingSuspiciousSiteDialogBridge implements ModalDialogProperties.Controller {
    private long mNativeSuspiciousSiteDialogViewAndroid;
    private final WindowAndroid mWindowAndroid;
    private @Nullable PropertyModel mDialogModel;

    private SafeBrowsingSuspiciousSiteDialogBridge(
            WindowAndroid windowAndroid, long nativeSuspiciousSiteDialogViewAndroid) {
        mNativeSuspiciousSiteDialogViewAndroid = nativeSuspiciousSiteDialogViewAndroid;
        mWindowAndroid = windowAndroid;
    }

    public static SafeBrowsingSuspiciousSiteDialogBridge createForTests(
            WindowAndroid windowAndroid, long nativeSuspiciousSiteDialogViewAndroid) {
        return new SafeBrowsingSuspiciousSiteDialogBridge(
                windowAndroid, nativeSuspiciousSiteDialogViewAndroid);
    }

    @UiThread
    public static void createControllerForTesting(WebContents webContents) {
        ThreadUtils.assertOnUiThread();
        SafeBrowsingSuspiciousSiteDialogBridgeJni.get()
                .createControllerForTesting(webContents); // IN-TEST
    }

    @CalledByNative
    @UiThread
    public static SafeBrowsingSuspiciousSiteDialogBridge create(
            WindowAndroid windowAndroid, long nativeDialog) {
        ThreadUtils.assertOnUiThread();
        return new SafeBrowsingSuspiciousSiteDialogBridge(windowAndroid, nativeDialog);
    }

    @CalledByNative
    @UiThread
    public void showDialog(
            @JniType("std::u16string") String dialogTitle,
            @JniType("std::u16string") String dialogDetails,
            @JniType("std::u16string") String primaryButtonText,
            @JniType("std::u16string") String secondaryButtonText) {
        ThreadUtils.assertOnUiThread();
        if (mNativeSuspiciousSiteDialogViewAndroid == 0) return;

        Context context = mWindowAndroid.getContext().get();
        ModalDialogManager modalDialogManager = mWindowAndroid.getModalDialogManager();
        if (mWindowAndroid.getActivity().get() == null
                || modalDialogManager == null
                || context == null) {
            long nativePtr = mNativeSuspiciousSiteDialogViewAndroid;
            mNativeSuspiciousSiteDialogViewAndroid = 0;
            SafeBrowsingSuspiciousSiteDialogBridgeJni.get()
                    .close(nativePtr, DialogDismissalCause.ACTIVITY_DESTROYED);
            return;
        }

        View customView =
                LayoutInflater.from(context).inflate(R.layout.suspicious_site_dialog_view, null);

        TextView titleView = customView.findViewById(R.id.title);
        titleView.setText(dialogTitle);

        // TODO(b/552482499): Migrate to centralized framework close button
        // in Phase 2.
        ChromeImageButton closeButton = customView.findViewById(R.id.close_button);
        closeButton.setOnClickListener(
                v -> {
                    PropertyModel model = mDialogModel;
                    if (model != null) {
                        modalDialogManager.dismissDialog(
                                model, DialogDismissalCause.ACTION_ON_CONTENT);
                    }
                });

        CharSequence spannableDetails =
                SpanApplier.applySpans(
                        dialogDetails,
                        new SpanInfo(
                                "<link>",
                                "</link>",
                                new ChromeClickableSpan(
                                        context,
                                        v -> {
                                            long nativePtr = mNativeSuspiciousSiteDialogViewAndroid;
                                            if (nativePtr != 0) {
                                                SafeBrowsingSuspiciousSiteDialogBridgeJni.get()
                                                        .onLearnMoreClicked(nativePtr);
                                            }
                                        })));

        TextView messageView = customView.findViewById(R.id.message);
        messageView.setText(spannableDetails);
        UiUtils.maybeSetLinkMovementMethod(messageView);

        ButtonCompat negativeButton = customView.findViewById(R.id.negative_button);
        negativeButton.setText(secondaryButtonText);
        negativeButton.setOnClickListener(
                v -> {
                    PropertyModel model = mDialogModel;
                    if (model != null) {
                        modalDialogManager.dismissDialog(
                                model, DialogDismissalCause.NEGATIVE_BUTTON_CLICKED);
                    }
                });

        ButtonCompat positiveButton = customView.findViewById(R.id.positive_button);
        positiveButton.setText(primaryButtonText);
        positiveButton.setOnClickListener(
                v -> {
                    PropertyModel model = mDialogModel;
                    if (model != null) {
                        modalDialogManager.dismissDialog(
                                model, DialogDismissalCause.POSITIVE_BUTTON_CLICKED);
                    }
                });

        int horizontalMargin =
                context.getResources()
                        .getDimensionPixelSize(R.dimen.modal_dialog_view_external_margin);
        mDialogModel =
                new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                        .with(ModalDialogProperties.CONTROLLER, this)
                        .with(ModalDialogProperties.CUSTOM_VIEW, customView)
                        .with(ModalDialogProperties.CONTENT_DESCRIPTION, dialogTitle)
                        .with(
                                ModalDialogProperties.DIALOG_STYLES,
                                ModalDialogProperties.DialogStyles.DIALOG_WHEN_LARGE)
                        .with(ModalDialogProperties.CANCEL_ON_TOUCH_OUTSIDE, false)
                        .with(ModalDialogProperties.HORIZONTAL_MARGIN, horizontalMargin)
                        .build();

        modalDialogManager.showDialog(mDialogModel, ModalDialogManager.ModalDialogType.TAB);
    }

    @CalledByNative
    @UiThread
    private void destroy() {
        ThreadUtils.assertOnUiThread();
        mNativeSuspiciousSiteDialogViewAndroid = 0;
        PropertyModel model = mDialogModel;
        mDialogModel = null;
        ModalDialogManager manager = mWindowAndroid.getModalDialogManager();
        if (model != null && manager != null) {
            manager.dismissDialog(model, DialogDismissalCause.DISMISSED_BY_NATIVE);
        }
    }

    @Override
    @UiThread
    public void onClick(PropertyModel model, @ModalDialogProperties.ButtonType int buttonType) {
        ThreadUtils.assertOnUiThread();
        // Handled directly via custom view buttons.
    }

    @Override
    @UiThread
    public void onDismiss(PropertyModel model, @DialogDismissalCause int dismissalCause) {
        ThreadUtils.assertOnUiThread();
        if (mNativeSuspiciousSiteDialogViewAndroid == 0) return;

        long nativePtr = mNativeSuspiciousSiteDialogViewAndroid;
        mNativeSuspiciousSiteDialogViewAndroid = 0;
        mDialogModel = null;

        if (dismissalCause == DialogDismissalCause.POSITIVE_BUTTON_CLICKED) {
            SafeBrowsingSuspiciousSiteDialogBridgeJni.get().goBack(nativePtr);
        } else if (dismissalCause == DialogDismissalCause.NEGATIVE_BUTTON_CLICKED) {
            SafeBrowsingSuspiciousSiteDialogBridgeJni.get().continueAnyway(nativePtr);
        } else {
            SafeBrowsingSuspiciousSiteDialogBridgeJni.get().close(nativePtr, dismissalCause);
        }
    }

    @NativeMethods
    interface Natives {
        void continueAnyway(long nativeSuspiciousSiteDialogViewAndroid);

        void goBack(long nativeSuspiciousSiteDialogViewAndroid);

        void onLearnMoreClicked(long nativeSuspiciousSiteDialogViewAndroid);

        void close(
                long nativeSuspiciousSiteDialogViewAndroid,
                @JniType("ui::ModalDialogWrapper::DismissalCause") int dismissalCause);

        void createControllerForTesting(@JniType("content::WebContents*") WebContents webContents);
    }
}

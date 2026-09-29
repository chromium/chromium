// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.safe_browsing;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.TextView;

import androidx.annotation.UiThread;
import androidx.annotation.VisibleForTesting;

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
import org.chromium.ui.base.DeviceFormFactor;
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

        CharSequence spannableDetails = applyLearnMoreLink(context, dialogDetails);

        // Large form factor devices (tablets and desktop Android) use the framework's
        // standard dialog (title, message and trailing button bar) instead of the
        // phone-optimized custom view.
        mDialogModel =
                DeviceFormFactor.isNonMultiDisplayContextOnTablet(context)
                        ? buildStandardDialogModel(
                                dialogTitle,
                                spannableDetails,
                                primaryButtonText,
                                secondaryButtonText)
                        : buildCustomViewDialogModel(
                                context,
                                modalDialogManager,
                                dialogTitle,
                                spannableDetails,
                                primaryButtonText,
                                secondaryButtonText);

        modalDialogManager.showDialog(mDialogModel, ModalDialogManager.ModalDialogType.TAB);
    }

    /** Applies the "learn more" clickable span to the {@code <link>} tags in the dialog body. */
    private CharSequence applyLearnMoreLink(Context context, String dialogDetails) {
        return SpanApplier.applySpans(
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
    }

    /**
     * Builds the standard framework dialog model used on large form factors (tablets and desktop
     * Android). The framework renders the title, body and buttons, so there is no close 'x': the
     * negative button is the explicit dismiss affordance.
     */
    private PropertyModel buildStandardDialogModel(
            String dialogTitle,
            CharSequence spannableDetails,
            String primaryButtonText,
            String secondaryButtonText) {
        // CONTENT_DESCRIPTION is intentionally omitted here: TITLE already satisfies
        // TabModalPresenter's assertion and sets the accessibility pane title, whereas setting
        // CONTENT_DESCRIPTION on the screen-reader-focusable root view would cause TalkBack to
        // announce only the title and skip MESSAGE_PARAGRAPH_1.
        // Note: ModalDialogView#setMessageParagraphs automatically applies
        // UiUtils.maybeSetLinkMovementMethod to MESSAGE_PARAGRAPH_1 paragraphs.
        return new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                .with(ModalDialogProperties.CONTROLLER, this)
                .with(ModalDialogProperties.TITLE, dialogTitle)
                .with(ModalDialogProperties.MESSAGE_PARAGRAPH_1, spannableDetails)
                .with(ModalDialogProperties.POSITIVE_BUTTON_TEXT, primaryButtonText)
                .with(ModalDialogProperties.NEGATIVE_BUTTON_TEXT, secondaryButtonText)
                .with(
                        ModalDialogProperties.BUTTON_STYLES,
                        ModalDialogProperties.ButtonStyles.PRIMARY_FILLED_NEGATIVE_OUTLINE)
                .with(ModalDialogProperties.CANCEL_ON_TOUCH_OUTSIDE, false)
                .build();
    }

    /** Builds the phone dialog model backed by the Clank custom view. */
    private PropertyModel buildCustomViewDialogModel(
            Context context,
            ModalDialogManager modalDialogManager,
            String dialogTitle,
            CharSequence spannableDetails,
            String primaryButtonText,
            String secondaryButtonText) {
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
        return new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                .with(ModalDialogProperties.CONTROLLER, this)
                .with(ModalDialogProperties.CUSTOM_VIEW, customView)
                .with(ModalDialogProperties.CONTENT_DESCRIPTION, dialogTitle)
                .with(
                        ModalDialogProperties.DIALOG_STYLES,
                        ModalDialogProperties.DialogStyles.DIALOG_WHEN_LARGE)
                .with(ModalDialogProperties.CANCEL_ON_TOUCH_OUTSIDE, false)
                .with(ModalDialogProperties.HORIZONTAL_MARGIN, horizontalMargin)
                .build();
    }

    @VisibleForTesting
    public @Nullable PropertyModel getDialogModelForTesting() {
        return mDialogModel;
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
        // ModalDialogView does not auto-dismiss on button clicks; Controller#onClick must call
        // dismissDialog() explicitly (see SimpleModalDialogController#onClick).
        ModalDialogManager modalDialogManager = mWindowAndroid.getModalDialogManager();
        if (modalDialogManager == null) return;

        if (buttonType == ModalDialogProperties.ButtonType.POSITIVE) {
            modalDialogManager.dismissDialog(model, DialogDismissalCause.POSITIVE_BUTTON_CLICKED);
        } else if (buttonType == ModalDialogProperties.ButtonType.NEGATIVE) {
            modalDialogManager.dismissDialog(model, DialogDismissalCause.NEGATIVE_BUTTON_CLICKED);
        }
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

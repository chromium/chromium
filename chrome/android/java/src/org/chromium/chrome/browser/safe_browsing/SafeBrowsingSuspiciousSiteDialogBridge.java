// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.safe_browsing;

import android.content.Context;
import android.content.res.ColorStateList;
import android.graphics.Typeface;
import android.text.Spannable;
import android.text.SpannableString;
import android.text.style.ClickableSpan;
import android.text.style.StyleSpan;
import android.text.style.TextAppearanceSpan;
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
import org.chromium.components.browser_ui.modaldialog.R;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.UiUtils;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.text.SpanApplier;
import org.chromium.ui.text.SpanApplier.SpanInfo;
import org.chromium.ui.widget.ButtonCompat;

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

    public static void createControllerForTesting(WebContents webContents) {
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
        if (mWindowAndroid.getActivity().get() == null) {
            long nativePtr = mNativeSuspiciousSiteDialogViewAndroid;
            mNativeSuspiciousSiteDialogViewAndroid = 0;
            SafeBrowsingSuspiciousSiteDialogBridgeJni.get()
                    .close(nativePtr, DialogDismissalCause.ACTIVITY_DESTROYED);
            return;
        }
        var modalDialogManager = mWindowAndroid.getModalDialogManager();
        if (modalDialogManager == null) {
            long nativePtr = mNativeSuspiciousSiteDialogViewAndroid;
            mNativeSuspiciousSiteDialogViewAndroid = 0;
            SafeBrowsingSuspiciousSiteDialogBridgeJni.get()
                    .close(nativePtr, DialogDismissalCause.ACTIVITY_DESTROYED);
            return;
        }

        Context context = mWindowAndroid.getContext().get();
        if (context == null) {
            long nativePtr = mNativeSuspiciousSiteDialogViewAndroid;
            mNativeSuspiciousSiteDialogViewAndroid = 0;
            SafeBrowsingSuspiciousSiteDialogBridgeJni.get()
                    .close(nativePtr, DialogDismissalCause.ACTIVITY_DESTROYED);
            return;
        }

        ModalDialogProperties.ModalDialogButtonSpec[] buttonSpecList =
                new ModalDialogProperties.ModalDialogButtonSpec[] {
                    new ModalDialogProperties.ModalDialogButtonSpec(
                            ModalDialogProperties.ButtonType.NEGATIVE, secondaryButtonText),
                    new ModalDialogProperties.ModalDialogButtonSpec(
                            ModalDialogProperties.ButtonType.POSITIVE, primaryButtonText)
                };

        var spannableDetails =
                new SpannableString(
                        SpanApplier.applySpans(
                                dialogDetails,
                                new SpanInfo(
                                        "<link>",
                                        "</link>",
                                        new ClickableSpan() {
                                            @Override
                                            public void onClick(View v) {
                                                if (mNativeSuspiciousSiteDialogViewAndroid != 0) {
                                                    SafeBrowsingSuspiciousSiteDialogBridgeJni.get()
                                                            .onLearnMoreClicked(
                                                                    mNativeSuspiciousSiteDialogViewAndroid);
                                                }
                                            }
                                        })));

        TextView messageView =
                (TextView)
                        LayoutInflater.from(context)
                                .inflate(R.layout.modal_dialog_paragraph_view, null);
        messageView.setText(spannableDetails);
        messageView.setPaddingRelative(
                messageView.getPaddingStart(),
                0,
                messageView.getPaddingEnd(),
                messageView.getPaddingBottom());
        UiUtils.maybeSetLinkMovementMethod(messageView);
        messageView.addOnAttachStateChangeListener(
                new View.OnAttachStateChangeListener() {
                    @Override
                    public void onViewAttachedToWindow(View v) {
                        v.post(
                                () -> {
                                    View rootView = v.getRootView();
                                    View positiveButton =
                                            rootView.findViewWithTag("ModalDialogButton0");
                                    if (positiveButton == null) {
                                        android.view.ViewGroup buttonGroup =
                                                rootView.findViewById(R.id.button_group);
                                        if (buttonGroup != null
                                                && buttonGroup.getChildCount() > 1) {
                                            positiveButton = buttonGroup.getChildAt(1);
                                        }
                                    }
                                    if (positiveButton instanceof ButtonCompat) {
                                        ButtonCompat button = (ButtonCompat) positiveButton;
                                        button.setButtonColor(
                                                ColorStateList.valueOf(
                                                        SemanticColorUtils.getFilledButtonBgColor(
                                                                context)));
                                        button.setTextColor(
                                                SemanticColorUtils.getDefaultTextColorOnAccent1(
                                                        context));
                                        button.setTypeface(button.getTypeface(), Typeface.BOLD);
                                    }
                                });
                    }

                    @Override
                    public void onViewDetachedFromWindow(View v) {}
                });

        SpannableString styledTitle = new SpannableString(dialogTitle);
        styledTitle.setSpan(
                new TextAppearanceSpan(context, org.chromium.ui.R.style.TextAppearance_TextLarge),
                0,
                styledTitle.length(),
                Spannable.SPAN_EXCLUSIVE_EXCLUSIVE);
        styledTitle.setSpan(
                new StyleSpan(Typeface.BOLD),
                0,
                styledTitle.length(),
                Spannable.SPAN_EXCLUSIVE_EXCLUSIVE);

        mDialogModel =
                new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                        .with(ModalDialogProperties.CONTROLLER, this)
                        .with(ModalDialogProperties.TITLE, styledTitle)
                        .with(ModalDialogProperties.CUSTOM_VIEW, messageView)
                        .with(ModalDialogProperties.WRAP_CUSTOM_VIEW_IN_SCROLLABLE, true)
                        .with(ModalDialogProperties.BUTTON_GROUP_BUTTON_SPEC_LIST, buttonSpecList)
                        .with(ModalDialogProperties.CONTENT_DESCRIPTION, dialogTitle)
                        .with(ModalDialogProperties.TITLE_CLOSE_BUTTON_VISIBLE, true)
                        .with(
                                ModalDialogProperties.TITLE_CLOSE_BUTTON_CLICK_LISTENER,
                                (v) -> {
                                    PropertyModel model = mDialogModel;
                                    if (model != null) {
                                        modalDialogManager.dismissDialog(
                                                model, DialogDismissalCause.ACTION_ON_CONTENT);
                                    }
                                })
                        .build();

        modalDialogManager.showDialog(mDialogModel, ModalDialogManager.ModalDialogType.TAB);
    }

    @CalledByNative
    private void destroy() {
        ThreadUtils.assertOnUiThread();
        mNativeSuspiciousSiteDialogViewAndroid = 0;
        PropertyModel model = mDialogModel;
        var manager = mWindowAndroid.getModalDialogManager();
        if (model != null && manager != null) {
            manager.dismissDialog(model, DialogDismissalCause.DISMISSED_BY_NATIVE);
        }
    }

    @Override
    public void onClick(PropertyModel model, @ModalDialogProperties.ButtonType int buttonType) {
        if (mNativeSuspiciousSiteDialogViewAndroid == 0) return;

        if (buttonType == ModalDialogProperties.ButtonType.POSITIVE) {
            SafeBrowsingSuspiciousSiteDialogBridgeJni.get()
                    .goBack(mNativeSuspiciousSiteDialogViewAndroid);
        } else if (buttonType == ModalDialogProperties.ButtonType.NEGATIVE) {
            SafeBrowsingSuspiciousSiteDialogBridgeJni.get()
                    .continueAnyway(mNativeSuspiciousSiteDialogViewAndroid);
        }
    }

    @Override
    public void onDismiss(PropertyModel model, @DialogDismissalCause int dismissalCause) {
        if (mNativeSuspiciousSiteDialogViewAndroid == 0) return;

        long nativePtr = mNativeSuspiciousSiteDialogViewAndroid;
        mNativeSuspiciousSiteDialogViewAndroid = 0;
        mDialogModel = null;

        if (dismissalCause != DialogDismissalCause.POSITIVE_BUTTON_CLICKED
                && dismissalCause != DialogDismissalCause.NEGATIVE_BUTTON_CLICKED) {
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

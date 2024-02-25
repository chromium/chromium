// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.translate;

import android.app.Activity;
import android.content.res.Resources;
import android.graphics.drawable.Drawable;

import androidx.annotation.VisibleForTesting;
import androidx.appcompat.content.res.AppCompatResources;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.LocaleUtils;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManagerProvider;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;

import java.lang.ref.WeakReference;
import java.util.Locale;

/** Manages the auto-translate Snackbar */
@JNINamespace("translate")
class AutoTranslateSnackbarController implements SnackbarManager.SnackbarController {
    private static final int AUTO_TRANSLATE_SNACKBAR_DURATION_MS = 4000;

    private WeakReference<Activity> mActivity;
    private long mNativeAutoTranslateSnackbarController;
    private SnackbarManager mSnackbarManager;

    @VisibleForTesting
    static class TargetLanguageData {
        private String mTargetLanguage;

        TargetLanguageData(String targetLanguage) {
            mTargetLanguage = targetLanguage;
        }

        public String getTargetLanguage() {
            return mTargetLanguage;
        }
    }

    AutoTranslateSnackbarController(
            WeakReference<Activity> activity,
            SnackbarManager snackbarManager,
            long nativeAutoTranslateSnackbarController) {
        mActivity = activity;
        mSnackbarManager = snackbarManager;
        mNativeAutoTranslateSnackbarController = nativeAutoTranslateSnackbarController;
    }

    // SnackbarController implementation.
    // Called when the user clicks the undo button.
    @Override
    public void onAction(Object actionData) {
        if (!(actionData instanceof TargetLanguageData)) return;
        if (mNativeAutoTranslateSnackbarController == 0) return;

        TargetLanguageData targetLanguageData = (TargetLanguageData) actionData;

        AutoTranslateSnackbarControllerJni.get()
                .onUndoActionPressed(
                        mNativeAutoTranslateSnackbarController,
                        targetLanguageData.getTargetLanguage());
    }

    // Called when the snackbar is dismissed by timeout or UI environment change.
    @Override
    public void onDismissNoAction(Object actionData) {
        if (mNativeAutoTranslateSnackbarController == 0) return;
        AutoTranslateSnackbarControllerJni.get()
                .onDismissNoAction(mNativeAutoTranslateSnackbarController);
    }

    // Class methods called by native.
    // Create and show a Snackbar for the given target language code.
    @CalledByNative
    public void show(String targetLanguage) {
        Resources resources = mActivity.get().getResources();

        Locale targetLocale =
                LocaleUtils.getUpdatedLocaleForChromium(Locale.forLanguageTag(targetLanguage));
        String targetLanguageName = targetLocale.getDisplayLanguage();
        Drawable icon =
                AppCompatResources.getDrawable(
                        mActivity.get(), R.drawable.infobar_translate_compact);

        TargetLanguageData targetLanguageData = new TargetLanguageData(targetLanguage);
        String title =
                resources.getString(
                        R.string.translate_message_snackbar_page_translated, targetLanguageName);

        Snackbar snackbar =
                Snackbar.make(title, this, Snackbar.TYPE_NOTIFICATION, Snackbar.UMA_AUTO_TRANSLATE)
                        .setSingleLine(false)
                        .setProfileImage(icon)
                        .setDuration(AUTO_TRANSLATE_SNACKBAR_DURATION_MS)
                        .setAction(resources.getString(R.string.undo), targetLanguageData);

        mSnackbarManager.showSnackbar(snackbar);
    }

    @CalledByNative
    // Hide the Snackbar by calling the dismiss on the {@link SnackbarManager}
    public void dismiss() {
        mSnackbarManager.dismissSnackbars(this);
    }

    // Static methods called by native.
    /** Create a new AutoTranslateSnackbarController */
    @CalledByNative
    public static AutoTranslateSnackbarController create(
            WebContents webContents, long nativeAutoTranslateSnackbarController) {
        WindowAndroid window = webContents.getTopLevelNativeWindow();
        if (window == null) return null;

        WeakReference<Activity> activity = window.getActivity();
        if (activity == null) return null;

        SnackbarManager snackbarManager = SnackbarManagerProvider.from(window);
        if (snackbarManager == null) return null;

        return new AutoTranslateSnackbarController(
                activity, snackbarManager, nativeAutoTranslateSnackbarController);
    }

    @NativeMethods
    interface Natives {
        void onUndoActionPressed(long nativeAutoTranslateSnackbarController, String targetLanguage);

        void onDismissNoAction(long nativeAutoTranslateSnackbarController);
    }
}

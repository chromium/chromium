// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_sandbox;

import android.app.Dialog;
import android.content.Context;
import android.view.ViewGroup;

import org.chromium.base.version_info.VersionInfo;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.embedder_support.delegate.WebContentsDelegateAndroid;
import org.chromium.components.embedder_support.view.ContentView;
import org.chromium.components.thinwebview.ThinWebView;
import org.chromium.components.thinwebview.ThinWebViewConstraints;
import org.chromium.components.thinwebview.ThinWebViewFactory;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.ActivityWindowAndroid;
import org.chromium.ui.base.ViewAndroidDelegate;

import java.lang.ref.WeakReference;
import java.util.HashMap;
import java.util.Locale;
import java.util.Map;

/** Controller for the dialog shown for the Privacy Sandbox. */
public class PrivacySandboxDialogController {
    private static WeakReference<Dialog> sDialog;
    private static boolean sDisableAnimations;
    private static boolean sDisableEEANoticeForTesting;
    private static LoadUrlParams sThinWebViewLoadUrlParams;

    public static boolean shouldShowPrivacySandboxDialog(Profile profile, int surfaceType) {
        assert profile != null;
        if (profile.isOffTheRecord()) {
            return false;
        }
        @PromptType
        int promptType = new PrivacySandboxBridge(profile).getRequiredPromptType(surfaceType);
        if (promptType != PromptType.M1_CONSENT
                && promptType != PromptType.M1_NOTICE_EEA
                && promptType != PromptType.M1_NOTICE_ROW
                && promptType != PromptType.M1_NOTICE_RESTRICTED) {
            return false;
        }
        return true;
    }

    public static ThinWebView createThinWebView(
            WebContents webContents,
            Profile profile,
            ActivityWindowAndroid activityWindowAndroid,
            String url) {
        ContentView contentView =
                ContentView.createContentView(
                        activityWindowAndroid.getContext().get(), webContents);
        webContents.setDelegates(
                VersionInfo.getProductVersion(),
                ViewAndroidDelegate.createBasicDelegate(contentView),
                contentView,
                activityWindowAndroid,
                WebContents.createDefaultInternalsHolder());
        sThinWebViewLoadUrlParams = new LoadUrlParams(url);
        Map<String, String> extraHeaders = new HashMap<>();
        extraHeaders.put("Accept-Language", Locale.getDefault().toLanguageTag());
        sThinWebViewLoadUrlParams.setExtraHeaders(extraHeaders);
        webContents.getNavigationController().loadUrl(sThinWebViewLoadUrlParams);
        ThinWebView thinWebView =
                ThinWebViewFactory.create(
                        activityWindowAndroid.getContext().get(),
                        new ThinWebViewConstraints(),
                        activityWindowAndroid.getIntentRequestTracker());
        thinWebView.attachWebContents(webContents, contentView, new WebContentsDelegateAndroid());
        thinWebView
                .getView()
                .setLayoutParams(
                        new ViewGroup.LayoutParams(
                                ViewGroup.LayoutParams.MATCH_PARENT,
                                ViewGroup.LayoutParams.MATCH_PARENT));
        return thinWebView;
    }

    /** Launches an appropriate dialog if necessary and returns whether that happened. */
    public static boolean maybeLaunchPrivacySandboxDialog(
            Context context,
            Profile profile,
            int surfaceType,
            ActivityWindowAndroid activityWindowAndroid) {
        assert profile != null;
        if (profile.isOffTheRecord()) {
            return false;
        }
        PrivacySandboxBridge privacySandboxBridge = new PrivacySandboxBridge(profile);
        @PromptType int promptType = privacySandboxBridge.getRequiredPromptType(surfaceType);
        Dialog dialog = null;
        switch (promptType) {
            case PromptType.NONE:
                return false;
            case PromptType.M1_CONSENT:
                dialog =
                        new PrivacySandboxDialogConsentEEA(
                                context,
                                privacySandboxBridge,
                                sDisableAnimations,
                                surfaceType,
                                profile,
                                activityWindowAndroid);
                dialog.show();
                sDialog = new WeakReference<>(dialog);
                return true;
            case PromptType.M1_NOTICE_EEA:
                showNoticeEEA(context, privacySandboxBridge, surfaceType);
                return true;
            case PromptType.M1_NOTICE_ROW:
                dialog =
                        new PrivacySandboxDialogNoticeROW(
                                context, privacySandboxBridge, surfaceType);
                dialog.show();
                sDialog = new WeakReference<>(dialog);
                return true;
            case PromptType.M1_NOTICE_RESTRICTED:
                dialog =
                        new PrivacySandboxDialogNoticeRestricted(
                                context, privacySandboxBridge, surfaceType);
                dialog.show();
                sDialog = new WeakReference<>(dialog);
                return true;
            default:
                assert false : "Unknown PromptType value.";
                // Should not be reached.
                return false;
        }
    }

    /** Shows the NoticeEEA dialog. */
    public static void showNoticeEEA(
            Context context,
            PrivacySandboxBridge privacySandboxBridge,
            @SurfaceType int surfaceType) {
        if (!sDisableEEANoticeForTesting) {
            Dialog dialog;
            dialog = new PrivacySandboxDialogNoticeEEA(context, privacySandboxBridge, surfaceType);
            dialog.show();
            sDialog = new WeakReference<>(dialog);
        }
    }

    static Dialog getDialogForTesting() {
        return sDialog != null ? sDialog.get() : null;
    }

    static void disableAnimationsForTesting(boolean disable) {
        sDisableAnimations = disable;
    }

    static void disableEEANoticeForTesting(boolean disable) {
        sDisableEEANoticeForTesting = disable;
    }

    static LoadUrlParams getThinWebViewLoadUrlParamsForTesting() {
        return sThinWebViewLoadUrlParams;
    }
}

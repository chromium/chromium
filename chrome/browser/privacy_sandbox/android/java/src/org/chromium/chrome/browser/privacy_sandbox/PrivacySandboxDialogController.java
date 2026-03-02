// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_sandbox;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.app.Activity;
import android.app.Dialog;
import android.content.Context;
import android.view.ViewGroup;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.version_info.VersionInfo;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
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
import org.chromium.ui.util.ColorUtils;

import java.lang.ref.WeakReference;
import java.util.Locale;

/** Controller for the dialog shown for the Privacy Sandbox. */
@NullMarked
public class PrivacySandboxDialogController {
    private static @Nullable WeakReference<Dialog> sDialog;
    private static boolean sShowMoreButtonForTesting;

    public static boolean shouldShowPrivacySandboxDialog(Profile profile, int surfaceType) {
        assert profile != null;
        if (profile.isOffTheRecord()) {
            return false;
        }
        @PromptType
        int promptType = new PrivacySandboxBridge(profile).getRequiredPromptType(surfaceType);
        if (promptType != PromptType.M1_NOTICE_RESTRICTED) {
            return false;
        }
        return true;
    }

    public static ThinWebView createPrivacyPolicyThinWebView(
            WebContents webContents, Profile profile, ActivityWindowAndroid activityWindowAndroid) {
        Context context = activityWindowAndroid.getContext().get();
        assumeNonNull(context);
        ContentView contentView = ContentView.createContentView(context, webContents);
        webContents.setDelegates(
                VersionInfo.getProductVersion(),
                ViewAndroidDelegate.createBasicDelegate(contentView),
                contentView,
                activityWindowAndroid,
                WebContents.createDefaultInternalsHolder());
        PrivacySandboxBridge privacySandboxBridge = new PrivacySandboxBridge(profile);
        String privacyPolicyUrl =
                privacySandboxBridge.getEmbeddedPrivacyPolicyURL(
                        privacySandboxBridge.shouldUsePrivacyPolicyChinaDomain()
                                ? PrivacyPolicyDomainType.CHINA
                                : PrivacyPolicyDomainType.NON_CHINA,
                        ColorUtils.inNightMode(context)
                                ? PrivacyPolicyColorScheme.DARK_MODE
                                : PrivacyPolicyColorScheme.LIGHT_MODE,
                        Locale.getDefault().toLanguageTag());
        assumeNonNull(activityWindowAndroid.getIntentRequestTracker());
        webContents.getNavigationController().loadUrl(new LoadUrlParams(privacyPolicyUrl));
        ThinWebView thinWebView =
                ThinWebViewFactory.create(
                        context,
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
            Activity activity,
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
            case PromptType.M1_NOTICE_RESTRICTED:
                dialog =
                        new PrivacySandboxDialogNoticeRestricted(
                                activity,
                                privacySandboxBridge,
                                surfaceType,
                                sShowMoreButtonForTesting);
                dialog.show();
                sDialog = new WeakReference<>(dialog);
                return true;
            default:
                assert false : "Unknown PromptType value.";
                // Should not be reached.
                return false;
        }
    }

    @VisibleForTesting
    static @Nullable Dialog getDialog() {
        return sDialog != null ? sDialog.get() : null;
    }

    @VisibleForTesting
    static void setShowMoreButton(boolean value) {
        sShowMoreButtonForTesting = value;
    }
}

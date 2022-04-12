// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import android.app.Activity;
import android.content.Context;

import androidx.annotation.DimenRes;
import androidx.annotation.Nullable;

import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.ui.TabObscuringHandler;
import org.chromium.chrome.browser.ui.TabObscuringHandlerSupplier;
import org.chromium.chrome.browser.util.ChromeAccessibilityUtil;
import org.chromium.components.autofill_assistant.AssistantAccessTokenUtil;
import org.chromium.components.autofill_assistant.AssistantDependencies;
import org.chromium.components.autofill_assistant.AssistantEditorFactory;
import org.chromium.components.autofill_assistant.AssistantFeedbackUtil;
import org.chromium.components.autofill_assistant.AssistantInfoPageUtil;
import org.chromium.components.autofill_assistant.AssistantProfileImageUtil;
import org.chromium.components.autofill_assistant.AssistantSettingsUtil;
import org.chromium.components.autofill_assistant.AssistantStaticDependencies;
import org.chromium.components.autofill_assistant.AssistantTabObscuringUtil;
import org.chromium.components.autofill_assistant.AssistantTabUtil;
import org.chromium.components.favicon.LargeIconBridge;
import org.chromium.components.image_fetcher.ImageFetcher;
import org.chromium.components.image_fetcher.ImageFetcherConfig;
import org.chromium.components.image_fetcher.ImageFetcherFactory;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.content_public.browser.BrowserContextHandle;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.util.AccessibilityUtil;

/**
 * Provides default implementations of {@link AssistantStaticDependencies} for Chrome.
 */
@JNINamespace("autofill_assistant")
public class AssistantStaticDependenciesChrome implements AssistantStaticDependencies {
    @Override
    public long createNative() {
        return AssistantStaticDependenciesChromeJni.get().init(
                new AssistantStaticDependenciesChrome());
    }

    @Override
    public AssistantDependencies createDependencies(Activity activity) {
        return new AssistantDependenciesChrome(activity);
    }

    @Override
    public AccessibilityUtil getAccessibilityUtil() {
        return ChromeAccessibilityUtil.get();
    }

    @Override
    @Nullable
    public AssistantTabObscuringUtil getTabObscuringUtilOrNull(WindowAndroid windowAndroid) {
        TabObscuringHandler tabObscuringHandler =
                TabObscuringHandlerSupplier.getValueOrNullFrom(windowAndroid);
        assert tabObscuringHandler != null;
        if (tabObscuringHandler == null) {
            return null;
        }

        return new AssistantTabObscuringUtilChrome(tabObscuringHandler);
    }

    @Override
    public AssistantInfoPageUtil createInfoPageUtil() {
        return new AssistantInfoPageUtilChrome();
    }

    @Override
    public AssistantFeedbackUtil createFeedbackUtil() {
        return new AssistantFeedbackUtilChrome();
    }

    @Override
    public AssistantTabUtil createTabUtil() {
        return new AssistantTabUtilChrome();
    }

    @Override
    public AssistantSettingsUtil createSettingsUtil() {
        return new AssistantSettingsUtilChrome();
    }

    @Override
    public AssistantAccessTokenUtil createAccessTokenUtil() {
        return new AssistantAccessTokenUtilChrome();
    }

    /**
     * Getter for the current profile while assistant is running. Since autofill assistant is only
     * available in regular mode and there is only one regular profile in android, this method
     * returns {@link Profile#getLastUsedRegularProfile()}.
     *
     * TODO(b/161519639): Return current profile to support multi profiles, instead of returning
     * always regular profile. This could be achieve by retrieving profile from native and using it
     * where the profile is needed on Java side.
     * @return The current regular profile.
     */
    private Profile getProfile() {
        return Profile.getLastUsedRegularProfile();
    }

    @Override
    public BrowserContextHandle getBrowserContext() {
        return getProfile();
    }

    @Override
    public ImageFetcher createImageFetcher() {
        return ImageFetcherFactory.createImageFetcher(
                ImageFetcherConfig.DISK_CACHE_ONLY, getProfile().getProfileKey());
    }

    @Override
    public LargeIconBridge createIconBridge() {
        return new LargeIconBridge(getProfile());
    }

    @Override
    @Nullable
    public String getSignedInAccountEmailOrNull() {
        IdentityManager identityManager =
                IdentityServicesProvider.get().getIdentityManager(getProfile());
        return CoreAccountInfo.getEmailFrom(
                identityManager.getPrimaryAccountInfo(ConsentLevel.SYNC));
    }

    @Override
    @Nullable
    public AssistantProfileImageUtil createProfileImageUtilOrNull(
            Context context, @DimenRes int imageSizeRedId) {
        String signedInAccountEmail = getSignedInAccountEmailOrNull();
        if (signedInAccountEmail == null) return null;

        return new AssistantProfileImageUtilChrome(context, signedInAccountEmail, imageSizeRedId);
    }

    @Override
    public AssistantEditorFactory createEditorFactory() {
        return new AssistantEditorFactoryChrome();
    }

    @NativeMethods
    interface Natives {
        long init(AssistantStaticDependencies staticDependencies);
    }
}

// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.extensions;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.lifetime.Destroyable;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.url_constants.ExtensionsUrlOverrideRegistry;
import org.chromium.components.embedder_support.util.UrlConstants;

/**
 * Listens to changes to the Native-level extensions URL registry and handles updates to Android
 * classes.
 */
@NullMarked
@JNINamespace("extensions")
public class ExtensionsUrlOverrideRegistryManager implements Destroyable {
    private long mNativePtr;

    public ExtensionsUrlOverrideRegistryManager(Profile profile) {
        mNativePtr = ExtensionsUrlOverrideRegistryManagerJni.get().initialize(this, profile);
    }

    @Override
    public void destroy() {
        if (mNativePtr != 0) {
            ExtensionsUrlOverrideRegistryManagerJni.get().destroy(mNativePtr);
            mNativePtr = 0;
        }
    }

    @CalledByNative
    public void onUrlOverrideEnabled(
            @JniType("std::string") String chromeUrlPath, boolean incognitoEnabled) {
        switch (chromeUrlPath) {
            case UrlConstants.NTP_HOST -> {
                ExtensionsUrlOverrideRegistry.setNtpOverrideEnabled(true);
                ExtensionsUrlOverrideRegistry.setIncognitoNtpOverrideEnabled(incognitoEnabled);
            }
            case UrlConstants.BOOKMARKS_HOST -> {
                ExtensionsUrlOverrideRegistry.setBookmarksPageOverrideEnabled(true);
                ExtensionsUrlOverrideRegistry.setIncognitoBookmarksPageOverrideEnabled(
                        incognitoEnabled);
            }
            case UrlConstants.HISTORY_HOST -> ExtensionsUrlOverrideRegistry
                    .setHistoryPageOverrideEnabled(true);
        }
    }

    @CalledByNative
    public void onUrlOverrideDisabled(@JniType("std::string") String chromeUrlPath) {
        switch (chromeUrlPath) {
            case UrlConstants.NTP_HOST -> {
                ExtensionsUrlOverrideRegistry.setNtpOverrideEnabled(false);
                ExtensionsUrlOverrideRegistry.setIncognitoNtpOverrideEnabled(false);
            }
            case UrlConstants.BOOKMARKS_HOST -> {
                ExtensionsUrlOverrideRegistry.setBookmarksPageOverrideEnabled(false);
                ExtensionsUrlOverrideRegistry.setIncognitoBookmarksPageOverrideEnabled(false);
            }
            case UrlConstants.HISTORY_HOST -> ExtensionsUrlOverrideRegistry
                    .setHistoryPageOverrideEnabled(false);
        }
    }

    @NativeMethods
    interface Natives {
        long initialize(
                ExtensionsUrlOverrideRegistryManager javaObject,
                @JniType("Profile*") Profile profile);

        void destroy(long nativeExtensionsUrlOverrideRegistryManager);
    }
}

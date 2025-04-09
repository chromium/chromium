// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safe_browsing;

import static org.chromium.build.NullUtil.assumeNonNull;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.components.messages.MessageDispatcher;
import org.chromium.components.messages.MessageDispatcherProvider;
import org.chromium.components.permissions.OsAdditionalSecurityPermissionProvider;
import org.chromium.components.permissions.OsAdditionalSecurityPermissionUtil;
import org.chromium.ui.base.WindowAndroid;

/** A class for showing UI whenever the Android-OS-supplied advanced-protection state changes. */
@NullMarked
public class AdvancedProtectionMediator implements OsAdditionalSecurityPermissionProvider.Observer {
    private WindowAndroid mWindowAndroid;

    public AdvancedProtectionMediator(WindowAndroid windowAndroid) {
        mWindowAndroid = windowAndroid;

        var provider = OsAdditionalSecurityPermissionUtil.getProviderInstance();
        if (provider != null) {
            provider.addObserver(this);
        }
    }

    public void destroy() {
        var provider = OsAdditionalSecurityPermissionUtil.getProviderInstance();
        if (provider != null) {
            provider.removeObserver(this);
        }
    }

    public boolean showMessageOnStartupIfNeeded() {
        var provider = OsAdditionalSecurityPermissionUtil.getProviderInstance();
        if (provider == null) return false;

        boolean cachedAdvancedProtectionSetting =
                ChromeSharedPreferences.getInstance()
                        .readBoolean(
                                ChromePreferenceKeys.DEFAULT_OS_ADVANCED_PROTECTION_SETTING,
                                /* defaultValue= */ false);
        if (cachedAdvancedProtectionSetting == provider.isAdvancedProtectionRequestedByOs()) {
            return false;
        }

        updatePref(provider);
        if (provider.isAdvancedProtectionRequestedByOs()) {
            enqueueMessage(provider);
        }
        return true;
    }

    @Override
    public void onAdvancedProtectionOsSettingChanged() {
        var provider = OsAdditionalSecurityPermissionUtil.getProviderInstance();
        if (provider == null) return;

        updatePref(provider);
        if (provider.isAdvancedProtectionRequestedByOs()) {
            enqueueMessage(provider);
        }
    }

    private void enqueueMessage(OsAdditionalSecurityPermissionProvider provider) {
        var context = assumeNonNull(mWindowAndroid.getContext().get());
        var propertyModel =
                provider.buildAdvancedProtectionMessagePropertyModel(
                        context, /* primaryButtonAction= */ null);
        if (propertyModel == null) {
            return;
        }
        MessageDispatcher dispatcher = MessageDispatcherProvider.from(mWindowAndroid);
        if (dispatcher != null) {
            dispatcher.enqueueWindowScopedMessage(propertyModel, /* highPriority= */ false);
        }
    }

    private void updatePref(OsAdditionalSecurityPermissionProvider provider) {
        ChromeSharedPreferences.getInstance()
                .writeBoolean(
                        ChromePreferenceKeys.DEFAULT_OS_ADVANCED_PROTECTION_SETTING,
                        provider.isAdvancedProtectionRequestedByOs());
    }
}

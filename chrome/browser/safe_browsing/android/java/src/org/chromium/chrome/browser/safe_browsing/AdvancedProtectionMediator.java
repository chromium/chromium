// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safe_browsing;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.os.Bundle;

import androidx.fragment.app.Fragment;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.privacy.settings.PrivacySettingsNavigation;
import org.chromium.chrome.browser.settings.SettingsNavigationFactory;
import org.chromium.components.messages.MessageDispatcher;
import org.chromium.components.messages.MessageDispatcherProvider;
import org.chromium.components.permissions.OsAdditionalSecurityPermissionProvider;
import org.chromium.components.permissions.OsAdditionalSecurityPermissionUtil;
import org.chromium.ui.base.WindowAndroid;

/** A class for showing UI whenever the Android-OS-supplied advanced-protection state changes. */
@NullMarked
public class AdvancedProtectionMediator implements OsAdditionalSecurityPermissionProvider.Observer {
    private final WindowAndroid mWindowAndroid;
    private final Class<? extends Fragment> mPrivacySettingsFragmentClass;
    private boolean mShouldShowMessageOnStartup;

    public AdvancedProtectionMediator(
            WindowAndroid windowAndroid, Class<? extends Fragment> privacySettingsFragmentClass) {
        mWindowAndroid = windowAndroid;
        mPrivacySettingsFragmentClass = privacySettingsFragmentClass;

        var provider = OsAdditionalSecurityPermissionUtil.getProviderInstance();
        if (provider != null) {
            provider.addObserver(this);

            boolean cachedAdvancedProtectionSetting =
                    ChromeSharedPreferences.getInstance()
                            .readBoolean(
                                    ChromePreferenceKeys.OS_ADVANCED_PROTECTION_SETTING,
                                    /* defaultValue= */ false);
            boolean advancedProtectionSetting = provider.isAdvancedProtectionRequestedByOs();
            if (cachedAdvancedProtectionSetting != advancedProtectionSetting) {
                updatePref(provider);
                mShouldShowMessageOnStartup = advancedProtectionSetting;
            }
        }

        recordStartupHistograms(provider);
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

        if (mShouldShowMessageOnStartup && provider.isAdvancedProtectionRequestedByOs()) {
            enqueueMessage(provider);
            return true;
        }
        return false;
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
        Runnable buttonHandler =
                () -> {
                    Bundle args = new Bundle();
                    args.putBoolean(
                            PrivacySettingsNavigation.EXTRA_FOCUS_ADVANCED_PROTECTION_SECTION,
                            true);
                    SettingsNavigationFactory.createSettingsNavigation()
                            .startSettings(context, mPrivacySettingsFragmentClass, args);
                };
        var propertyModel =
                provider.buildAdvancedProtectionMessagePropertyModel(
                        context, /* primaryButtonAction= */ buttonHandler);
        if (propertyModel == null) {
            return;
        }
        MessageDispatcher dispatcher = MessageDispatcherProvider.from(mWindowAndroid);
        if (dispatcher != null) {
            dispatcher.enqueueWindowScopedMessage(propertyModel, /* highPriority= */ false);
        }
    }

    private void updatePref(OsAdditionalSecurityPermissionProvider provider) {
        SharedPreferencesManager preferences = ChromeSharedPreferences.getInstance();
        preferences.writeBoolean(
                ChromePreferenceKeys.OS_ADVANCED_PROTECTION_SETTING,
                provider.isAdvancedProtectionRequestedByOs());
        preferences.writeLong(
                ChromePreferenceKeys.OS_ADVANCED_PROTECTION_SETTING_UPDATED_TIME,
                System.currentTimeMillis());
    }

    private void recordStartupHistograms(
            @Nullable OsAdditionalSecurityPermissionProvider provider) {
        RecordHistogram.recordBooleanHistogram(
                "SafeBrowsing.Android.AdvancedProtection.Enabled",
                provider != null && provider.isAdvancedProtectionRequestedByOs());
    }
}

// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.accessibility.settings;

import android.content.Context;
import android.os.Bundle;

import androidx.preference.PreferenceFragmentCompat;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.image_descriptions.ImageDescriptionsController;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.settings.SettingsLauncherImpl;
import org.chromium.components.browser_ui.accessibility.AccessibilitySettingsDelegate;
import org.chromium.components.browser_ui.accessibility.PageZoomUtils;
import org.chromium.components.browser_ui.settings.SettingsLauncher;
import org.chromium.components.browser_ui.site_settings.AllSiteSettings;
import org.chromium.components.browser_ui.site_settings.SingleCategorySettings;
import org.chromium.components.browser_ui.site_settings.SiteSettingsCategory;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.content_public.browser.BrowserContextHandle;

/** The Chrome implementation of AccessibilitySettingsDelegate. */
public class ChromeAccessibilitySettingsDelegate implements AccessibilitySettingsDelegate {
    private static class ReaderForAccessibilityDelegate implements BooleanPreferenceDelegate {
        private final Profile mProfile;

        ReaderForAccessibilityDelegate(Profile profile) {
            mProfile = profile;
        }

        @Override
        public boolean isEnabled() {
            return UserPrefs.get(mProfile).getBoolean(Pref.READER_FOR_ACCESSIBILITY);
        }

        @Override
        public void setEnabled(boolean value) {
            UserPrefs.get(mProfile).setBoolean(Pref.READER_FOR_ACCESSIBILITY, (Boolean) value);
        }
    }

    private static class TextSizeContrastAccessibilityDelegate
            implements IntegerPreferenceDelegate {
        private final BrowserContextHandle mBrowserContextHandle;

        public TextSizeContrastAccessibilityDelegate(BrowserContextHandle mBrowserContextHandle) {
            this.mBrowserContextHandle = mBrowserContextHandle;
        }

        @Override
        public int getValue() {
            return UserPrefs.get(mBrowserContextHandle)
                    .getInteger(Pref.ACCESSIBILITY_TEXT_SIZE_CONTRAST_FACTOR);
        }

        @Override
        public void setValue(int value) {
            UserPrefs.get(mBrowserContextHandle)
                    .setInteger(Pref.ACCESSIBILITY_TEXT_SIZE_CONTRAST_FACTOR, value);
        }
    }

    private final Profile mProfile;

    /**
     * Constructs a delegate for the given profile.
     * @param profile The profile associated with the delegate.
     */
    public ChromeAccessibilitySettingsDelegate(Profile profile) {
        mProfile = profile;
    }

    @Override
    public BrowserContextHandle getBrowserContextHandle() {
        return mProfile;
    }

    @Override
    public BooleanPreferenceDelegate getReaderForAccessibilityDelegate() {
        return new ReaderForAccessibilityDelegate(mProfile);
    }

    @Override
    public IntegerPreferenceDelegate getTextSizeContrastAccessibilityDelegate() {
        return new TextSizeContrastAccessibilityDelegate(getBrowserContextHandle());
    }

    @Override
    public void addExtraPreferences(PreferenceFragmentCompat fragment) {
        if (ImageDescriptionsController.getInstance().shouldShowImageDescriptionsMenuItem()) {
            fragment.addPreferencesFromResource(R.xml.image_descriptions_settings_preference);
        }
    }

    @Override
    public boolean showPageZoomSettingsUI() {
        return PageZoomUtils.shouldShowSettingsUI();
    }

    @Override
    public void launchSiteSettingsZoomActivity(Context context) {
        Bundle initialArguments = new Bundle();
        initialArguments.putString(SingleCategorySettings.EXTRA_CATEGORY,
                SiteSettingsCategory.preferenceKey(SiteSettingsCategory.Type.ZOOM));
        SettingsLauncher settingsLauncher = new SettingsLauncherImpl();
        settingsLauncher.launchSettingsActivity(context, AllSiteSettings.class, initialArguments);
    }
}

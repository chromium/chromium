// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.accessibility.settings;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.image_descriptions.ImageDescriptionsController;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.settings.SettingsNavigationFactory;
import org.chromium.components.browser_ui.accessibility.AccessibilitySettingsDelegate;
import org.chromium.components.browser_ui.settings.SettingsNavigation;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.content_public.browser.BrowserContextHandle;

/** The Chrome implementation of AccessibilitySettingsDelegate. */
@NullMarked
public class ChromeAccessibilitySettingsDelegate implements AccessibilitySettingsDelegate {
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

    private static class ChromeBooleanPreferenceDelegate implements BooleanPreferenceDelegate {
        private final BrowserContextHandle mBrowserContextHandle;
        private final String mPreferenceKey;

        public ChromeBooleanPreferenceDelegate(
                BrowserContextHandle mBrowserContextHandle, String mPreferenceKey) {
            this.mBrowserContextHandle = mBrowserContextHandle;
            this.mPreferenceKey = mPreferenceKey;
        }

        @Override
        public boolean getValue() {
            return UserPrefs.get(mBrowserContextHandle).getBoolean(mPreferenceKey);
        }

        @Override
        public void setValue(boolean value) {
            UserPrefs.get(mBrowserContextHandle).setBoolean(mPreferenceKey, value);
        }
    }

    private final Profile mProfile;

    /**
     * Constructs a delegate for the given profile.
     *
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
    public boolean shouldShowImageDescriptionsSetting() {
        return ImageDescriptionsController.getInstance().shouldShowImageDescriptionsMenuItem();
    }

    @Override
    public SettingsNavigation getSiteSettingsNavigation() {
        return SettingsNavigationFactory.createSettingsNavigation();
    }

    @Override
    public IntegerPreferenceDelegate getTextSizeContrastAccessibilityDelegate() {
        return new TextSizeContrastAccessibilityDelegate(getBrowserContextHandle());
    }

    @Override
    public BooleanPreferenceDelegate getForceEnableZoomAccessibilityDelegate() {
        return new ChromeBooleanPreferenceDelegate(
                getBrowserContextHandle(), Pref.ACCESSIBILITY_FORCE_ENABLE_ZOOM);
    }

    @Override
    public BooleanPreferenceDelegate getTouchpadOverscrollHistoryNavigationAccessibilityDelegate() {
        return new ChromeBooleanPreferenceDelegate(
                getBrowserContextHandle(),
                Pref.ACCESSIBILITY_TOUCHPAD_OVERSCROLL_HISTORY_NAVIGATION);
    }

    @Override
    public BooleanPreferenceDelegate getReaderAccessibilityDelegate() {
        return new ChromeBooleanPreferenceDelegate(
                getBrowserContextHandle(), Pref.READER_FOR_ACCESSIBILITY);
    }

    /**
     * Returns whether the material slider should be used for the page zoom preference.
     *
     * @return True if the slider should be used, false otherwise.
     */
    @Override
    public boolean shouldUseSlider() {
        return ChromeFeatureList.sAndroidSettingsContainment.isEnabled();
    }

    /**
     * Checks if the caret browsing feature is currently enabled for the associated profile.
     *
     * @return True if caret browsing is enabled, false otherwise.
     */
    @Override
    public boolean isCaretBrowsingEnabled() {
        return AccessibilitySettingsBridge.isCaretBrowsingEnabled(mProfile);
    }

    /**
     * Sets the enabled state of the caret browsing feature for the associated profile.
     *
     * @param enabled True to enable caret browsing, false to disable it.
     */
    @Override
    public void setCaretBrowsingEnabled(boolean enabled) {
        AccessibilitySettingsBridge.setCaretBrowsingEnabled(mProfile, enabled);
    }
}

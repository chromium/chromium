// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.language.settings;

import android.content.Context;
import android.text.TextUtils;
import android.util.AttributeSet;

import org.chromium.chrome.browser.language.AppLocaleUtils;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.browser_ui.settings.ChromeBasePreference;

/** Chrome Preference that enables selecting a single LanguageItem. */
public class LanguageItemPickerPreference extends ChromeBasePreference {
    private LanguageItem mLanguageItem;
    private boolean mUseLanguageItemForTitle;

    public LanguageItemPickerPreference(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    /**
     * Get the LanguageItem value for this preference.
     * @return LanguageItem saved to this preference.
     */
    public LanguageItem getLanguageItem() {
        return mLanguageItem;
    }

    /**
     * Set the LanguageItem value.
     * @param LanguageItem The LanguageItem to use for this preference
     */
    public void setLanguageItem(LanguageItem languageItem) {
        mLanguageItem = languageItem;
        updateDisplay();
    }

    /**
     * Set the LanguageItem value based on the string locale code. If null is used the system
     * default language is set as the preference's LanguageItem
     *
     * @param profile The Profile for the current session.
     * @param languageCode The iso639 languageCode for LanguageItem
     */
    public void setLanguageItem(Profile profile, String languageCode) {
        LanguageItem languageItem;
        if (AppLocaleUtils.isFollowSystemLanguage(languageCode)) {
            languageItem = LanguageItem.makeFollowSystemLanguageItem();
        } else {
            languageItem = LanguagesManager.getForProfile(profile).getLanguageItem(languageCode);
        }
        setLanguageItem(languageItem);
    }

    /**
     * By default only the summary text is synced to the LanguageItem. Enabling this will make the
     * preference title the display name and summary the native display name.
     * @param boolean use
     */
    public void useLanguageItemForTitle(boolean useForTitle) {
        mUseLanguageItemForTitle = useForTitle;
        updateDisplay();
    }

    /** Update the title and summary to display */
    private void updateDisplay() {
        if (mLanguageItem == null) {
            return;
        }
        String displayName = mLanguageItem.getDisplayName();
        if (mUseLanguageItemForTitle) {
            setTitle(displayName);
            String nativeName = mLanguageItem.getNativeDisplayName();
            if (TextUtils.equals(displayName, nativeName)) {
                // Clear summary if native name is the same as display name.
                setSummary("");
            } else {
                setSummary(nativeName);
            }
        } else {
            setSummary(displayName);
        }
    }
}

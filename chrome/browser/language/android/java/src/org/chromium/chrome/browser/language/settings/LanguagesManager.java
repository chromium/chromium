// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.language.settings;

import androidx.annotation.IntDef;

import org.chromium.base.LocaleUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.translate.TranslateBridge;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.Collection;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.TreeSet;

/**
 * Manages languages details for languages settings.
 *
 *The LanguagesManager is responsible for fetching languages details from native.
 */
public class LanguagesManager {
    /**
     * An observer interface that allows other classes to know when the accept language list is
     * updated in native side.
     */
    interface AcceptLanguageObserver {
        /**
         * Called when the accept languages for the current user are updated.
         */
        void onDataUpdated();
    }

    // Constants used to log UMA enum histogram, must stay in sync with
    // LanguageSettingsActionType. Further actions can only be appended, existing
    // entries must not be overwritten.
    @IntDef({LanguageSettingsActionType.LANGUAGE_ADDED, LanguageSettingsActionType.LANGUAGE_REMOVED,
            LanguageSettingsActionType.DISABLE_TRANSLATE_GLOBALLY,
            LanguageSettingsActionType.ENABLE_TRANSLATE_GLOBALLY,
            LanguageSettingsActionType.DISABLE_TRANSLATE_FOR_SINGLE_LANGUAGE,
            LanguageSettingsActionType.ENABLE_TRANSLATE_FOR_SINGLE_LANGUAGE,
            LanguageSettingsActionType.LANGUAGE_LIST_REORDERED,
            LanguageSettingsActionType.CHANGE_CHROME_LANGUAGE,
            LanguageSettingsActionType.CHANGE_TARGET_LANGUAGE,
            LanguageSettingsActionType.REMOVE_FROM_NEVER_TRANSLATE,
            LanguageSettingsActionType.ADD_TO_NEVER_TRANSLATE,
            LanguageSettingsActionType.REMOVE_FROM_ALWAYS_TRANSLATE,
            LanguageSettingsActionType.ADD_TO_ALWAYS_TRANSLATE,
            LanguageSettingsActionType.REMOVE_SITE_FROM_NEVER_TRANSLATE})
    @Retention(RetentionPolicy.SOURCE)
    @interface LanguageSettingsActionType {
        // int CLICK_ON_ADD_LANGUAGE = 1; // Removed M89
        int LANGUAGE_ADDED = 2;
        int LANGUAGE_REMOVED = 3;
        int DISABLE_TRANSLATE_GLOBALLY = 4;
        int ENABLE_TRANSLATE_GLOBALLY = 5;
        int DISABLE_TRANSLATE_FOR_SINGLE_LANGUAGE = 6;
        int ENABLE_TRANSLATE_FOR_SINGLE_LANGUAGE = 7;
        int LANGUAGE_LIST_REORDERED = 8;
        int CHANGE_CHROME_LANGUAGE = 9;
        int CHANGE_TARGET_LANGUAGE = 10;
        int REMOVE_FROM_NEVER_TRANSLATE = 11;
        int ADD_TO_NEVER_TRANSLATE = 12;
        int REMOVE_FROM_ALWAYS_TRANSLATE = 13;
        int ADD_TO_ALWAYS_TRANSLATE = 14;
        int REMOVE_SITE_FROM_NEVER_TRANSLATE = 15;
        int NUM_ENTRIES = 16;
    }

    // Constants used to log UMA enum histogram, must stay in sync with
    // LanguageSettingsPageType. Further actions can only be appended, existing
    // entries must not be overwritten.
    @IntDef({LanguageSettingsPageType.PAGE_MAIN,
            LanguageSettingsPageType.CONTENT_LANGUAGE_ADD_LANGUAGE,
            LanguageSettingsPageType.CHANGE_CHROME_LANGUAGE,
            LanguageSettingsPageType.ADVANCED_LANGUAGE_SETTINGS,
            LanguageSettingsPageType.CHANGE_TARGET_LANGUAGE,
            LanguageSettingsPageType.LANGUAGE_OVERFLOW_MENU_OPENED,
            LanguageSettingsPageType.VIEW_NEVER_TRANSLATE_LANGUAGES,
            LanguageSettingsPageType.NEVER_TRANSLATE_ADD_LANGUAGE,
            LanguageSettingsPageType.VIEW_ALWAYS_TRANSLATE_LANGUAGES,
            LanguageSettingsPageType.ALWAYS_TRANSLATE_ADD_LANGUAGE,
            LanguageSettingsPageType.VIEW_NEVER_TRANSLATE_SITES})
    @Retention(RetentionPolicy.SOURCE)
    @interface LanguageSettingsPageType {
        int PAGE_MAIN = 0;
        int CONTENT_LANGUAGE_ADD_LANGUAGE = 1;
        // int LANGUAGE_DETAILS = 2; // iOS only
        int CHANGE_CHROME_LANGUAGE = 3;
        int ADVANCED_LANGUAGE_SETTINGS = 4;
        int CHANGE_TARGET_LANGUAGE = 5;
        int LANGUAGE_OVERFLOW_MENU_OPENED = 6;
        int VIEW_NEVER_TRANSLATE_LANGUAGES = 7;
        int NEVER_TRANSLATE_ADD_LANGUAGE = 8;
        int VIEW_ALWAYS_TRANSLATE_LANGUAGES = 9;
        int ALWAYS_TRANSLATE_ADD_LANGUAGE = 10;
        int VIEW_NEVER_TRANSLATE_SITES = 11;
        int NUM_ENTRIES = 12;
    }

    private static LanguagesManager sManager;

    private final Map<String, LanguageItem> mLanguagesMap;

    private AcceptLanguageObserver mObserver;

    private LanguagesManager() {
        // Get all language data from native.
        mLanguagesMap = new LinkedHashMap<>();
        for (LanguageItem item : TranslateBridge.getChromeLanguageList()) {
            mLanguagesMap.put(item.getCode(), item);
        }
    }

    private void notifyAcceptLanguageObserver() {
        if (mObserver != null) mObserver.onDataUpdated();
    }

    /**
     * Sets the observer tracking the user accept languages changes.
     */
    public void setAcceptLanguageObserver(AcceptLanguageObserver observer) {
        mObserver = observer;
    }

    /**
     * @return A list of LanguageItems for the current user's accept languages.
     */
    public List<LanguageItem> getUserAcceptLanguageItems() {
        // Always read the latest user accept language code list from native.
        List<String> codes = TranslateBridge.getUserLanguageCodes();

        List<LanguageItem> results = new ArrayList<>();
        // Keep the same order as accept language codes list.
        for (String code : codes) {
            // Check language code and only languages supported on Android are added in.
            if (mLanguagesMap.containsKey(code)) results.add(mLanguagesMap.get(code));
        }
        return results;
    }

    /**
     * @return A list of LanguageItems, excluding the current user's accept languages.
     */
    public List<LanguageItem> getLanguageItemsExcludingUserAccept() {
        // Always read the latest user accept language code list from native.
        List<String> codes = TranslateBridge.getUserLanguageCodes();

        List<LanguageItem> results = new ArrayList<>();
        // Keep the same order as mLanguagesMap.
        for (LanguageItem item : mLanguagesMap.values()) {
            if (!codes.contains(item.getCode())) results.add(item);
        }
        return results;
    }

    /**
     * Get a list of LanguageItems that can be Chrome UI languages.
     * @return List of LanguageItems.
     */
    public List<LanguageItem> getAvailableUiLanguageItems() {
        List<LanguageItem> results = new ArrayList<>();
        for (LanguageItem item : mLanguagesMap.values()) {
            if (item.isUISupported()) results.add(item);
        }
        return results;
    }

    /**
     * Get a list of all LanguageItems that are supported by translate.
     * @return List of LanguageItems.
     */
    public List<LanguageItem> getTranslateLanguageItems() {
        List<LanguageItem> results = new ArrayList<>();
        for (LanguageItem item : mLanguagesMap.values()) {
            if (item.isSupportedBaseLanguage()) results.add(item);
        }
        return results;
    }

    /**
     * Get a list of LanguageItems that the user has set to always translate. The list is sorted
     * alphabetically by display name.
     * @return List of LanguageItems.
     */
    public Collection<LanguageItem> getAlwaysTranslateLanguageItems() {
        // Get the latest always translate list from native. This list has no guaranteed order.
        List<String> codes = TranslateBridge.getAlwaysTranslateLanguages();
        TreeSet<LanguageItem> results = new TreeSet(LanguageItem.COMPARE_BY_DISPLAY_NAME);
        for (String code : codes) {
            if (mLanguagesMap.containsKey(code)) results.add(mLanguagesMap.get(code));
        }
        return results;
    }

    /**
     * Get a list of LanguageItems that the user has set to never prompt for translation. The list
     * is sorted alphabetically by display name.
     * @return List of LanguageItems.
     */
    public Collection<LanguageItem> getNeverTranslateLanguageItems() {
        // Get the latest never translate list from native. This list has no guaranteed order.
        List<String> codes = TranslateBridge.getNeverTranslateLanguages();
        TreeSet<LanguageItem> results = new TreeSet(LanguageItem.COMPARE_BY_DISPLAY_NAME);
        for (String code : codes) {
            if (mLanguagesMap.containsKey(code)) results.add(mLanguagesMap.get(code));
        }
        return results;
    }

    /**
     * Get a LanguageItem given the iso639 locale code (e.g. en-US).  If no direct match is found
     * only the language is checked. If there is still no match null is returned.
     * @return LanguageItem or null if none found
     */
    public LanguageItem getLanguageItem(String localeCode) {
        LanguageItem result = mLanguagesMap.get(localeCode);
        if (result != null) return result;

        String baseLanguage = LocaleUtils.toLanguage(localeCode);
        return mLanguagesMap.get(baseLanguage);
    }

    /**
     * Add a language to the current user's accept languages.
     * @param code The language code to remove.
     */
    public void addToAcceptLanguages(String code) {
        TranslateBridge.updateUserAcceptLanguages(code, true /* is_add */);
        notifyAcceptLanguageObserver();
    }

    /**
     * Remove a language from the current user's accept languages.
     * @param code The language code to remove.
     */
    public void removeFromAcceptLanguages(String code) {
        TranslateBridge.updateUserAcceptLanguages(code, false /* is_add */);
        notifyAcceptLanguageObserver();
    }

    /**
     * Move a language's position in the user's accept languages list.
     * @param code The language code to move.
     * @param offset The offset from the original position.
     *               Negative value means up and positive value means down.
     * @param reload  Whether to reload the language list.
     */
    public void moveLanguagePosition(String code, int offset, boolean reload) {
        if (offset == 0) return;

        TranslateBridge.moveAcceptLanguage(code, offset);
        recordAction(LanguageSettingsActionType.LANGUAGE_LIST_REORDERED);
        if (reload) notifyAcceptLanguageObserver();
    }

    /**
     * Sets the preference order of the user's accepted languages to the provided order.
     *
     * @param codes The new order for the user's languages.
     * @param reload True iff the language list should be reloaded.
     */
    public void setOrder(String[] codes, boolean reload) {
        TranslateBridge.setLanguageOrder(codes);
        recordAction(LanguageSettingsActionType.LANGUAGE_LIST_REORDERED);
        if (reload) notifyAcceptLanguageObserver();
    }

    /**
     * Called to get all languages available in chrome.
     * @return A map of language code to {@code LanguageItem} for all available languages.
     */
    public Map<String, LanguageItem> getLanguageMap() {
        return mLanguagesMap;
    }

    /**
     * Get the static instance of ChromePreferenceManager if it exists else create it.
     * @return the LanguagesManager singleton.
     */
    public static LanguagesManager getInstance() {
        if (sManager == null) sManager = new LanguagesManager();
        return sManager;
    }

    /**
     * Called to release unused resources.
     */
    public static void recycle() {
        sManager = null;
    }

    /**
     * Record language settings page impression.
     */
    public static void recordImpression(@LanguageSettingsPageType int pageType) {
        RecordHistogram.recordEnumeratedHistogram(
                "LanguageSettings.PageImpression", pageType, LanguageSettingsPageType.NUM_ENTRIES);
    }

    /**
     * Record actions taken on language settings page.
     */
    public static void recordAction(@LanguageSettingsActionType int actionType) {
        RecordHistogram.recordEnumeratedHistogram(
                "LanguageSettings.Actions", actionType, LanguageSettingsActionType.NUM_ENTRIES);
    }
}

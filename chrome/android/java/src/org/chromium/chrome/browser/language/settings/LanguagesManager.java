// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.language.settings;

import androidx.annotation.IntDef;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.translate.TranslateBridge;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;

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
    @IntDef({LanguageSettingsActionType.CLICK_ON_ADD_LANGUAGE,
            LanguageSettingsActionType.LANGUAGE_ADDED, LanguageSettingsActionType.LANGUAGE_REMOVED,
            LanguageSettingsActionType.DISABLE_TRANSLATE_GLOBALLY,
            LanguageSettingsActionType.ENABLE_TRANSLATE_GLOBALLY,
            LanguageSettingsActionType.DISABLE_TRANSLATE_FOR_SINGLE_LANGUAGE,
            LanguageSettingsActionType.ENABLE_TRANSLATE_FOR_SINGLE_LANGUAGE,
            LanguageSettingsActionType.LANGUAGE_LIST_REORDERED})
    @Retention(RetentionPolicy.SOURCE)
    @interface LanguageSettingsActionType {
        int CLICK_ON_ADD_LANGUAGE = 1;
        int LANGUAGE_ADDED = 2;
        int LANGUAGE_REMOVED = 3;
        int DISABLE_TRANSLATE_GLOBALLY = 4;
        int ENABLE_TRANSLATE_GLOBALLY = 5;
        int DISABLE_TRANSLATE_FOR_SINGLE_LANGUAGE = 6;
        int ENABLE_TRANSLATE_FOR_SINGLE_LANGUAGE = 7;
        int LANGUAGE_LIST_REORDERED = 8;
        int NUM_ENTRIES = 9;
    }

    // Constants used to log UMA enum histogram, must stay in sync with
    // LanguageSettingsPageType. Further actions can only be appended, existing
    // entries must not be overwritten.
    @IntDef({LanguageSettingsPageType.PAGE_MAIN, LanguageSettingsPageType.PAGE_ADD_LANGUAGE})
    @Retention(RetentionPolicy.SOURCE)
    @interface LanguageSettingsPageType {
        int PAGE_MAIN = 0;
        int PAGE_ADD_LANGUAGE = 1;
        int NUM_ENTRIES = 2;
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

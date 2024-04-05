// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.language.settings;

import androidx.annotation.IntDef;
import androidx.core.util.Predicate;

import org.chromium.base.LocaleUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.language.AppLocaleUtils;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileKeyedMap;
import org.chromium.chrome.browser.translate.TranslateBridge;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collection;
import java.util.HashSet;
import java.util.LinkedHashMap;
import java.util.LinkedHashSet;
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
        /** Called when the accept languages for the current user are updated. */
        void onDataUpdated();
    }

    // Constants used to log UMA enum histogram, must stay in sync with
    // LanguageSettingsActionType. Further actions can only be appended, existing
    // entries must not be overwritten.
    @IntDef({
        LanguageSettingsActionType.LANGUAGE_ADDED,
        LanguageSettingsActionType.LANGUAGE_REMOVED,
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
        LanguageSettingsActionType.REMOVE_SITE_FROM_NEVER_TRANSLATE,
        LanguageSettingsActionType.RESTART_CHROME
    })
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
        int RESTART_CHROME = 16;
        int NUM_ENTRIES = 17;
    }

    // Constants used to log UMA enum histogram, must stay in sync with
    // LanguageSettingsPageType. Further actions can only be appended, existing
    // entries must not be overwritten.
    @IntDef({
        LanguageSettingsPageType.PAGE_MAIN,
        LanguageSettingsPageType.CONTENT_LANGUAGE_ADD_LANGUAGE,
        LanguageSettingsPageType.CHANGE_CHROME_LANGUAGE,
        LanguageSettingsPageType.ADVANCED_LANGUAGE_SETTINGS,
        LanguageSettingsPageType.CHANGE_TARGET_LANGUAGE,
        LanguageSettingsPageType.LANGUAGE_OVERFLOW_MENU_OPENED,
        LanguageSettingsPageType.VIEW_NEVER_TRANSLATE_LANGUAGES,
        LanguageSettingsPageType.NEVER_TRANSLATE_ADD_LANGUAGE,
        LanguageSettingsPageType.VIEW_ALWAYS_TRANSLATE_LANGUAGES,
        LanguageSettingsPageType.ALWAYS_TRANSLATE_ADD_LANGUAGE,
        LanguageSettingsPageType.VIEW_NEVER_TRANSLATE_SITES
    })
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

    // Int keys to determine the list of potential languages for different language preferences.
    @IntDef({
        LanguageListType.ACCEPT_LANGUAGES,
        LanguageListType.UI_LANGUAGES,
        LanguageListType.TARGET_LANGUAGES,
        LanguageListType.NEVER_LANGUAGES,
        LanguageListType.ALWAYS_LANGUAGES
    })
    @Retention(RetentionPolicy.SOURCE)
    @interface LanguageListType {
        int ACCEPT_LANGUAGES = 0; // Default
        int UI_LANGUAGES = 1;
        int TARGET_LANGUAGES = 2;
        int NEVER_LANGUAGES = 3;
        int ALWAYS_LANGUAGES = 4;
    }

    private static ProfileKeyedMap<LanguagesManager> sProfileMap;

    private final Profile mProfile;
    private final Map<String, LanguageItem> mLanguagesMap;

    private AcceptLanguageObserver mObserver;

    private LanguagesManager(Profile profile) {
        mProfile = profile;

        // Get all language data from native.
        mLanguagesMap = new LinkedHashMap<>();
        for (LanguageItem item : TranslateBridge.getChromeLanguageList(mProfile)) {
            mLanguagesMap.put(item.getCode(), item);
        }
    }

    private void notifyAcceptLanguageObserver() {
        if (mObserver != null) mObserver.onDataUpdated();
    }

    /** Sets the observer tracking the user accept languages changes. */
    public void setAcceptLanguageObserver(AcceptLanguageObserver observer) {
        mObserver = observer;
    }

    /**
     * @return A list of LanguageItems for the current user's accept languages.
     */
    public List<LanguageItem> getUserAcceptLanguageItems() {
        // Always read the latest user accept language code list from native.
        List<String> codes = TranslateBridge.getUserLanguageCodes(mProfile);

        List<LanguageItem> results = new ArrayList<>();
        // Keep the same order as accept language codes list.
        for (String code : codes) {
            // Check language code and only languages supported on Android are added in.
            if (mLanguagesMap.containsKey(code)) results.add(mLanguagesMap.get(code));
        }
        return results;
    }

    /**
     * Get the list of potential languages to show in the {@link SelectLanguageFragment} based on
     * which list or preference a language will be added to. By default the potential languages for
     * the Accept-Language list is returned.
     *
     * @param LanguageListType key to select which languages to get.
     * @return A list of LanguageItems to choose from for the given preference.
     */
    public List<LanguageItem> getPotentialLanguages(@LanguageListType int potentialLanguages) {
        switch (potentialLanguages) {
            case LanguageListType.ALWAYS_LANGUAGES:
                return getPotentialTranslateLanguages(
                        TranslateBridge.getAlwaysTranslateLanguages(mProfile));
            case LanguageListType.NEVER_LANGUAGES:
                return getPotentialTranslateLanguages(
                        TranslateBridge.getNeverTranslateLanguages(mProfile));
            case LanguageListType.TARGET_LANGUAGES:
                return getPotentialTranslateLanguages(
                        Arrays.asList(TranslateBridge.getTargetLanguageForChromium(mProfile)));
            case LanguageListType.UI_LANGUAGES:
                return getPotentialUiLanguages();
            case LanguageListType.ACCEPT_LANGUAGES:
                return getPotentialAcceptLanguages();
            default:
                assert false : "No valid LanguageListType";
                return null;
        }
    }

    /**
     * Get a list of LanguageItems that can be used as a Translate language but excluding
     * |codesToSkip|. The current Accept-Languages are added to the front of the list.
     * @param codesToSkip Collection of String language codes to exclude from the list.
     * @return List of LanguageItems.
     */
    private List<LanguageItem> getPotentialTranslateLanguages(Collection<String> codesToSkip) {
        HashSet<String> codesToSkipSet = new HashSet<String>(codesToSkip);
        LinkedHashSet<LanguageItem> results = new LinkedHashSet<>();
        // Filter for translatable languages not in |codesToSkipSet|.
        Predicate<LanguageItem> filter =
                (item) -> {
                    return item.isSupportedBaseTranslateLanguage()
                            && !codesToSkipSet.contains(item.getCode());
                };
        addItemsToResult(results, getUserAcceptLanguageItems(), filter);
        addItemsToResult(results, mLanguagesMap.values(), filter);
        return new ArrayList<>(results);
    }

    /**
     * Get a list of potential LanguageItems Chrome UI languages excluding the current UI language.
     * The current Accept-Languages are added to the front of the the list.
     * @return List of LanguageItems.
     */
    private List<LanguageItem> getPotentialUiLanguages() {
        LinkedHashSet<LanguageItem> results = new LinkedHashSet<>();
        LanguageItem currentUiLanguage = getLanguageItem(AppLocaleUtils.getAppLanguagePref());

        // Add the system default language if an override language is set.
        if (!currentUiLanguage.isSystemDefault()) {
            results.add(LanguageItem.makeFollowSystemLanguageItem());
        }

        // Filter for UI languages that are not the current UI language.
        Predicate<LanguageItem> filter =
                (item) -> {
                    return item.isUISupported() && !item.equals(currentUiLanguage);
                };
        addItemsToResult(results, getUserAcceptLanguageItems(), filter);
        addItemsToResult(results, mLanguagesMap.values(), filter);
        return new ArrayList<>(results);
    }

    /**
     * Get a list of all possible UI Languages ins alphabetical order.
     * @return List of LanguageItems
     */
    public List<LanguageItem> getAllPossibleUiLanguages() {
        LinkedHashSet<LanguageItem> results = new LinkedHashSet<>();
        Predicate<LanguageItem> filter =
                (item) -> {
                    return item.isUISupported();
                };
        addItemsToResult(results, mLanguagesMap.values(), filter);
        return new ArrayList<>(results);
    }

    /**
     * Get a list of potential Accept-Languages excluding the current Accept-Languages.
     *
     * @return A list of LanguageItems, excluding the current user's accept languages.
     */
    private List<LanguageItem> getPotentialAcceptLanguages() {
        // Always read the latest user accept language code list from native.
        HashSet<String> codesToSkip = new HashSet(TranslateBridge.getUserLanguageCodes(mProfile));
        LinkedHashSet<LanguageItem> results = new LinkedHashSet<>();
        addItemsToResult(
                results, mLanguagesMap.values(), (item) -> !codesToSkip.contains(item.getCode()));
        return new ArrayList<>(results);
    }

    /**
     * Add LanguageItems in |items| to |results| keeping their order and excluding items that match
     * |filter|.
     * @param results LinkedHashSet of LanguageItems to add items to.
     * @param items Collection of LanguageItems to potentially add to results.
     * @param filter Predicate to return true for items that should be added to results.
     */
    private void addItemsToResult(
            LinkedHashSet<LanguageItem> results,
            Collection<LanguageItem> items,
            Predicate<LanguageItem> filter) {
        for (LanguageItem item : items) {
            if (filter.test(item)) {
                results.add(item);
            }
        }
    }

    /**
     * Get a list of LanguageItems that the user has set to always translate. The list is sorted
     * alphabetically by display name.
     *
     * @return List of LanguageItems.
     */
    public Collection<LanguageItem> getAlwaysTranslateLanguageItems() {
        // Get the latest always translate list from native. This list has no guaranteed order.
        List<String> codes = TranslateBridge.getAlwaysTranslateLanguages(mProfile);
        TreeSet<LanguageItem> results = new TreeSet(LanguageItem.COMPARE_BY_DISPLAY_NAME);
        for (String code : codes) {
            if (mLanguagesMap.containsKey(code)) results.add(mLanguagesMap.get(code));
        }
        return results;
    }

    /**
     * Get a list of LanguageItems that the user has set to never prompt for translation. The list
     * is sorted alphabetically by display name.
     *
     * @return List of LanguageItems.
     */
    public Collection<LanguageItem> getNeverTranslateLanguageItems() {
        // Get the latest never translate list from native. This list has no guaranteed order.
        List<String> codes = TranslateBridge.getNeverTranslateLanguages(mProfile);
        TreeSet<LanguageItem> results = new TreeSet(LanguageItem.COMPARE_BY_DISPLAY_NAME);
        for (String code : codes) {
            if (mLanguagesMap.containsKey(code)) results.add(mLanguagesMap.get(code));
        }
        return results;
    }

    /**
     * Get a LanguageItem given the iso639 locale code (e.g. en-US). If no match is found the base
     * language is checked (e.g. "en" for "en-AU"). If there is still no match null is returned.
     * @return LanguageItem or null if none found
     */
    public LanguageItem getLanguageItem(String localeCode) {
        if (AppLocaleUtils.isFollowSystemLanguage(localeCode)) {
            return LanguageItem.makeFollowSystemLanguageItem();
        }
        LanguageItem result = mLanguagesMap.get(localeCode);
        if (result != null) return result;

        String baseLanguage = LocaleUtils.toBaseLanguage(localeCode);
        return mLanguagesMap.get(baseLanguage);
    }

    /**
     * Add a language to the current user's accept languages.
     *
     * @param code The language code to remove.
     */
    public void addToAcceptLanguages(String code) {
        TranslateBridge.updateUserAcceptLanguages(mProfile, code, /* add= */ true);
        notifyAcceptLanguageObserver();
    }

    /**
     * Remove a language from the current user's accept languages.
     *
     * @param code The language code to remove.
     */
    public void removeFromAcceptLanguages(String code) {
        TranslateBridge.updateUserAcceptLanguages(mProfile, code, /* add= */ false);
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

        TranslateBridge.moveAcceptLanguage(mProfile, code, offset);
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
        TranslateBridge.setLanguageOrder(mProfile, codes);
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

    /** Return the {@link LanguagesManager} associated with the current {@link Profile}. */
    public static LanguagesManager getForProfile(Profile profile) {
        if (sProfileMap == null) {
            sProfileMap =
                    new ProfileKeyedMap<>(
                            ProfileKeyedMap.ProfileSelection.REDIRECTED_TO_ORIGINAL,
                            ProfileKeyedMap.NO_REQUIRED_CLEANUP_ACTION);
        }
        return sProfileMap.getForProfile(profile, LanguagesManager::new);
    }

    /** Called to release unused resources. */
    public static void recycle() {
        sProfileMap.destroy();
        sProfileMap = null;
    }

    /** Record language settings page impression. */
    public static void recordImpression(@LanguageSettingsPageType int pageType) {
        RecordHistogram.recordEnumeratedHistogram(
                "LanguageSettings.PageImpression", pageType, LanguageSettingsPageType.NUM_ENTRIES);
    }

    /** Record actions taken on language settings page. */
    public static void recordAction(@LanguageSettingsActionType int actionType) {
        RecordHistogram.recordEnumeratedHistogram(
                "LanguageSettings.Actions", actionType, LanguageSettingsActionType.NUM_ENTRIES);
    }
}

// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.translate;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.language.settings.LanguageItem;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.content_public.browser.WebContents;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.LinkedHashSet;
import java.util.List;

/**
 * Bridge class that lets Android code access native code to execute translate on a tab.
 */
public class TranslateBridge {
    /**
     * Translates the given tab when the necessary state has been computed (e.g. source language).
     */
    public static void translateTabWhenReady(Tab tab) {
        TranslateBridgeJni.get().manualTranslateWhenReady(tab.getWebContents());
    }

    /**
     * Initates a translation on the given tab to the given target language.
     */
    public static void translateTabToLanguage(Tab tab, String targetLanguageCode) {
        TranslateBridgeJni.get().translateToLanguage(tab.getWebContents(), targetLanguageCode);
    }

    /**
     * Returns true iff the current tab can be manually translated.
     * Logging should only be performed when this method is called to show the translate menu item.
     */
    public static boolean canManuallyTranslate(Tab tab, boolean menuLogging) {
        return TranslateBridgeJni.get().canManuallyTranslate(tab.getWebContents(), menuLogging);
    }

    /**
     * Returns true iff we're in a state where the manual translate IPH could be shown.
     */
    public static boolean shouldShowManualTranslateIPH(Tab tab) {
        return TranslateBridgeJni.get().shouldShowManualTranslateIPH(tab.getWebContents());
    }

    /**
     * Sets the language that the contents of the tab needs to be translated to.
     * No-op in case target language is invalid or not supported.
     *
     * @param targetLanguage language code in ISO 639 format.
     */
    public static void setPredefinedTargetLanguage(Tab tab, String targetLanguage) {
        TranslateBridgeJni.get().setPredefinedTargetLanguage(tab.getWebContents(), targetLanguage);
    }

    /**
     * @return The original language code of the given tab. Empty string if no language was detected
     *         yet.
     */
    public static String getOriginalLanguage(Tab tab) {
        return TranslateBridgeJni.get().getOriginalLanguage(tab.getWebContents());
    }

    /**
     * @return The current language code of the given tab. Empty string if no language was detected
     *         yet.
     */
    public static String getCurrentLanguage(Tab tab) {
        return TranslateBridgeJni.get().getCurrentLanguage(tab.getWebContents());
    }

    /**
     * @return The best target language based on what the Translate Service knows about the user.
     */
    public static String getTargetLanguage() {
        return TranslateBridgeJni.get().getTargetLanguage();
    }

    /** @return whether the given string is blocked for translation. */
    public static boolean isBlockedLanguage(String language) {
        return TranslateBridgeJni.get().isBlockedLanguage(language);
    }

    /**
     * @return The ordered set of all languages that the user's knows, ordered by how well they know
     *         them with the most familiar listed first.
     */
    public static LinkedHashSet<String> getModelLanguages() {
        LinkedHashSet<String> set = new LinkedHashSet<String>();
        // Calls back through addModelLanguageToSet repeatedly.
        TranslateBridgeJni.get().getModelLanguages(set);
        return set;
    }

    /**
     * Called by {@link #TranslateBridgeJni.get().getModelLanguages} with the set to add to and the
     * language to add.
     */
    @CalledByNative
    private static void addModelLanguageToSet(
            LinkedHashSet<String> languages, String languageCode) {
        languages.add(languageCode);
    }

    @CalledByNative
    private static void copyStringArrayToList(List<String> list, String[] source) {
        list.addAll(Arrays.asList(source));
    }

    @CalledByNative
    private static void addNewLanguageItemToList(List<LanguageItem> list, String code,
            String displayName, String nativeDisplayName, boolean supportTranslate) {
        list.add(new LanguageItem(code, displayName, nativeDisplayName, supportTranslate));
    }

    /**
     * Reset accept-languages to its default value.
     *
     * @param defaultLocale A fall-back value such as en_US, de_DE, zh_CN, etc.
     */
    public static void resetAcceptLanguages(String defaultLocale) {
        TranslateBridgeJni.get().resetAcceptLanguages(defaultLocale);
    }

    /**
     * @return A sorted list of LanguageItems representing the Chrome accept languages with details.
     *         Languages that are not supported on Android have been filtered out.
     */
    public static List<LanguageItem> getChromeLanguageList() {
        List<LanguageItem> list = new ArrayList<>();
        TranslateBridgeJni.get().getChromeAcceptLanguages(list);
        return list;
    }

    /**
     * @return A sorted list of accept language codes for the current user.
     *         Note that for the signed-in user, the list might contain some language codes from
     *         other platforms but not supported on Android.
     */
    public static List<String> getUserLanguageCodes() {
        List<String> list = new ArrayList<>();
        TranslateBridgeJni.get().getUserAcceptLanguages(list);
        return list;
    }

    /**
     * Update accept language for the current user.
     *
     * @param languageCode A valid language code to update.
     * @param add Whether this is an "add" operation or "delete" operation.
     */
    public static void updateUserAcceptLanguages(String languageCode, boolean add) {
        TranslateBridgeJni.get().updateUserAcceptLanguages(languageCode, add);
    }

    /**
     * Move a language to the given position of the user's accept language.
     *
     * @param languageCode A valid language code to set.
     * @param offset The offset from the original position of the language.
     */
    public static void moveAcceptLanguage(String languageCode, int offset) {
        TranslateBridgeJni.get().moveAcceptLanguage(languageCode, offset);
    }

    /**
     * Given an array of language codes, sets the order of the user's accepted languages to match.
     *
     * @param codes The new order for the user's accepted languages.
     */
    public static void setLanguageOrder(String[] codes) {
        TranslateBridgeJni.get().setLanguageOrder(codes);
    }

    /**
     * @param languageCode A valid language code to check.
     * @return Whether the given language is blocked by the user.
     */
    public static boolean isBlockedLanguage2(String languageCode) {
        return TranslateBridgeJni.get().isBlockedLanguage2(languageCode);
    }

    /**
     * Sets the blocked state of a given language.
     *
     * @param languageCode A valid language code to change.
     * @param blocked Whether to set language blocked.
     */
    public static void setLanguageBlockedState(String languageCode, boolean blocked) {
        TranslateBridgeJni.get().setLanguageBlockedState(languageCode, blocked);
    }

    /**
     * @return Whether the explicit language prompt was shown at least once.
     */
    public static boolean getExplicitLanguageAskPromptShown() {
        return TranslateBridgeJni.get().getExplicitLanguageAskPromptShown();
    }

    /**
     * @param shown The value to set the underlying pref to: whether the prompt
     * was shown to the user at least once.
     */
    public static void setExplicitLanguageAskPromptShown(boolean shown) {
        TranslateBridgeJni.get().setExplicitLanguageAskPromptShown(shown);
    }

    public static void setIgnoreMissingKeyForTesting(boolean ignore) {
        TranslateBridgeJni.get().setIgnoreMissingKeyForTesting(ignore); // IN-TEST
    }

    @NativeMethods
    interface Natives {
        void manualTranslateWhenReady(WebContents webContents);
        void translateToLanguage(WebContents webContents, String targetLanguageCode);
        boolean canManuallyTranslate(WebContents webContents, boolean menuLogging);
        boolean shouldShowManualTranslateIPH(WebContents webContents);
        void setPredefinedTargetLanguage(WebContents webContents, String targetLanguage);
        String getOriginalLanguage(WebContents webContents);
        String getCurrentLanguage(WebContents webContents);
        String getTargetLanguage();
        boolean isBlockedLanguage(String language);
        void getModelLanguages(LinkedHashSet<String> set);
        void resetAcceptLanguages(String defaultLocale);
        void getChromeAcceptLanguages(List<LanguageItem> list);
        void getUserAcceptLanguages(List<String> list);
        void updateUserAcceptLanguages(String language, boolean add);
        void moveAcceptLanguage(String language, int offset);
        void setLanguageOrder(String[] codes);
        boolean isBlockedLanguage2(String language);
        void setLanguageBlockedState(String language, boolean blocked);
        boolean getExplicitLanguageAskPromptShown();
        void setExplicitLanguageAskPromptShown(boolean shown);
        void setIgnoreMissingKeyForTesting(boolean ignore);
    }
}

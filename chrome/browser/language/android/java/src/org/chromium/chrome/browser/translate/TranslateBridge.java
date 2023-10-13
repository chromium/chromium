// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.translate;

import org.jni_zero.CalledByNative;
import org.jni_zero.NativeMethods;

import org.chromium.base.LocaleUtils;
import org.chromium.chrome.browser.language.settings.LanguageItem;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.content_public.browser.WebContents;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/**
 * Bridge class that lets Android code access native code to execute translate on a tab.
 */
// TODO(crbug.com/1410601): Pass in the profile and remove GetActiveUserProfile in C++.
public class TranslateBridge {
    /**
     * Translates the given tab when the necessary state has been computed (e.g. source language).
     */
    public static void translateTabWhenReady(Tab tab) {
        TranslateBridgeJni.get().manualTranslateWhenReady(tab.getWebContents());
    }

    /**
     * Returns true iff the current tab can be manually translated.
     * Logging should only be performed when this method is called to show the translate menu item.
     */
    public static boolean canManuallyTranslate(Tab tab, boolean menuLogging) {
        return canManuallyTranslate(tab.getWebContents(), menuLogging);
    }

    /**
     * Returns true iff the contents can be manually translated.
     * Logging should only be performed when this method is called to show the translate menu item.
     */
    public static boolean canManuallyTranslate(WebContents webContents, boolean menuLogging) {
        return TranslateBridgeJni.get().canManuallyTranslate(webContents, menuLogging);
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
     * @param targetLanguage language code in ISO 639 format.
     * @param shouldAutoTranslate If true, the page should be automatically translated immediately
     *                            to targetLanguage.
     */
    public static void setPredefinedTargetLanguage(
            Tab tab, String targetLanguage, boolean shouldAutoTranslate) {
        TranslateBridgeJni.get().setPredefinedTargetLanguage(
                tab.getWebContents(), targetLanguage, shouldAutoTranslate);
    }

    /**
     * @return The best target language based on what the Translate Service knows about the user.
     */
    public static String getTargetLanguage() {
        return TranslateBridgeJni.get().getTargetLanguage();
    }

    /**
     * The target language is stored in Translate format, which uses the old deprecated Java codes
     * for several languages (Hebrew, Indonesian), and uses "tl" while Chromium uses "fil" for
     * Tagalog/Filipino. This converts the target language into the correct Chromium format.
     * @return The Chrome version of the users translate target language.
     */
    public static String getTargetLanguageForChromium() {
        return LocaleUtils.getUpdatedLanguageForChromium(getTargetLanguage());
    }

    /**
     * Set the default target language the Translate Service will use.
     * @param String targetLanguage Language code of new target language.
     */
    public static void setDefaultTargetLanguage(String targetLanguage) {
        TranslateBridgeJni.get().setDefaultTargetLanguage(targetLanguage);
    }

    @CalledByNative
    private static void addNewLanguageItemToList(List<LanguageItem> list, String code,
            String displayName, String nativeDisplayName, boolean supportTranslate) {
        list.add(new LanguageItem(code, displayName, nativeDisplayName, supportTranslate));
    }

    /**
     * Reset accept-languages to its default value.
     * @param defaultLocale A fall-back value such as en_US, de_DE, zh_CN, etc.
     */
    public static void resetAcceptLanguages(String defaultLocale) {
        TranslateBridgeJni.get().resetAcceptLanguages(defaultLocale);
    }

    /**
     * @return A list of LanguageItems sorted by display name that represent all languages that can
     * be on the Chrome accept languages list.
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
        return new ArrayList<>(Arrays.asList(TranslateBridgeJni.get().getUserAcceptLanguages()));
    }

    /** @return List of languages to always translate. */
    public static List<String> getAlwaysTranslateLanguages() {
        return new ArrayList<>(
                Arrays.asList(TranslateBridgeJni.get().getAlwaysTranslateLanguages()));
    }

    /** @return List of languages that translation should not be prompted for. */
    public static List<String> getNeverTranslateLanguages() {
        return new ArrayList<>(
                Arrays.asList(TranslateBridgeJni.get().getNeverTranslateLanguages()));
    }

    public static void setLanguageAlwaysTranslateState(
            String languageCode, boolean alwaysTranslate) {
        TranslateBridgeJni.get().setLanguageAlwaysTranslateState(languageCode, alwaysTranslate);
    }

    /**
     * Update accept language for the current user.
     * @param languageCode A valid language code to update.
     * @param add Whether this is an "add" operation or "delete" operation.
     */
    public static void updateUserAcceptLanguages(String languageCode, boolean add) {
        TranslateBridgeJni.get().updateUserAcceptLanguages(languageCode, add);
    }

    /**
     * Move a language to the given position of the user's accept language.
     * @param languageCode A valid language code to set.
     * @param offset The offset from the original position of the language.
     */
    public static void moveAcceptLanguage(String languageCode, int offset) {
        TranslateBridgeJni.get().moveAcceptLanguage(languageCode, offset);
    }

    /**
     * Given an array of language codes, sets the order of the user's accepted languages to match.
     * @param codes The new order for the user's accepted languages.
     */
    public static void setLanguageOrder(String[] codes) {
        TranslateBridgeJni.get().setLanguageOrder(codes);
    }

    /**
     * @param language The language code to check.
     * @return boolean Whether the given string is blocked for translation.
     */
    public static boolean isBlockedLanguage(String language) {
        return TranslateBridgeJni.get().isBlockedLanguage(language);
    }

    /**
     * Sets the blocked state of a given language.
     * @param languageCode A valid language code to change.
     * @param blocked Whether to set language blocked.
     */
    public static void setLanguageBlockedState(String languageCode, boolean blocked) {
        TranslateBridgeJni.get().setLanguageBlockedState(languageCode, blocked);
    }

    /**
     * @return Whether the app language prompt has been shown or not.
     */
    public static boolean getAppLanguagePromptShown() {
        return TranslateBridgeJni.get().getAppLanguagePromptShown();
    }

    /**
     * Set the pref indicating the app language prompt has been shown to the user.
     */
    public static void setAppLanguagePromptShown() {
        TranslateBridgeJni.get().setAppLanguagePromptShown();
    }

    public static void setIgnoreMissingKeyForTesting(boolean ignore) {
        TranslateBridgeJni.get().setIgnoreMissingKeyForTesting(ignore); // IN-TEST
    }

    /**
     * Get current page language.
     *
     * @param tab Tab to get the current language for
     * @return The current language code or empty string if no language detected.
     */
    public static String getCurrentLanguage(Tab tab) {
        return getCurrentLanguage(tab.getWebContents());
    }

    /**
     * Get the current page language.
     *
     * @param webContents Web contents to get the current language for
     * @return The current language code or empty string if no language detected.
     */
    public static String getCurrentLanguage(WebContents webContents) {
        return TranslateBridgeJni.get().getCurrentLanguage(webContents);
    }

    @NativeMethods
    public interface Natives {
        void manualTranslateWhenReady(WebContents webContents);
        boolean canManuallyTranslate(WebContents webContents, boolean menuLogging);
        boolean shouldShowManualTranslateIPH(WebContents webContents);
        void setPredefinedTargetLanguage(
                WebContents webContents, String targetLanguage, boolean shouldAutoTranslate);
        String getTargetLanguage();
        void setDefaultTargetLanguage(String targetLanguage);
        void resetAcceptLanguages(String defaultLocale);
        void getChromeAcceptLanguages(List<LanguageItem> list);
        String[] getUserAcceptLanguages();
        String[] getAlwaysTranslateLanguages();
        String[] getNeverTranslateLanguages();
        void setLanguageAlwaysTranslateState(String language, boolean alwaysTranslate);
        void updateUserAcceptLanguages(String language, boolean add);
        void moveAcceptLanguage(String language, int offset);
        void setLanguageOrder(String[] codes);
        boolean isBlockedLanguage(String language);
        void setLanguageBlockedState(String language, boolean blocked);
        boolean getAppLanguagePromptShown();
        void setAppLanguagePromptShown();
        void setIgnoreMissingKeyForTesting(boolean ignore);

        String getCurrentLanguage(WebContents webContents);
    }
}

// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.translate;

import org.jni_zero.CalledByNative;
import org.jni_zero.NativeMethods;

import org.chromium.base.LocaleUtils;
import org.chromium.chrome.browser.language.settings.LanguageItem;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.content_public.browser.WebContents;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/** Bridge class that lets Android code access native code to execute translate on a tab. */
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

    /** Returns true iff we're in a state where the manual translate IPH could be shown. */
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
        TranslateBridgeJni.get()
                .setPredefinedTargetLanguage(
                        tab.getWebContents(), targetLanguage, shouldAutoTranslate);
    }

    /**
     * @param profile The current {@link Profile} for this session.
     * @return The best target language based on what the Translate Service knows about the user.
     */
    public static String getTargetLanguage(Profile profile) {
        return TranslateBridgeJni.get().getTargetLanguage(profile);
    }

    /**
     * The target language is stored in Translate format, which uses the old deprecated Java codes
     * for several languages (Hebrew, Indonesian), and uses "tl" while Chromium uses "fil" for
     * Tagalog/Filipino. This converts the target language into the correct Chromium format.
     *
     * @param profile The current {@link Profile} for this session.
     * @return The Chrome version of the users translate target language.
     */
    public static String getTargetLanguageForChromium(Profile profile) {
        return LocaleUtils.getUpdatedLanguageForChromium(getTargetLanguage(profile));
    }

    /**
     * Set the default target language the Translate Service will use.
     *
     * @param profile The current {@link Profile} for this session.
     * @param targetLanguage Language code of new target language.
     */
    public static void setDefaultTargetLanguage(Profile profile, String targetLanguage) {
        TranslateBridgeJni.get().setDefaultTargetLanguage(profile, targetLanguage);
    }

    @CalledByNative
    private static void addNewLanguageItemToList(
            List<LanguageItem> list,
            String code,
            String displayName,
            String nativeDisplayName,
            boolean supportTranslate) {
        list.add(new LanguageItem(code, displayName, nativeDisplayName, supportTranslate));
    }

    /**
     * Reset accept-languages to its default value.
     *
     * @param profile The current {@link Profile} for this session.
     * @param defaultLocale A fall-back value such as en_US, de_DE, zh_CN, etc.
     */
    public static void resetAcceptLanguages(Profile profile, String defaultLocale) {
        TranslateBridgeJni.get().resetAcceptLanguages(profile, defaultLocale);
    }

    /**
     * @param profile The current {@link Profile} for this session.
     * @return A list of LanguageItems sorted by display name that represent all languages that can
     *     be on the Chrome accept languages list.
     */
    public static List<LanguageItem> getChromeLanguageList(Profile profile) {
        List<LanguageItem> list = new ArrayList<>();
        TranslateBridgeJni.get().getChromeAcceptLanguages(profile, list);
        return list;
    }

    /**
     * @param profile The current {@link Profile} for this session.
     * @return A sorted list of accept language codes for the current user. Note that for the
     *     signed-in user, the list might contain some language codes from other platforms but not
     *     supported on Android.
     */
    public static List<String> getUserLanguageCodes(Profile profile) {
        return new ArrayList<>(
                Arrays.asList(TranslateBridgeJni.get().getUserAcceptLanguages(profile)));
    }

    /**
     * @param profile The current {@link Profile} for this session.
     * @return List of languages to always translate.
     */
    public static List<String> getAlwaysTranslateLanguages(Profile profile) {
        return new ArrayList<>(
                Arrays.asList(TranslateBridgeJni.get().getAlwaysTranslateLanguages(profile)));
    }

    /**
     * @param profile The current {@link Profile} for this session.
     * @return List of languages that translation should not be prompted for.
     */
    public static List<String> getNeverTranslateLanguages(Profile profile) {
        return new ArrayList<>(
                Arrays.asList(TranslateBridgeJni.get().getNeverTranslateLanguages(profile)));
    }

    /**
     * Specifies whether a language should be automatically translated.
     *
     * @param profile The current {@link Profile} for this session.
     * @param languageCode A valid language code to update.
     * @param alwaysTranslate Whether the specified language should be automatically translated.
     */
    public static void setLanguageAlwaysTranslateState(
            Profile profile, String languageCode, boolean alwaysTranslate) {
        TranslateBridgeJni.get()
                .setLanguageAlwaysTranslateState(profile, languageCode, alwaysTranslate);
    }

    /**
     * Update accept language for the current user.
     *
     * @param profile The current {@link Profile} for this session.
     * @param languageCode A valid language code to update.
     * @param add Whether this is an "add" operation or "delete" operation.
     */
    public static void updateUserAcceptLanguages(
            Profile profile, String languageCode, boolean add) {
        TranslateBridgeJni.get().updateUserAcceptLanguages(profile, languageCode, add);
    }

    /**
     * Move a language to the given position of the user's accept language.
     *
     * @param profile The current {@link Profile} for this session.
     * @param languageCode A valid language code to set.
     * @param offset The offset from the original position of the language.
     */
    public static void moveAcceptLanguage(Profile profile, String languageCode, int offset) {
        TranslateBridgeJni.get().moveAcceptLanguage(profile, languageCode, offset);
    }

    /**
     * Given an array of language codes, sets the order of the user's accepted languages to match.
     *
     * @param profile The current {@link Profile} for this session.
     * @param codes The new order for the user's accepted languages.
     */
    public static void setLanguageOrder(Profile profile, String[] codes) {
        TranslateBridgeJni.get().setLanguageOrder(profile, codes);
    }

    /**
     * @param profile The current {@link Profile} for this session.
     * @param language The language code to check.
     * @return boolean Whether the given string is blocked for translation.
     */
    public static boolean isBlockedLanguage(Profile profile, String language) {
        return TranslateBridgeJni.get().isBlockedLanguage(profile, language);
    }

    /**
     * Sets the blocked state of a given language.
     *
     * @param profile The current {@link Profile} for this session.
     * @param languageCode A valid language code to change.
     * @param blocked Whether to set language blocked.
     */
    public static void setLanguageBlockedState(
            Profile profile, String languageCode, boolean blocked) {
        TranslateBridgeJni.get().setLanguageBlockedState(profile, languageCode, blocked);
    }

    /**
     * @param profile The current {@link Profile} for this session.
     * @return Whether the app language prompt has been shown or not.
     */
    public static boolean getAppLanguagePromptShown(Profile profile) {
        return TranslateBridgeJni.get().getAppLanguagePromptShown(profile);
    }

    /** Set the pref indicating the app language prompt has been shown to the user. */
    public static void setAppLanguagePromptShown(Profile profile) {
        TranslateBridgeJni.get().setAppLanguagePromptShown(profile);
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

    /**
     * Add an observer for translation events.
     *
     * @param webContents WebContents to observe.
     * @param observer Observer.
     * @return Native observer pointer, needed for removeTranslationObserver().
     */
    public static long addTranslationObserver(
            WebContents webContents, TranslationObserver observer) {
        return TranslateBridgeJni.get().addTranslationObserver(webContents, observer);
    }

    /**
     * Remove a previously added TranslationObserver, destroying the native observer.
     *
     * @param webContents WebContents the observer was registered on.
     * @param observerNativePtr Pointer to the native observer object.
     */
    public static void removeTranslationObserver(WebContents webContents, long observerNativePtr) {
        TranslateBridgeJni.get().removeTranslationObserver(webContents, observerNativePtr);
    }

    /** Whether or not the WebContents have been translated. */
    public static boolean isPageTranslated(WebContents webContents) {
        return TranslateBridgeJni.get().isPageTranslated(webContents);
    }

    @NativeMethods
    public interface Natives {
        long addTranslationObserver(WebContents webContents, TranslationObserver observer);

        void removeTranslationObserver(WebContents webContents, long observerNativePtr);

        void manualTranslateWhenReady(WebContents webContents);

        boolean canManuallyTranslate(WebContents webContents, boolean menuLogging);

        boolean shouldShowManualTranslateIPH(WebContents webContents);

        boolean isPageTranslated(WebContents webContents);

        void setPredefinedTargetLanguage(
                WebContents webContents, String targetLanguage, boolean shouldAutoTranslate);

        String getTargetLanguage(Profile profile);

        void setDefaultTargetLanguage(Profile profile, String targetLanguage);

        void resetAcceptLanguages(Profile profile, String defaultLocale);

        void getChromeAcceptLanguages(Profile profile, List<LanguageItem> list);

        String[] getUserAcceptLanguages(Profile profile);

        String[] getAlwaysTranslateLanguages(Profile profile);

        String[] getNeverTranslateLanguages(Profile profile);

        void setLanguageAlwaysTranslateState(
                Profile profile, String language, boolean alwaysTranslate);

        void updateUserAcceptLanguages(Profile profile, String language, boolean add);

        void moveAcceptLanguage(Profile profile, String language, int offset);

        void setLanguageOrder(Profile profile, String[] codes);

        boolean isBlockedLanguage(Profile profile, String language);

        void setLanguageBlockedState(Profile profile, String language, boolean blocked);

        boolean getAppLanguagePromptShown(Profile profile);

        void setAppLanguagePromptShown(Profile profile);

        void setIgnoreMissingKeyForTesting(boolean ignore);

        String getCurrentLanguage(WebContents webContents);
    }
}

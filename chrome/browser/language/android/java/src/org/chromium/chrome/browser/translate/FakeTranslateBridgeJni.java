// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.translate;

import org.chromium.chrome.browser.language.settings.LanguageItem;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.content_public.browser.WebContents;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collection;
import java.util.HashMap;
import java.util.HashSet;
import java.util.LinkedHashSet;
import java.util.List;
import java.util.Map;
import java.util.TreeMap;

/**
 * Fake implementation of the TranslateBridge native methods that provides support for the language
 * preferences methods. An implementation for methods actually translating WebContents is not.
 *
 * Use FakeTranslateBridgeJni.getSimpleLanguageItemList() to get a fake list of LanguageItems with
 * only 9 languages potentail Accept-Languages that represent a mix of acceptable UI and translate
 * languages.
 */
public class FakeTranslateBridgeJni implements TranslateBridge.Natives {
    private String mTargetLanguage;
    private ArrayList<String> mUserAcceptLanguages;
    private LinkedHashSet<String> mDefaultUserAcceptLanguages;
    private HashSet<String> mNeverLanguages;
    private HashSet<String> mAlwaysLanguages;
    private TreeMap<String, LanguageItem> mChromeLanguages;
    private boolean mAppLanguagePromptShown;
    private String mCurrentLanguage;
    private boolean mIsPageTranslated;
    private final Map<WebContents, Map<Long, TranslationObserver>> mObservers = new HashMap<>();
    private static long sObserverPtr;

    public FakeTranslateBridgeJni(
            Collection<LanguageItem> chromeLanguages,
            Collection<String> userAcceptLanguages,
            Collection<String> neverLanguages,
            Collection<String> alwaysLanguages,
            String targetLanguage) {
        mChromeLanguages = new TreeMap<String, LanguageItem>();
        for (LanguageItem item : chromeLanguages) {
            mChromeLanguages.put(item.getDisplayName(), item);
        }
        mUserAcceptLanguages = new ArrayList(userAcceptLanguages);
        mDefaultUserAcceptLanguages = new LinkedHashSet(userAcceptLanguages);
        mNeverLanguages = new HashSet(neverLanguages);
        mAlwaysLanguages = new HashSet(alwaysLanguages);
        mTargetLanguage = targetLanguage;
    }

    /** Create a basic fake translate bridge with English as the default language. */
    public FakeTranslateBridgeJni() {
        mChromeLanguages = new TreeMap<String, LanguageItem>();
        mUserAcceptLanguages = new ArrayList(Arrays.asList("en"));
        mDefaultUserAcceptLanguages = new LinkedHashSet(Arrays.asList("en"));
        mNeverLanguages = new HashSet(Arrays.asList("en"));
        mAlwaysLanguages = new HashSet();
        mTargetLanguage = "en";
    }

    @Override
    public void getChromeAcceptLanguages(Profile profile, List<LanguageItem> list) {
        list.addAll(mChromeLanguages.values());
    }

    @Override
    public String[] getUserAcceptLanguages(Profile profile) {
        return mUserAcceptLanguages.toArray(new String[mUserAcceptLanguages.size()]);
    }

    /**
     * Reset the user Accept-Languages to the default list and prepend |defaultLocale| if needed.
     */
    @Override
    public void resetAcceptLanguages(Profile profile, String defaultLocale) {
        mUserAcceptLanguages = new ArrayList<>();
        if (!mDefaultUserAcceptLanguages.contains(defaultLocale)) {
            mUserAcceptLanguages.add(defaultLocale);
        }
        mUserAcceptLanguages.addAll(mDefaultUserAcceptLanguages);
    }

    /** Set the Accept-Languages to the new list of strings. */
    @Override
    public void setLanguageOrder(Profile profile, String[] codes) {
        mUserAcceptLanguages = new ArrayList<>(Arrays.asList(codes));
    }

    @Override
    public void updateUserAcceptLanguages(Profile profile, String language, boolean add) {
        if (!add) {
            mUserAcceptLanguages.remove(language);
        } else if (!mUserAcceptLanguages.contains(language)) {
            mUserAcceptLanguages.add(language);
        }
    }

    @Override
    public String getTargetLanguage(Profile profile) {
        return mTargetLanguage;
    }

    @Override
    public void setDefaultTargetLanguage(Profile profile, String targetLanguage) {
        mTargetLanguage = targetLanguage;
    }

    @Override
    public String[] getAlwaysTranslateLanguages(Profile profile) {
        return mAlwaysLanguages.toArray(new String[mAlwaysLanguages.size()]);
    }

    @Override
    public void setLanguageAlwaysTranslateState(
            Profile profile, String language, boolean alwaysTranslate) {
        if (alwaysTranslate) {
            mAlwaysLanguages.add(language);
        } else if (!mAlwaysLanguages.contains(language)) {
            mAlwaysLanguages.remove(language);
        }
    }

    @Override
    public String[] getNeverTranslateLanguages(Profile profile) {
        return mNeverLanguages.toArray(new String[mNeverLanguages.size()]);
    }

    @Override
    public void setLanguageBlockedState(Profile profile, String language, boolean blocked) {
        if (blocked) {
            mNeverLanguages.add(language);
        } else {
            mNeverLanguages.remove(language);
        }
    }

    @Override
    public boolean isBlockedLanguage(Profile profile, String language) {
        return mNeverLanguages.contains(language);
    }

    @Override
    public boolean getAppLanguagePromptShown(Profile profile) {
        return mAppLanguagePromptShown;
    }

    @Override
    public void setAppLanguagePromptShown(Profile profile) {
        mAppLanguagePromptShown = true;
    }

    /**
     * Allow the App Language Prompt shown status to be set for testing.
     * @param shown Boolean value to set App Language Prompt shown status to.
     */
    public void setAppLanguagePromptShown(boolean shown) {
        mAppLanguagePromptShown = shown;
    }

    @Override
    public String getCurrentLanguage(WebContents webContents) {
        return mCurrentLanguage;
    }

    @Override
    public long addTranslationObserver(WebContents webContents, TranslationObserver observer) {
        long ptr = ++sObserverPtr;
        if (!mObservers.containsKey(webContents)) {
            mObservers.put(webContents, new HashMap<>());
        }
        mObservers.get(webContents).put(ptr, observer);
        return ptr;
    }

    @Override
    public void removeTranslationObserver(WebContents webContents, long observerNativePtr) {
        var observersForWebContents = mObservers.get(webContents);
        if (observersForWebContents != null) {
            observersForWebContents.remove(observerNativePtr);
        }
    }

    public int getObserverCount() {
        int count = 0;
        for (var observersForWebContents : mObservers.values()) {
            count += observersForWebContents.size();
        }
        return count;
    }

    public int getObserverCount(WebContents webContents) {
        var observersForWebContents = mObservers.get(webContents);
        return observersForWebContents == null ? 0 : observersForWebContents.size();
    }

    /**
     * Set the web content's current language for testing.
     *
     * @param language String value of what getCurrentLanguage should return.
     */
    public void setCurrentLanguage(String language) {
        mCurrentLanguage = language;
    }

    @Override
    public boolean isPageTranslated(WebContents webContents) {
        return mIsPageTranslated;
    }

    /**
     * Set the web content's current translation state for testing.
     *
     * @param isTranslated whether or not simulate the page as being translated.
     */
    public void setIsPageTranslated(boolean isTranslated) {
        mIsPageTranslated = isTranslated;
    }

    /** Following methods are not implemented yet since they are not needed by current tests. */
    @Override
    public void manualTranslateWhenReady(WebContents webContents) {
        throw new UnsupportedOperationException();
    }

    @Override
    public boolean canManuallyTranslate(WebContents webContents, boolean menuLogging) {
        throw new UnsupportedOperationException();
    }

    @Override
    public boolean shouldShowManualTranslateIPH(WebContents webContents) {
        throw new UnsupportedOperationException();
    }

    @Override
    public void setPredefinedTargetLanguage(
            WebContents webContents, String targetLanguage, boolean shouldAutoTranslate) {
        throw new UnsupportedOperationException();
    }

    @Override
    public void moveAcceptLanguage(Profile profile, String language, int offset) {
        throw new UnsupportedOperationException();
    }

    @Override
    public void setIgnoreMissingKeyForTesting(boolean ignore) {
        throw new UnsupportedOperationException();
    }

    /** Extra utility functions for MockTranslateBridge */
    public int getChromeLanguagesCount() {
        return mChromeLanguages.size();
    }

    public int getAlwaysLanguagesCount() {
        return mAlwaysLanguages.size();
    }

    public int getNeverLanguagesCount() {
        return mNeverLanguages.size();
    }

    /**
     * Create a simple list of nine LanguageItems that are all possible Chrome languages
     * representing combinations of available UI and translateable languages.
     * The order or languages is: en, en-US, en-GB, hi, sw, xh, wa, af, and fil.
     *  - UI Languages: af, fil, en-US, en-GB, hi, sw
     *  - Translateable Languages: af, en, fil, hi, sw, xh
     * @return List of LanguageItems.
     */
    public static List<LanguageItem> getSimpleLanguageItemList() {
        ArrayList<LanguageItem> languages = new ArrayList<>();
        languages.add(new LanguageItem("en", "English", "English", true));
        languages.add(
                new LanguageItem(
                        "en-US", "English (United States)", "English (United States)", true));
        languages.add(
                new LanguageItem(
                        "en-GB", "English (United Kingdom)", "English (United Kingdom)", true));
        languages.add(new LanguageItem("hi", "Hindi", "हिन्दी", true));
        languages.add(new LanguageItem("sw", "Swahili", "Kiswahili", true));
        languages.add(new LanguageItem("xh", "Xhosa", "isiXhosa", true));
        languages.add(new LanguageItem("wa", "Walloon", "Walloon", false));
        languages.add(new LanguageItem("af", "Afrikaans", "Afrikaans", true));
        languages.add(new LanguageItem("fil", "Filipino", "Filipino", true));
        return languages;
    }
}

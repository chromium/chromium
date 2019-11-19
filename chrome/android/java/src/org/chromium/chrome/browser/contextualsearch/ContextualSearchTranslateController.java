// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextualsearch;

import android.content.Context;
import android.text.TextUtils;

import androidx.annotation.Nullable;

import org.chromium.base.ContextUtils;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.ui.UiUtils;

import java.util.ArrayList;
import java.util.LinkedHashSet;
import java.util.List;
import java.util.Locale;

/**
 * Controls how Translation One-box triggering is handled for the {@link ContextualSearchManager}.
 */
public class ContextualSearchTranslateController implements ContextualSearchTranslation {
    private static final int LOCALE_MIN_LENGTH = 2;

    private final ContextualSearchPolicy mPolicy;
    private final ContextualSearchTranslateInterface mHost;

    // Cached native language data for translation;
    private String mTranslateServiceTargetLanguage;
    private String mAcceptLanguages;

    /**
     * Constructs a translation implementation that determines when to trigger translations for
     * Contextual Search requests.
     * @param policy The {@link ContextualSearchPolicy} for determining the target language and
     *        whether translation is disabled.
     * @param hostInterface A {@link ContextualSearchTranslateInterface} back to the host which
     *        provides native implementations of this interface.
     */
    static public ContextualSearchTranslation getContextualSearchTranslation(
            ContextualSearchPolicy policy, ContextualSearchTranslateInterface hostInterface) {
        if (useChromeLanguageModel() && !policy.isTranslationDisabled()) {
            return new ContextualSearchTranslationImpl(policy);
        } else {
            return new ContextualSearchTranslateController(policy, hostInterface);
        }
    }

    /** Do not construct directly, call getContextualSearchTranslation static method. */
    protected ContextualSearchTranslateController(
            ContextualSearchPolicy policy, ContextualSearchTranslateInterface hostInterface) {
        mPolicy = policy;
        mHost = hostInterface;
    }

    @Override
    public void forceTranslateIfNeeded(
            ContextualSearchRequest searchRequest, String sourceLanguage) {
        if (mPolicy.isTranslationDisabled()) return;

        // Force translation if not disabled and server controlled or client logic says required.
        boolean doForceTranslate = needsTranslation(sourceLanguage);
        if (doForceTranslate && searchRequest != null) {
            searchRequest.forceTranslation(
                    sourceLanguage, mPolicy.bestTargetLanguage(getProficientLanguageList()));
        }
        // Log whether or not translate conditions are met
        ContextualSearchUma.logTranslateCondition(doForceTranslate);
    }

    @Override
    public void forceAutoDetectTranslateUnlessDisabled(ContextualSearchRequest searchRequest) {
        // Always trigger translation using auto-detect when we're not resolving,
        // unless disabled by policy.
        if (mPolicy.isTranslationDisabled()) return;

        if (searchRequest != null) {
            // The translation one-box won't actually show when the source text ends up being
            // the same as the target text, so we err on over-triggering.
            searchRequest.forceAutoDetectTranslation(
                    mPolicy.bestTargetLanguage(getProficientLanguageList()));
        }
    }

    @Override
    public boolean needsTranslation(@Nullable String sourceLanguage) {
        return !mPolicy.isTranslationDisabled() && !TextUtils.isEmpty(sourceLanguage)
                && mPolicy.needsTranslation(sourceLanguage, getReadableLanguages());
    }

    @Override
    public String getTranslateServiceTargetLanguage() {
        return getNativeTranslateServiceTargetLanguage();
    }

    /**
     * Gets the list of readable languages for the current user, with the first
     * item in the list being the user's primary language (according to the Translate Service).
     * We assume that the user can read all languages that they can write.
     * @return The {@link List} of languages the user understands with their primary language first.
     */
    private List<String> getReadableLanguages() {
        // Using LinkedHashSet keeps the entries both unique and ordered.
        LinkedHashSet<String> uniqueLanguages = getProficientLanguages();

        // Add the accept languages to the end, since they are a weaker hint than
        // the proficient languages.
        List<String> acceptLanguages = getAcceptLanguages();
        for (String accept : acceptLanguages) {
            if (isValidLocale(accept)) uniqueLanguages.add(trimLocaleToLanguage(accept));
        }
        return new ArrayList<String>(uniqueLanguages);
    }

    /**
     * Gets the list of languages that the current user is proficient using.
     * The list produced is based on the Translation-Service's target language, supplemented
     * with the user's IME keyboard locales.
     * @return An ordered {@link List} of languages the user is proficient using.
     */
    private ArrayList<String> getProficientLanguageList() {
        return new ArrayList<String>(getProficientLanguages());
    }

    /**
     * Similar to {@link #getProficientLanguageList} except the the result is provided in
     * a {@link LinkedHashSet} to provide access to a unique ordered list.
     * @return a {@link LinkedHashSet} of languages the user is proficient using.
     */
    private LinkedHashSet<String> getProficientLanguages() {
        LinkedHashSet<String> uniqueLanguages = new LinkedHashSet<String>();
        // The primary language, according to the translation-service, always comes first.
        String primaryLanguage = getNativeTranslateServiceTargetLanguage();
        if (isValidLocale(primaryLanguage)) {
            uniqueLanguages.add(trimLocaleToLanguage(primaryLanguage));
        }
        // Merge in the IME locales, if possible.
        Context context = ContextUtils.getApplicationContext();
        if (context != null) {
            for (String locale : UiUtils.getIMELocales(context)) {
                if (isValidLocale(locale)) uniqueLanguages.add(trimLocaleToLanguage(locale));
            }
        }
        return uniqueLanguages;
    }

    /**
     * Gets the list of accept languages for this user.
     * @return The {@link List} of languages the user understands or does not want translated.
     */
    private List<String> getAcceptLanguages() {
        List<String> result = new ArrayList<String>();
        String acceptLanguages = getNativeAcceptLanguages();
        if (!TextUtils.isEmpty(acceptLanguages)) {
            for (String language : acceptLanguages.split(",")) {
                result.add(language);
            }
        }
        return result;
    }

    /**
     * @return Whether the given locale appears to be valid.
     */
    private boolean isValidLocale(String locale) {
        return !TextUtils.isEmpty(locale) && locale.length() >= LOCALE_MIN_LENGTH;
    }

    /**
     * Converts a given locale to a language code.
     * @param locale The locale string, which must be a valid locale (See #isValidLocale).
     * @return The given locale as a language code.
     */
    private String trimLocaleToLanguage(String locale) {
        // TODO(donnd): use getScript or getLanguageTag (both API 21), or some other standard way to
        // strip the country, instead of hard-coding the two character language code.
        // TODO(donnd): Shouldn't getLanguage() do this?
        String trimmedLocale = locale.substring(0, LOCALE_MIN_LENGTH);
        return new Locale(trimmedLocale).getLanguage();
    }

    /** @return whether we should use the Chrome Language Model due to the feature being enabled. */
    static private boolean useChromeLanguageModel() {
        return ChromeFeatureList.isEnabled(ChromeFeatureList.CONTEXTUAL_SEARCH_TRANSLATION_MODEL);
    }

    /**
     * @return The accept-languages string from the cache or from native code (when not cached).
     */
    protected String getNativeAcceptLanguages() {
        if (mAcceptLanguages == null) {
            mAcceptLanguages = mHost.getAcceptLanguages();
        }
        return mAcceptLanguages;
    }

    /**
     * @return The Translate Service's target language string from the cache or from
     *         native code (when not cached).
     */
    protected String getNativeTranslateServiceTargetLanguage() {
        if (mTranslateServiceTargetLanguage == null) {
            mTranslateServiceTargetLanguage = mHost.getTranslateServiceTargetLanguage();
        }
        return mTranslateServiceTargetLanguage;
    }
}

// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextualsearch;

import android.text.TextUtils;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.translate.TranslateBridge;

import java.util.LinkedHashSet;

/**
 * Controls how Translation One-box triggering is handled for the {@link ContextualSearchManager}.
 */
public class ContextualSearchTranslationImpl implements ContextualSearchTranslation {
    private final TranslateBridgeWrapper mTranslateBridgeWrapper;
    private final ContextualSearchPolicy mPolicy;
    /**
     * Creates a {@link ContextualSearchTranslation} for updating {@link ContextualSearchRequest}s
     * for translation.
     */
    ContextualSearchTranslationImpl(ContextualSearchPolicy policy) {
        mPolicy = policy;
        mTranslateBridgeWrapper = new TranslateBridgeWrapper();
    }

    /** Constructor useful for testing, uses the given {@link TranslateBridgeWrapper}. */
    ContextualSearchTranslationImpl(
            ContextualSearchPolicy policy, TranslateBridgeWrapper translateBridgeWrapper) {
        mPolicy = policy;
        mTranslateBridgeWrapper = translateBridgeWrapper;
    }

    @Override
    public void forceTranslateIfNeeded(
            ContextualSearchRequest searchRequest, String sourceLanguage) {
        if (needsTranslation(sourceLanguage)) {
            searchRequest.forceTranslation(sourceLanguage, getTranslateServiceTargetLanguage());
        }
    }

    @Override
    public void forceAutoDetectTranslateUnlessDisabled(ContextualSearchRequest searchRequest) {
        searchRequest.forceAutoDetectTranslation(getTranslateServiceTargetLanguage());
    }

    @Override
    public boolean needsTranslation(@Nullable String sourceLanguage) {
        if (TextUtils.isEmpty(sourceLanguage) || isBlockedLanguage(sourceLanguage)
                || mPolicy.isTranslationDisabled())
            return false;

        LinkedHashSet<String> languages = mTranslateBridgeWrapper.getModelLanguages();
        for (String language : languages) {
            if (language.equals(sourceLanguage)) return false;
        }
        return true;
    }

    @Override
    public String getTranslateServiceTargetLanguage() {
        return mTranslateBridgeWrapper.getTargetLanguage();
    }

    /** @return whether the given {@code language} is blocked for translation. */
    private boolean isBlockedLanguage(String language) {
        return mTranslateBridgeWrapper.isBlockedLanguage(language);
    }

    /**
     * Wraps our usage of the static methods in the {@link TranslateBridge} into a class that can be
     * mocked for testing.
     */
    static class TranslateBridgeWrapper {
        /** @return whether the given string is blocked for translation. */
        public boolean isBlockedLanguage(String language) {
            return TranslateBridge.isBlockedLanguage(language);
        }

        /**
         * @return The best target language based on what the Translate Service knows about the
         *         user.
         */
        public String getTargetLanguage() {
            return TranslateBridge.getTargetLanguage();
        }

        /**
         * @return The {@link LinkedHashSet} of language code strings that the Chrome Language Model
         *         thinks the user knows, in order of most familiar to least familiar.
         */
        public LinkedHashSet<String> getModelLanguages() {
            return TranslateBridge.getModelLanguages();
        }
    }
}

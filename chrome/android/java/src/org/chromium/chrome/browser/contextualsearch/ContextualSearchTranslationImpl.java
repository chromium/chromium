// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextualsearch;

import android.text.TextUtils;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.translate.TranslateBridge;

import java.util.List;

/** Controls how Translation triggering is handled for the {@link ContextualSearchManager}. */
public class ContextualSearchTranslationImpl implements ContextualSearchTranslation {
    private final TranslateBridgeWrapper mTranslateBridgeWrapper;

    /**
     * Creates a {@link ContextualSearchTranslation} for updating {@link ContextualSearchRequest}s
     * for translation.
     */
    public ContextualSearchTranslationImpl(Profile profile) {
        mTranslateBridgeWrapper = new TranslateBridgeWrapper(profile);
    }

    /** Constructor useful for testing, uses the given {@link TranslateBridgeWrapper}. */
    ContextualSearchTranslationImpl(TranslateBridgeWrapper translateBridgeWrapper) {
        mTranslateBridgeWrapper = translateBridgeWrapper;
    }

    @Override
    public void forceTranslateIfNeeded(
            ContextualSearchRequest searchRequest, String sourceLanguage, boolean isTapSelection) {
        if (needsTranslation(sourceLanguage)) {
            searchRequest.forceTranslation(sourceLanguage, getTranslateServiceTargetLanguage());
        }
    }

    @Override
    public void forceAutoDetectTranslateUnlessDisabled(ContextualSearchRequest searchRequest) {
        // TODO(donnd): Consider only forcing the auto-detect translation when we've detected a
        // language mismatch. Due to probable poor language recognition on the selection (without
        // any page content) we currently enable which relies on the server and search to decide.
        searchRequest.forceAutoDetectTranslation(getTranslateServiceTargetLanguage());
    }

    @Override
    public boolean needsTranslation(@Nullable String sourceLanguage) {
        if (TextUtils.isEmpty(sourceLanguage)) return false;

        List<String> languages = mTranslateBridgeWrapper.getNeverTranslateLanguages();
        for (String language : languages) {
            if (language.equals(sourceLanguage)) return false;
        }
        return true;
    }

    @Override
    public String getTranslateServiceTargetLanguage() {
        return mTranslateBridgeWrapper.getTargetLanguage();
    }

    @Override
    public String getTranslateServiceFluentLanguages() {
        return TextUtils.join(",", mTranslateBridgeWrapper.getNeverTranslateLanguages());
    }

    /**
     * Wraps our usage of the static methods in the {@link TranslateBridge} into a class that can be
     * mocked for testing.
     */
    static class TranslateBridgeWrapper {
        private final Profile mProfile;

        public TranslateBridgeWrapper(Profile profile) {
            mProfile = profile;
        }

        /**
         * @return The best target language based on what the Translate Service knows about the
         *     user.
         */
        public String getTargetLanguage() {
            return TranslateBridge.getTargetLanguage(mProfile);
        }

        /**
         * @return The {@link List} of languages the user has set to never translate, in
         *     alphabetical order.
         */
        public List<String> getNeverTranslateLanguages() {
            return TranslateBridge.getNeverTranslateLanguages(mProfile);
        }
    }
}

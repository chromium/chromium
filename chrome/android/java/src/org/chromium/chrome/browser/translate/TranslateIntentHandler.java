// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.translate;

import android.content.Intent;
import android.text.TextUtils;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;

import org.chromium.base.FeatureList;
import org.chromium.base.IntentUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.IntentHandler.IntentHandlerDelegate;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab.Tab;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Utility functions used for handling TRANSLATE_TAB intents.
 */
public class TranslateIntentHandler {
    /**
     * An intent to signal that Chrome should start translating the current foreground tab.
     */
    public static final String ACTION_TRANSLATE_TAB =
            "org.chromium.chrome.browser.translate.TRANSLATE_TAB";

    /**
     * The activity-alias that all TRANSLATE_TAB intents must be sent to.
     *
     * Intents sent to other activities cannot have the signature permission enforced. Translate
     * intents that do not match this component must be ignored.
     */
    public static final String COMPONENT_TRANSLATE_DISPATCHER =
            "com.google.android.apps.chrome.TranslateDispatcher";

    /**
     * Extra to indicate the target (ISO 639) language code of a translate intent.
     */
    public static final String EXTRA_TARGET_LANGUAGE_CODE =
            "com.android.chrome.translate.target_language_code";

    /**
     * Extra to indicate the expected URL of the page to be translated. This is to be used as a
     * check in case the tab changes between the translate info being provided to Assistant and when
     * the Translate intent is received. No translate will occur if this does not match the URL of
     * the current foreground tab.
     */
    public static final String EXTRA_EXPECTED_URL = "com.android.chrome.translate.expected_url";

    /**
     * Represents the outcome of processing a TRANSLATE_TAB intent.
     * DO NOT reorder items in this interface, because it's mirrored to UMA (as
     * TranslateTabIntentResult). Values should be enumerated from 0 and can't have gaps. When
     * removing items, comment them out and keep existing numeric values stable.
     */
    @IntDef({TranslateTabIntentResult.FEATURE_DISABLED, TranslateTabIntentResult.MISSING_AUTH_TOKEN,
            TranslateTabIntentResult.TAB_WAS_NULL, TranslateTabIntentResult.INCOGNITO_TAB,
            TranslateTabIntentResult.EXPECTED_URL_MISMATCH,
            TranslateTabIntentResult.CANNOT_TRANSLATE_TAB, TranslateTabIntentResult.SUCCESS})
    @Retention(RetentionPolicy.SOURCE)
    public @interface TranslateTabIntentResult {
        int FEATURE_DISABLED = 0;
        int MISSING_AUTH_TOKEN = 1;
        int TAB_WAS_NULL = 2;
        int INCOGNITO_TAB = 3;
        int EXPECTED_URL_MISMATCH = 4;
        int CANNOT_TRANSLATE_TAB = 5;
        int SUCCESS = 6;
        // Update TranslateTabIntentResult in enums.xml when adding new items.
        int NUM_ENTRIES = 7;
    }

    private static void recordTranslateTabResultUMA(@TranslateTabIntentResult int result) {
        RecordHistogram.recordEnumeratedHistogram(
                "Translate.TranslateTabIntentResult", result, TranslateTabIntentResult.NUM_ENTRIES);
    }

    /**
     * Translates the given tab to the given target language, if possible.
     * @param tab The tab in question.
     * @param targetLanguageCode The language to translate into. The user's preferred target
     *         language will be used if targetLanguageCode is null or empty.
     * @param expectedUrl The URL of the page that should be translated. If this doesn't match the
     *         current tab, no translation will be performed.
     */
    public static void translateTab(
            @Nullable Tab tab, @Nullable String targetLanguageCode, @Nullable String expectedUrl) {
        if (tab == null) {
            recordTranslateTabResultUMA(TranslateTabIntentResult.TAB_WAS_NULL);
            return;
        }
        if (tab.isIncognito()) {
            recordTranslateTabResultUMA(TranslateTabIntentResult.INCOGNITO_TAB);
            return;
        }
        if (expectedUrl == null || !expectedUrl.equals(tab.getUrl().getSpec())) {
            recordTranslateTabResultUMA(TranslateTabIntentResult.EXPECTED_URL_MISMATCH);
            return;
        }
        if (!TranslateUtils.canTranslateCurrentTab(tab)) {
            recordTranslateTabResultUMA(TranslateTabIntentResult.CANNOT_TRANSLATE_TAB);
            return;
        }

        if (targetLanguageCode == null || TextUtils.isEmpty(targetLanguageCode)) {
            TranslateBridge.translateTabWhenReady(tab);
        } else {
            TranslateBridge.translateTabToLanguage(tab, targetLanguageCode);
        }
        recordTranslateTabResultUMA(TranslateTabIntentResult.SUCCESS);
    }

    /**
     * Determines if a given intent is a TRANSLATE_TAB intent and handles it if the intent feature
     * is enabled.
     * @param intent The intent in question.
     * @param delegate A delegate used to interact with the Activity that received the intent.
     * @return True if the intent has been handled and should not be processed by fallback intent
     *         handlers. False otherwise.
     */
    public static boolean handleTranslateTabIntent(Intent intent, IntentHandlerDelegate delegate) {
        if (intent == null || !ACTION_TRANSLATE_TAB.equals(intent.getAction())) {
            return false;
        }

        assert FeatureList.isInitialized();
        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.TRANSLATE_INTENT)) {
            recordTranslateTabResultUMA(TranslateTabIntentResult.FEATURE_DISABLED);
            // Return false here to let fallback handlers process the intent. This preserves
            // externally visible behavior when the Feature is disabled.
            return false;
        }

        // Check to see if the auth token was added by LaunchIntentDispatcher. If so, we know that
        // the signature permission was enforced.
        if (!IntentHandler.wasIntentSenderChrome(intent)) {
            recordTranslateTabResultUMA(TranslateTabIntentResult.MISSING_AUTH_TOKEN);
            // Return true here to mark the intent as handled. This will prevent further fallback
            // logic from trying to handle the malformed translate intent.
            return true;
        }

        String languageCode = IntentUtils.safeGetStringExtra(intent, EXTRA_TARGET_LANGUAGE_CODE);
        String expectedUrl = IntentUtils.safeGetStringExtra(intent, EXTRA_EXPECTED_URL);

        delegate.processTranslateTabIntent(languageCode, expectedUrl);

        return true;
    }
}

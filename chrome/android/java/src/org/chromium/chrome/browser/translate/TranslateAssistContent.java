// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.translate;

import android.text.TextUtils;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;

import org.json.JSONException;
import org.json.JSONObject;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.MutableFlagWithSafeDefault;
import org.chromium.chrome.browser.tab.Tab;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Provides utilities related to providing translate information to Assistant.
 */
public class TranslateAssistContent {
    public static final String TYPE_KEY = "@type";
    public static final String TYPE_VALUE = "WebPage";
    public static final String URL_KEY = "url";
    public static final String IN_LANGUAGE_KEY = "inLanguage";
    public static final String WORK_TRANSLATION_KEY = "workTranslation";
    public static final String TRANSLATION_OF_WORK_KEY = "translationOfWork";
    private static final MutableFlagWithSafeDefault sTranslateAssistContentFlag =
            new MutableFlagWithSafeDefault(ChromeFeatureList.TRANSLATE_ASSIST_CONTENT, false);

    /**
     * Represents the result of attempting to attach translate data to AssistContent.
     * DO NOT reorder items in this interface, because it's mirrored to UMA (as
     * TranslateAssistContentResult). Values should be enumerated from 0 and can't have gaps.
     * When removing items, comment them out and keep existing numeric values stable.
     */
    @IntDef({TranslateAssistContentResult.FEATURE_DISABLED,
            TranslateAssistContentResult.TAB_WAS_NULL, TranslateAssistContentResult.INCOGNITO_TAB,
            TranslateAssistContentResult.OVERVIEW_MODE,
            TranslateAssistContentResult.CANNOT_TRANSLATE_TAB,
            TranslateAssistContentResult.MISSING_ORIGINAL_LANGUAGE,
            TranslateAssistContentResult.MISSING_CURRENT_LANGUAGE,
            TranslateAssistContentResult.JSON_EXCEPTION,
            TranslateAssistContentResult.TRANSLATABLE_PAGE,
            TranslateAssistContentResult.TRANSLATED_PAGE})
    @Retention(RetentionPolicy.SOURCE)
    public @interface TranslateAssistContentResult {
        int FEATURE_DISABLED = 0;
        int TAB_WAS_NULL = 1;
        int INCOGNITO_TAB = 2;
        int OVERVIEW_MODE = 3;
        int CANNOT_TRANSLATE_TAB = 4;
        int MISSING_ORIGINAL_LANGUAGE = 5;
        int MISSING_CURRENT_LANGUAGE = 6;
        int JSON_EXCEPTION = 7;
        int TRANSLATABLE_PAGE = 8;
        int TRANSLATED_PAGE = 9;
        // Update TranslateAssistContentResult in enums.xml when adding new items.
        int NUM_ENTRIES = 10;
    }

    private static void recordTranslateAssistContentResultUMA(
            @TranslateAssistContentResult int result) {
        RecordHistogram.recordEnumeratedHistogram("Translate.TranslateAssistContentResult", result,
                TranslateAssistContentResult.NUM_ENTRIES);
    }

    /**
     * Returns a StructuredData string populated with translate information. This is used to inform
     * the Assistant of the current page's translate state.
     * @param tab The tab to create translate data for.
     * @param isInOverviewMode Whether the ChromeActivity is in overview mode.
     * @return A JSON string of translate data to be surfaced to the Assistant via
     *         AssistContent#setStructuredData. Returns null if there was an issue creating the
     *         StructuredData or if the feature is disabled.
     */
    public static @Nullable String getTranslateDataForTab(
            @Nullable Tab tab, boolean isInOverviewMode) {
        if (!sTranslateAssistContentFlag.isEnabled()) {
            recordTranslateAssistContentResultUMA(TranslateAssistContentResult.FEATURE_DISABLED);
            return null;
        } else if (isInOverviewMode) {
            recordTranslateAssistContentResultUMA(TranslateAssistContentResult.OVERVIEW_MODE);
            return null;
        } else if (tab == null) {
            recordTranslateAssistContentResultUMA(TranslateAssistContentResult.TAB_WAS_NULL);
            return null;
        } else if (tab.isIncognito()) {
            recordTranslateAssistContentResultUMA(TranslateAssistContentResult.INCOGNITO_TAB);
            return null;
        }

        try {
            JSONObject structuredData =
                    new JSONObject().put(TYPE_KEY, TYPE_VALUE).put(URL_KEY, tab.getUrl().getSpec());
            if (!TranslateBridge.canManuallyTranslate(tab, /*menuLogging=*/false)) {
                recordTranslateAssistContentResultUMA(
                        TranslateAssistContentResult.CANNOT_TRANSLATE_TAB);
                return structuredData.toString();
            }

            String sourceLanguageCode = TranslateBridge.getSourceLanguage(tab);
            if (TextUtils.isEmpty(sourceLanguageCode)) {
                recordTranslateAssistContentResultUMA(
                        TranslateAssistContentResult.MISSING_ORIGINAL_LANGUAGE);
                return structuredData.toString();
            }
            String currentLanguageCode = TranslateBridge.getCurrentLanguage(tab);
            if (TextUtils.isEmpty(currentLanguageCode)) {
                recordTranslateAssistContentResultUMA(
                        TranslateAssistContentResult.MISSING_CURRENT_LANGUAGE);
                return structuredData.toString();
            }
            // The target language is not necessary for Assistant to decide whether to show the
            // translate UI.
            String targetLanguageCode = TranslateBridge.getTargetLanguage();

            structuredData.put(IN_LANGUAGE_KEY, currentLanguageCode);
            if (currentLanguageCode.equals(sourceLanguageCode)) {
                if (!TextUtils.isEmpty(targetLanguageCode)) {
                    structuredData.put(WORK_TRANSLATION_KEY,
                            new JSONObject().put(IN_LANGUAGE_KEY, targetLanguageCode));
                }
                recordTranslateAssistContentResultUMA(
                        TranslateAssistContentResult.TRANSLATABLE_PAGE);
            } else {
                structuredData.put(TRANSLATION_OF_WORK_KEY,
                        new JSONObject().put(IN_LANGUAGE_KEY, sourceLanguageCode));
                recordTranslateAssistContentResultUMA(TranslateAssistContentResult.TRANSLATED_PAGE);
            }
            return structuredData.toString();
        } catch (JSONException e) {
            recordTranslateAssistContentResultUMA(TranslateAssistContentResult.JSON_EXCEPTION);
            return null;
        }
    }
}

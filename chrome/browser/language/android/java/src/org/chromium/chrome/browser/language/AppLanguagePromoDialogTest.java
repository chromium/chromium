// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.language;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.LocaleUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.chrome.browser.language.settings.LanguageItem;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.LinkedHashSet;

/**
 * Tests for the AppPromoDialog class.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class AppLanguagePromoDialogTest {
    static final LanguageItem FOLLOW_SYSTEM = LanguageItem.makeFollowSystemLanguageItem();
    static final LanguageItem LANG_AF = new LanguageItem("af", "Afrikaans", "Afrikaans", true);
    static final LanguageItem LANG_AZ = new LanguageItem("az", "Azerbaijani", "azərbaycan", true);
    static final LanguageItem LANG_EN_GB =
            new LanguageItem("en-GB", "English (UK)", "English (UK)", true);
    static final LanguageItem LANG_EN_US =
            new LanguageItem("en-US", "English (United States)", "English (United States", true);
    static final LanguageItem LANG_ZU = new LanguageItem("zu", "Zulu", "isiZulu", true);
    // List of potential UI languages.
    static final LinkedHashSet<LanguageItem> UI_LANGUAGES =
            new LinkedHashSet<>(Arrays.asList(LANG_AF, LANG_AZ, LANG_EN_GB, LANG_EN_US, LANG_ZU));

    // Test getTopLanguagesHelper
    @Test
    @SmallTest
    public void testGetTopLanguagesHelper() {
        // Current override language is FOLLOW_SYSTEM, System Language is en-US
        LinkedHashSet<LanguageItem> topLanguages =
                AppLanguagePromoDialog.getTopLanguagesHelper(UI_LANGUAGES,
                        new LinkedHashSet<>(Arrays.asList("af", "an", "en-US", "en-AU", "zu")),
                        FOLLOW_SYSTEM, LocaleUtils.forLanguageTag("en-US"));
        Assert.assertEquals(
                new ArrayList<>(topLanguages), Arrays.asList(FOLLOW_SYSTEM, LANG_AF, LANG_ZU));

        // Current override language is FOLLOW_SYSTEM, System Language is Zulu
        topLanguages = AppLanguagePromoDialog.getTopLanguagesHelper(UI_LANGUAGES,
                new LinkedHashSet<>(Arrays.asList("en-US", "en-AU", "an", "af", "zu")),
                FOLLOW_SYSTEM, LocaleUtils.forLanguageTag("zu"));
        Assert.assertEquals(
                new ArrayList<>(topLanguages), Arrays.asList(FOLLOW_SYSTEM, LANG_EN_US, LANG_AF));

        // Current override language is en-US, System Language is en-US
        topLanguages = AppLanguagePromoDialog.getTopLanguagesHelper(UI_LANGUAGES,
                new LinkedHashSet<>(Arrays.asList("zu", "af", "an", "en-AU", "en-US")), LANG_EN_US,
                LocaleUtils.forLanguageTag("en-US"));
        Assert.assertEquals(
                new ArrayList<>(topLanguages), Arrays.asList(LANG_EN_US, LANG_ZU, LANG_AF));

        // Current override language is Afrikaans, System Language is Zulu
        topLanguages = AppLanguagePromoDialog.getTopLanguagesHelper(UI_LANGUAGES,
                new LinkedHashSet<>(Arrays.asList("an", "en-US", "en-AU", "zu", "af")), LANG_AF,
                LocaleUtils.forLanguageTag("zu"));
        Assert.assertEquals(
                new ArrayList<>(topLanguages), Arrays.asList(FOLLOW_SYSTEM, LANG_AF, LANG_EN_US));

        // Current override language is Afrikaans, System Language is Afrikaans (South Africa)
        topLanguages = AppLanguagePromoDialog.getTopLanguagesHelper(UI_LANGUAGES,
                new LinkedHashSet<>(Arrays.asList("af-ZA", "an", "zu", "en-US", "en-AU")), LANG_AF,
                LocaleUtils.forLanguageTag("af-ZA"));
        Assert.assertEquals(
                new ArrayList<>(topLanguages), Arrays.asList(LANG_AF, LANG_ZU, LANG_EN_US));

        // Current override language is en-US, System Language is Afrikaans (South Africa)
        topLanguages = AppLanguagePromoDialog.getTopLanguagesHelper(UI_LANGUAGES,
                new LinkedHashSet<>(Arrays.asList("af-ZA", "af", "an", "en-US", "en-AU", "zu")),
                LANG_EN_US, LocaleUtils.forLanguageTag("af-ZA"));
        Assert.assertEquals(
                new ArrayList<>(topLanguages), Arrays.asList(FOLLOW_SYSTEM, LANG_EN_US, LANG_ZU));

        // Current override language is FOLLOW_SYSTEM, System Language is Afrikaans (South Africa)
        topLanguages = AppLanguagePromoDialog.getTopLanguagesHelper(UI_LANGUAGES,
                new LinkedHashSet<>(Arrays.asList("af-ZA", "af", "an", "en-US", "en-AU", "zu")),
                FOLLOW_SYSTEM, LocaleUtils.forLanguageTag("af-ZA"));
        Assert.assertEquals(
                new ArrayList<>(topLanguages), Arrays.asList(FOLLOW_SYSTEM, LANG_EN_US, LANG_ZU));
    }

    // Test isOverrideLanguageOriginalSystemLanguage
    @Test
    @SmallTest
    public void testIsOverrideLanguageOriginalSystemLanguage() {
        // Only one UI variant: Afrikaans
        Assert.assertTrue(AppLanguagePromoDialog.isOverrideLanguageOriginalSystemLanguage(
                LANG_AF, LocaleUtils.forLanguageTag("af-ZA")));

        // Multiple UI variants: en-US
        Assert.assertFalse(AppLanguagePromoDialog.isOverrideLanguageOriginalSystemLanguage(
                LANG_EN_GB, LocaleUtils.forLanguageTag("en-US")));
        Assert.assertTrue(AppLanguagePromoDialog.isOverrideLanguageOriginalSystemLanguage(
                LANG_EN_US, LocaleUtils.forLanguageTag("en-US")));

        // Follow system language
        Assert.assertFalse(AppLanguagePromoDialog.isOverrideLanguageOriginalSystemLanguage(
                FOLLOW_SYSTEM, LocaleUtils.forLanguageTag("zu")));
    }
}

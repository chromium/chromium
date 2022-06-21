// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.language;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.LocaleUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.language.AppLanguagePromoDialog.LanguageItemAdapter;
import org.chromium.chrome.browser.language.settings.LanguageItem;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.LinkedHashSet;

/**
 * Tests for the AppPromoDialog class.
 */
@RunWith(BaseRobolectricTestRunner.class)
public class AppLanguagePromoDialogTest {
    LanguageItem mFollowSystem;
    LanguageItem mLangAf;
    LanguageItem mLangAz;
    LanguageItem mLangEnGb;
    LanguageItem mLangEnUs;
    LanguageItem mLangZu;
    // List of potential UI languages.
    LinkedHashSet<LanguageItem> mUiLanguages;

    @Before
    public void setUp() {
        LanguageTestUtils.initializeResourceBundleForTesting();
        mFollowSystem = LanguageItem.makeFollowSystemLanguageItem();
        mLangAf = new LanguageItem("af", "Afrikaans", "Afrikaans", true);
        mLangAz = new LanguageItem("az", "Azerbaijani", "azərbaycan", true);
        mLangEnGb = new LanguageItem("en-GB", "English (UK)", "English (UK)", true);
        mLangEnUs = new LanguageItem(
                "en-US", "English (United States)", "English (United States", true);
        mLangZu = new LanguageItem("zu", "Zulu", "isiZulu", true);
        mUiLanguages =
                new LinkedHashSet<>(Arrays.asList(mLangAf, mLangAz, mLangEnGb, mLangEnUs, mLangZu));
    }

    @After
    public void tearDown() {
        LanguageTestUtils.clearResourceBundleForTesting();
    }

    // Test getTopLanguagesHelper
    @Test
    @SmallTest
    public void testGetTopLanguagesHelper() {
        // Current override language is mFollowSystem, System Language is en-US
        LinkedHashSet<LanguageItem> topLanguages =
                AppLanguagePromoDialog.getTopLanguagesHelper(mUiLanguages,
                        new LinkedHashSet<>(Arrays.asList("af", "an", "en-US", "en-AU", "zu")),
                        mFollowSystem, LocaleUtils.forLanguageTag("en-US"));
        Assert.assertEquals(
                new ArrayList<>(topLanguages), Arrays.asList(mFollowSystem, mLangAf, mLangZu));

        // Current override language is mFollowSystem, System Language is Zulu
        topLanguages = AppLanguagePromoDialog.getTopLanguagesHelper(mUiLanguages,
                new LinkedHashSet<>(Arrays.asList("en-US", "en-AU", "an", "af", "zu")),
                mFollowSystem, LocaleUtils.forLanguageTag("zu"));
        Assert.assertEquals(
                new ArrayList<>(topLanguages), Arrays.asList(mFollowSystem, mLangEnUs, mLangAf));

        // Current override language is en-US, System Language is en-US
        topLanguages = AppLanguagePromoDialog.getTopLanguagesHelper(mUiLanguages,
                new LinkedHashSet<>(Arrays.asList("zu", "af", "an", "en-AU", "en-US")), mLangEnUs,
                LocaleUtils.forLanguageTag("en-US"));
        Assert.assertEquals(
                new ArrayList<>(topLanguages), Arrays.asList(mLangEnUs, mLangZu, mLangAf));

        // Current override language is Afrikaans, System Language is Zulu
        topLanguages = AppLanguagePromoDialog.getTopLanguagesHelper(mUiLanguages,
                new LinkedHashSet<>(Arrays.asList("an", "en-US", "en-AU", "zu", "af")), mLangAf,
                LocaleUtils.forLanguageTag("zu"));
        Assert.assertEquals(
                new ArrayList<>(topLanguages), Arrays.asList(mFollowSystem, mLangAf, mLangEnUs));

        // Current override language is Afrikaans, System Language is Afrikaans (South Africa)
        topLanguages = AppLanguagePromoDialog.getTopLanguagesHelper(mUiLanguages,
                new LinkedHashSet<>(Arrays.asList("af-ZA", "an", "zu", "en-US", "en-AU")), mLangAf,
                LocaleUtils.forLanguageTag("af-ZA"));
        Assert.assertEquals(
                new ArrayList<>(topLanguages), Arrays.asList(mLangAf, mLangZu, mLangEnUs));

        // Current override language is en-US, System Language is Afrikaans (South Africa)
        topLanguages = AppLanguagePromoDialog.getTopLanguagesHelper(mUiLanguages,
                new LinkedHashSet<>(Arrays.asList("af-ZA", "af", "an", "en-US", "en-AU", "zu")),
                mLangEnUs, LocaleUtils.forLanguageTag("af-ZA"));
        Assert.assertEquals(
                new ArrayList<>(topLanguages), Arrays.asList(mFollowSystem, mLangEnUs, mLangZu));

        // Current override language is mFollowSystem, System Language is Afrikaans (South Africa)
        topLanguages = AppLanguagePromoDialog.getTopLanguagesHelper(mUiLanguages,
                new LinkedHashSet<>(Arrays.asList("af-ZA", "af", "an", "en-US", "en-AU", "zu")),
                mFollowSystem, LocaleUtils.forLanguageTag("af-ZA"));
        Assert.assertEquals(
                new ArrayList<>(topLanguages), Arrays.asList(mFollowSystem, mLangEnUs, mLangZu));
    }

    // Test isOverrideLanguageOriginalSystemLanguage
    @Test
    @SmallTest
    public void testIsOverrideLanguageOriginalSystemLanguage() {
        // Only one UI variant: Afrikaans
        Assert.assertTrue(AppLanguagePromoDialog.isOverrideLanguageOriginalSystemLanguage(
                mLangAf, LocaleUtils.forLanguageTag("af-ZA")));

        // Multiple UI variants: en-US
        Assert.assertFalse(AppLanguagePromoDialog.isOverrideLanguageOriginalSystemLanguage(
                mLangEnGb, LocaleUtils.forLanguageTag("en-US")));
        Assert.assertTrue(AppLanguagePromoDialog.isOverrideLanguageOriginalSystemLanguage(
                mLangEnUs, LocaleUtils.forLanguageTag("en-US")));

        // Follow system language
        Assert.assertFalse(AppLanguagePromoDialog.isOverrideLanguageOriginalSystemLanguage(
                mFollowSystem, LocaleUtils.forLanguageTag("zu")));
    }

    // Test LanguageItemAdapter
    @Test
    @SmallTest
    public void testLanguageItemAdapter() {
        ArrayList<LanguageItem> topLanguages = new ArrayList<>(Arrays.asList(mLangAf, mLangAz));
        ArrayList<LanguageItem> otherLanguages =
                new ArrayList<>(Arrays.asList(mLangEnGb, mLangEnUs, mLangZu));
        LanguageItem currentLanguage = mLangAf;
        LanguageItemAdapter adapter =
                new LanguageItemAdapter(topLanguages, otherLanguages, currentLanguage);

        // Only the top languages plus "More languages" item are showing to start.
        Assert.assertEquals(3, adapter.getItemCount());

        adapter.showOtherLanguages();
        // All languages should now be showing.
        Assert.assertEquals(6, adapter.getItemCount());
    }
}

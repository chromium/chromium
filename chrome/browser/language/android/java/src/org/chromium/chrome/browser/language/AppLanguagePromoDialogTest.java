// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.language;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.LocaleUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.language.AppLanguagePromoDialog.LanguageItemAdapter;
import org.chromium.chrome.browser.language.settings.LanguageItem;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.translate.FakeTranslateBridgeJni;
import org.chromium.chrome.browser.translate.TranslateBridgeJni;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collection;
import java.util.LinkedHashSet;

/** Tests for the AppPromoDialog class. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class AppLanguagePromoDialogTest {
    @Rule public JniMocker mJniMocker = new JniMocker();

    @Mock private Profile mProfile;

    LanguageItem mFollowSystem;
    LanguageItem mLangAf;
    LanguageItem mLangAz;
    LanguageItem mLangEnGb;
    LanguageItem mLangEnUs;
    LanguageItem mLangEs;
    LanguageItem mLangZu;
    // List of potential UI languages.
    LinkedHashSet<LanguageItem> mUiLanguages;

    FakeLanguageBridgeJni mFakeLanguageBridge;
    FakeTranslateBridgeJni mFakeTranslateBridge;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        LanguageTestUtils.initializeResourceBundleForTesting();
        mFollowSystem = LanguageItem.makeFollowSystemLanguageItem();
        mLangAf = new LanguageItem("af", "Afrikaans", "Afrikaans", true);
        mLangAz = new LanguageItem("az", "Azerbaijani", "azərbaycan", true);
        mLangEnGb = new LanguageItem("en-GB", "English (UK)", "English (UK)", true);
        mLangEnUs =
                new LanguageItem(
                        "en-US", "English (United States)", "English (United States", true);
        mLangEs = new LanguageItem("es", "Spanish", "español", true);
        mLangZu = new LanguageItem("zu", "Zulu", "isiZulu", true);
        mUiLanguages =
                new LinkedHashSet<>(Arrays.asList(mLangAf, mLangAz, mLangEnGb, mLangEnUs, mLangZu));

        // Setup fake translate and language preferences.
        mFakeTranslateBridge = new FakeTranslateBridgeJni();
        mJniMocker.mock(TranslateBridgeJni.TEST_HOOKS, mFakeTranslateBridge);

        mFakeLanguageBridge = new FakeLanguageBridgeJni();
        mJniMocker.mock(LanguageBridgeJni.TEST_HOOKS, mFakeLanguageBridge);
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
                AppLanguagePromoDialog.getTopLanguagesHelper(
                        mUiLanguages,
                        new LinkedHashSet<>(Arrays.asList("af", "an", "en-US", "en-AU", "zu")),
                        mFollowSystem,
                        LocaleUtils.forLanguageTag("en-US"));
        Assert.assertEquals(
                new ArrayList<>(topLanguages), Arrays.asList(mFollowSystem, mLangAf, mLangZu));

        // Current override language is mFollowSystem, System Language is Zulu
        topLanguages =
                AppLanguagePromoDialog.getTopLanguagesHelper(
                        mUiLanguages,
                        new LinkedHashSet<>(Arrays.asList("en-US", "en-AU", "an", "af", "zu")),
                        mFollowSystem,
                        LocaleUtils.forLanguageTag("zu"));
        Assert.assertEquals(
                new ArrayList<>(topLanguages), Arrays.asList(mFollowSystem, mLangEnUs, mLangAf));

        // Current override language is en-US, System Language is en-US
        topLanguages =
                AppLanguagePromoDialog.getTopLanguagesHelper(
                        mUiLanguages,
                        new LinkedHashSet<>(
                                Arrays.asList("zu", "af", "an", "en-AU", "en-US", "en-GB")),
                        mLangEnUs,
                        LocaleUtils.forLanguageTag("en-US"));
        Assert.assertEquals(
                new ArrayList<>(topLanguages),
                Arrays.asList(mLangEnUs, mLangZu, mLangAf, mLangEnGb));

        // Current override language is Afrikaans, System Language is Zulu
        topLanguages =
                AppLanguagePromoDialog.getTopLanguagesHelper(
                        mUiLanguages,
                        new LinkedHashSet<>(Arrays.asList("an", "en-US", "en-AU", "zu", "af")),
                        mLangAf,
                        LocaleUtils.forLanguageTag("zu"));
        Assert.assertEquals(
                new ArrayList<>(topLanguages), Arrays.asList(mFollowSystem, mLangAf, mLangEnUs));

        // Current override language is Afrikaans, System Language is Afrikaans (South Africa)
        topLanguages =
                AppLanguagePromoDialog.getTopLanguagesHelper(
                        mUiLanguages,
                        new LinkedHashSet<>(Arrays.asList("af-ZA", "an", "zu", "en-US", "en-AU")),
                        mLangAf,
                        LocaleUtils.forLanguageTag("af-ZA"));
        Assert.assertEquals(
                new ArrayList<>(topLanguages), Arrays.asList(mLangAf, mLangZu, mLangEnUs));

        // Current override language is en-US, System Language is Afrikaans (South Africa)
        topLanguages =
                AppLanguagePromoDialog.getTopLanguagesHelper(
                        mUiLanguages,
                        new LinkedHashSet<>(
                                Arrays.asList("af-ZA", "af", "an", "en-US", "en-AU", "zu")),
                        mLangEnUs,
                        LocaleUtils.forLanguageTag("af-ZA"));
        Assert.assertEquals(
                new ArrayList<>(topLanguages), Arrays.asList(mFollowSystem, mLangEnUs, mLangZu));

        // Current override language is mFollowSystem, System Language is Afrikaans (South Africa)
        topLanguages =
                AppLanguagePromoDialog.getTopLanguagesHelper(
                        mUiLanguages,
                        new LinkedHashSet<>(
                                Arrays.asList("af-ZA", "af", "an", "en-US", "en-AU", "zu")),
                        mFollowSystem,
                        LocaleUtils.forLanguageTag("af-ZA"));
        Assert.assertEquals(
                new ArrayList<>(topLanguages), Arrays.asList(mFollowSystem, mLangEnUs, mLangZu));

        // Test that country specific top languages are converted to their base language.
        topLanguages =
                AppLanguagePromoDialog.getTopLanguagesHelper(
                        mUiLanguages,
                        new LinkedHashSet<>(
                                Arrays.asList(
                                        "af-ZA", "af-NA", "an", "as", "en-US", "en-AU", "zu-XX")),
                        mFollowSystem,
                        LocaleUtils.forLanguageTag("en-US"));
        Assert.assertEquals(
                new ArrayList<>(topLanguages), Arrays.asList(mFollowSystem, mLangAf, mLangZu));
    }

    // Test getPotentialUILanguage
    @Test
    @SmallTest
    public void testGetPotentialUILanguage() {
        LinkedHashSet<String> uiLanguages =
                new LinkedHashSet<>(Arrays.asList("af", "en-US", "en-GB", "es", "es-419"));
        Assert.assertEquals(
                AppLanguagePromoDialog.getPotentialUILanguage("af-ZA", uiLanguages), "af");
        Assert.assertEquals(
                AppLanguagePromoDialog.getPotentialUILanguage("en-GB", uiLanguages), "en-GB");
        Assert.assertEquals(
                AppLanguagePromoDialog.getPotentialUILanguage("en-ZA", uiLanguages), "en");
        Assert.assertEquals(
                AppLanguagePromoDialog.getPotentialUILanguage("es-AR", uiLanguages), "es");
        Assert.assertEquals(
                AppLanguagePromoDialog.getPotentialUILanguage("es-419", uiLanguages), "es-419");
    }

    // Test LanguageItemAdapter getItemCount
    @Test
    @SmallTest
    public void testLanguageItemAdapterGetItemCount() {
        LanguageItemAdapter adapter =
                makeLanguageItemAdapter(
                        Arrays.asList(mLangAf, mLangAz), // top languages
                        Arrays.asList(mLangEnGb, mLangEnUs, mLangZu), // other languages,
                        mLangAf // current language
                        );

        // Only the top languages plus "More languages" item are showing to start.
        Assert.assertEquals(3, adapter.getItemCount());
        Assert.assertFalse(adapter.areOtherLanguagesShown());

        adapter.showOtherLanguages();
        // All languages should now be showing.
        Assert.assertEquals(6, adapter.getItemCount());
        Assert.assertTrue(adapter.areOtherLanguagesShown());
    }

    // Test LanguageItemAdapter getLanguageItemAt
    @Test
    @SmallTest
    public void testLanguageItemAdapterGetLanguageItemAt() {
        LanguageItemAdapter adapter =
                makeLanguageItemAdapter(
                        Arrays.asList(mLangAf, mLangAz), // top languages
                        Arrays.asList(mLangEnGb, mLangEnUs, mLangZu), // other languages,
                        mLangAf // current language
                        );

        Assert.assertEquals(mLangAz, adapter.getLanguageItemAt(1)); // topLanguage
        Assert.assertEquals(mLangEnGb, adapter.getLanguageItemAt(3)); // otherLanguage
        Assert.assertThrows(AssertionError.class, () -> adapter.getLanguageItemAt(2)); // separator
    }

    // Test LanguageItemAdapter getPositionForLanguageItem
    @Test
    @SmallTest
    public void testLanguageItemAdapterGetPositionForLanguageItem() {
        LanguageItemAdapter adapter =
                makeLanguageItemAdapter(
                        Arrays.asList(mLangAf, mLangAz), // top languages
                        Arrays.asList(mLangEnGb, mLangEnUs, mLangZu), // other languages,
                        mLangAf // current language
                        );

        Assert.assertEquals(1, adapter.getPositionForLanguageItem(mLangAz)); // topLanguage
        Assert.assertEquals(3, adapter.getPositionForLanguageItem(mLangEnGb)); // otherLanguage
        Assert.assertEquals(-1, adapter.getPositionForLanguageItem(mLangEs)); // not found
    }

    // Test LanguageItemAdapter getItemViewType
    @Test
    @SmallTest
    public void testLanguageItemAdapterGetItemViewType() {
        LanguageItemAdapter adapter =
                makeLanguageItemAdapter(
                        Arrays.asList(mLangAf, mLangAz), // top languages
                        Arrays.asList(mLangEnGb, mLangEnUs, mLangZu), // other languages,
                        mLangAf // current language
                        );

        // More Languages is showing to start
        Assert.assertEquals(AppLanguagePromoDialog.ItemType.LANGUAGE, adapter.getItemViewType(0));
        Assert.assertEquals(AppLanguagePromoDialog.ItemType.LANGUAGE, adapter.getItemViewType(1));
        Assert.assertEquals(
                AppLanguagePromoDialog.ItemType.MORE_LANGUAGES, adapter.getItemViewType(2));
        Assert.assertEquals(AppLanguagePromoDialog.ItemType.LANGUAGE, adapter.getItemViewType(4));

        adapter.showOtherLanguages();

        // The separator is showing after
        Assert.assertEquals(AppLanguagePromoDialog.ItemType.LANGUAGE, adapter.getItemViewType(0));
        Assert.assertEquals(AppLanguagePromoDialog.ItemType.LANGUAGE, adapter.getItemViewType(1));
        Assert.assertEquals(AppLanguagePromoDialog.ItemType.SEPARATOR, adapter.getItemViewType(2));
        Assert.assertEquals(AppLanguagePromoDialog.ItemType.LANGUAGE, adapter.getItemViewType(4));
    }

    // Test LanguageItemAdapter setSelectedLanguage
    @Test
    @SmallTest
    public void testLanguageItemAdapterSetSelectedLanguage() {
        LanguageItemAdapter adapter =
                makeLanguageItemAdapter(
                        Arrays.asList(mLangAf, mLangAz), // top languages
                        Arrays.asList(mLangEnGb, mLangEnUs, mLangZu), // other languages,
                        mLangAf // current language
                        );

        Assert.assertTrue(adapter.isTopLanguageSelected());
        Assert.assertEquals(mLangAf, adapter.getSelectedLanguage());
        adapter.setSelectedLanguage(1);
        Assert.assertEquals(mLangAz, adapter.getSelectedLanguage());
        Assert.assertThrows(AssertionError.class, () -> adapter.setSelectedLanguage(2));
        adapter.setSelectedLanguage(4);
        Assert.assertFalse(adapter.isTopLanguageSelected());
        Assert.assertEquals(mLangEnUs, adapter.getSelectedLanguage());
    }

    // Test shouldShowPrompt conditions
    @Test
    @SmallTest
    public void testShouldShowPrompt() {
        final boolean online = true;

        // Prompt should not be shown if the top ULP has a base match with the current default
        // locale ("en-US" in tests).
        mFakeLanguageBridge.setULPLanguages(Arrays.asList("en-AU"));
        Assert.assertFalse(AppLanguagePromoDialog.shouldShowPrompt(mProfile, online));
        mFakeLanguageBridge.setULPLanguages(Arrays.asList("fr"));
        Assert.assertTrue(AppLanguagePromoDialog.shouldShowPrompt(mProfile, online));
        mFakeLanguageBridge.setULPLanguages(Arrays.asList("fr", "en-US"));
        Assert.assertTrue(AppLanguagePromoDialog.shouldShowPrompt(mProfile, online));

        // Prompt should not be shown if not online.
        Assert.assertFalse(AppLanguagePromoDialog.shouldShowPrompt(mProfile, !online));

        // Prompt should not be shown if ULP languages are empty.
        mFakeLanguageBridge.setULPLanguages(new ArrayList<>());
        Assert.assertFalse(AppLanguagePromoDialog.shouldShowPrompt(mProfile, online));

        // Prompt is not shown if it has been shown before.
        mFakeLanguageBridge.setULPLanguages(Arrays.asList("fr"));
        Assert.assertTrue(AppLanguagePromoDialog.shouldShowPrompt(mProfile, online));
        mFakeTranslateBridge.setAppLanguagePromptShown(true);
        Assert.assertFalse(AppLanguagePromoDialog.shouldShowPrompt(mProfile, online));
    }

    private static LanguageItemAdapter makeLanguageItemAdapter(
            Collection<LanguageItem> topLanguages,
            Collection<LanguageItem> otherLanguages,
            LanguageItem currentLanguage) {
        return new AppLanguagePromoDialog.LanguageItemAdapter(
                topLanguages, otherLanguages, currentLanguage);
    }
}

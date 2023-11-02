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
import org.robolectric.annotation.Config;
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;

import org.chromium.base.LocaleUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.language.AppLanguagePromoDialog.LanguageItemAdapter;
import org.chromium.chrome.browser.language.settings.LanguageItem;
import org.chromium.chrome.browser.translate.FakeTranslateBridgeJni;
import org.chromium.chrome.browser.translate.TranslateBridgeJni;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.LinkedHashSet;

/**
 * Tests for the AppPromoDialog class.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE,
        shadows = {AppLanguagePromoDialogTest.ShadowChromeFeatureList.class})
public class AppLanguagePromoDialogTest {
    @Rule
    public JniMocker mJniMocker = new JniMocker();

    LanguageItem mFollowSystem;
    LanguageItem mLangAf;
    LanguageItem mLangAz;
    LanguageItem mLangEnGb;
    LanguageItem mLangEnUs;
    LanguageItem mLangZu;
    // List of potential UI languages.
    LinkedHashSet<LanguageItem> mUiLanguages;

    /**
     * Shadow class to control app language prompt features.
     */
    @Implements(ChromeFeatureList.class)
    static class ShadowChromeFeatureList {
        static boolean sEnableForceAppLanguagePrompt;
        static boolean sEnableAppLanguagePrompt;
        static boolean sEnableAppLanguagePromptULP;

        @Implementation
        public static boolean isEnabled(String featureName) {
            if (featureName.equals(ChromeFeatureList.FORCE_APP_LANGUAGE_PROMPT)) {
                return sEnableForceAppLanguagePrompt;
            } else if (featureName.equals(ChromeFeatureList.APP_LANGUAGE_PROMPT)) {
                return sEnableAppLanguagePrompt;
            } else if (featureName.equals(ChromeFeatureList.APP_LANGUAGE_PROMPT_ULP)) {
                return sEnableAppLanguagePromptULP;
            }
            return false;
        }
    }

    FakeLanguageBridgeJni mFakeLanguageBridge;
    FakeTranslateBridgeJni mFakeTranslateBridge;

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
                new LinkedHashSet<>(Arrays.asList("zu", "af", "an", "en-AU", "en-US", "en-GB")),
                mLangEnUs, LocaleUtils.forLanguageTag("en-US"));
        Assert.assertEquals(new ArrayList<>(topLanguages),
                Arrays.asList(mLangEnUs, mLangZu, mLangAf, mLangEnGb));

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

        // Test that country specific top languages are converted to their base language.
        topLanguages = AppLanguagePromoDialog.getTopLanguagesHelper(mUiLanguages,
                new LinkedHashSet<>(
                        Arrays.asList("af-ZA", "af-NA", "an", "as", "en-US", "en-AU", "zu-XX")),
                mFollowSystem, LocaleUtils.forLanguageTag("en-US"));
        Assert.assertEquals(
                new ArrayList<>(topLanguages), Arrays.asList(mFollowSystem, mLangAf, mLangZu));
    }

    // Test isOverrideLanguageOriginalSystemLanguage
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

    // Test shouldShowPrompt conditions
    @Test
    @SmallTest
    public void testShouldShowPrompt() {
        mFakeLanguageBridge.setULPLanguages(Arrays.asList("en-US"));
        final boolean online = true;
        // With feature disabled prompt is not shown.
        Assert.assertFalse(AppLanguagePromoDialog.shouldShowPrompt(online));

        // With feature enabled ONLINE status is returned.
        ShadowChromeFeatureList.sEnableAppLanguagePrompt = true;
        Assert.assertTrue(AppLanguagePromoDialog.shouldShowPrompt(online));
        Assert.assertFalse(AppLanguagePromoDialog.shouldShowPrompt(!online));

        // With ULP match feature enabled the prompt should not be shown if the top ULP has a
        // base match with the current default locale ("en-US" in tests).
        ShadowChromeFeatureList.sEnableAppLanguagePromptULP = true;
        mFakeLanguageBridge.setULPLanguages(Arrays.asList("en-AU"));
        Assert.assertFalse(AppLanguagePromoDialog.shouldShowPrompt(online));
        mFakeLanguageBridge.setULPLanguages(Arrays.asList("fr"));
        Assert.assertTrue(AppLanguagePromoDialog.shouldShowPrompt(online));
        mFakeLanguageBridge.setULPLanguages(Arrays.asList("fr", "en-US"));
        Assert.assertTrue(AppLanguagePromoDialog.shouldShowPrompt(online));

        // Prompt should not be shown if ULP languages are empty.
        mFakeLanguageBridge.setULPLanguages(new ArrayList<>());
        Assert.assertFalse(AppLanguagePromoDialog.shouldShowPrompt(online));

        ShadowChromeFeatureList.sEnableAppLanguagePromptULP = false;

        // Prompt is not shown if it has been shown before.
        Assert.assertTrue(AppLanguagePromoDialog.shouldShowPrompt(online));
        mFakeTranslateBridge.setAppLanguagePromptShown(true);
        Assert.assertFalse(AppLanguagePromoDialog.shouldShowPrompt(online));

        // Prompt is shown if it is forced on for testing.
        ShadowChromeFeatureList.sEnableForceAppLanguagePrompt = true;
        Assert.assertTrue(AppLanguagePromoDialog.shouldShowPrompt(online));
    }
}

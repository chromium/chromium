// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.language.settings;

import android.text.TextUtils;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.language.AppLocaleUtils;
import org.chromium.chrome.browser.language.LanguageTestUtils;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.translate.FakeTranslateBridgeJni;
import org.chromium.chrome.browser.translate.TranslateBridge;
import org.chromium.chrome.browser.translate.TranslateBridgeJni;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.stream.Collectors;

/** Tests for {@link LanguagesManager} which gets language lists from native. */
@RunWith(BaseRobolectricTestRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class LanguagesManagerTest {
    @Rule public JniMocker mJniMocker = new JniMocker();

    private FakeTranslateBridgeJni mFakeTranslateBridge;
    @Mock private Profile mProfile;

    @Before
    public void setUp() throws Exception {
        MockitoAnnotations.initMocks(this);
        Mockito.when(mProfile.getOriginalProfile()).thenReturn(mProfile);
        ProfileManager.setLastUsedProfileForTesting(mProfile);
        LanguageTestUtils.initializeResourceBundleForTesting();
        // Setup fake translate and language preferences.
        List<LanguageItem> chromeLanguages = FakeTranslateBridgeJni.getSimpleLanguageItemList();
        List<String> acceptLanguages = Arrays.asList("sw", "en", "en-US");
        List<String> neverLanguages = Arrays.asList("en");
        List<String> alwaysLanguages = new ArrayList();
        String targetLanguage = "en";
        mFakeTranslateBridge =
                new FakeTranslateBridgeJni(
                        chromeLanguages,
                        acceptLanguages,
                        neverLanguages,
                        alwaysLanguages,
                        targetLanguage);
        mJniMocker.mock(TranslateBridgeJni.TEST_HOOKS, mFakeTranslateBridge);
    }

    @After
    public void tearDown() {
        LanguageTestUtils.clearResourceBundleForTesting();
    }

    /** Tests for getting the potential accept languages. */
    @Test
    @SmallTest
    public void testGetPotentialAcceptLanguages() {
        List<LanguageItem> items =
                LanguagesManager.getForProfile(mProfile)
                        .getPotentialLanguages(LanguagesManager.LanguageListType.ACCEPT_LANGUAGES);

        // The default accept languages list is "sw,en,en-US". Those languages should not be
        // in the potential languages for Accept Languages.
        Assert.assertFalse(containsLanguage(items, "sw"));
        Assert.assertFalse(containsLanguage(items, "en"));
        Assert.assertFalse(containsLanguage(items, "en-US"));
        // But other languages should be.
        Assert.assertEquals(mFakeTranslateBridge.getChromeLanguagesCount() - 3, items.size());
        Assert.assertTrue(containsLanguage(items, "en-GB"));

        // The first language should be Afrikaans.
        Assert.assertEquals(items.get(0).getCode(), "af");

        // Add "af" to front of Accept-Languages.
        items = LanguagesManager.getForProfile(mProfile).getUserAcceptLanguageItems();
        items.add(0, LanguagesManager.getForProfile(mProfile).getLanguageItem("af"));
        setOrder(items);

        items =
                LanguagesManager.getForProfile(mProfile)
                        .getPotentialLanguages(LanguagesManager.LanguageListType.ACCEPT_LANGUAGES);
        Assert.assertFalse(containsLanguage(items, "en"));
        Assert.assertFalse(containsLanguage(items, "en-US"));
        // Now "af" should not be in the list.
        Assert.assertFalse(containsLanguage(items, "af"));
        Assert.assertTrue(containsLanguage(items, "en-GB"));
    }

    /** Tests for getting the potential UI languages. */
    @Test
    @SmallTest
    public void testGetPotentialUiLanguages() {
        // Set UI Language to Swahili.
        AppLocaleUtils.setAppLanguagePref("sw");

        List<LanguageItem> items =
                LanguagesManager.getForProfile(mProfile)
                        .getPotentialLanguages(LanguagesManager.LanguageListType.UI_LANGUAGES);
        Assert.assertFalse(containsLanguage(items, "en"));
        Assert.assertTrue(containsLanguage(items, "en-US"));
        Assert.assertTrue(containsLanguage(items, "en-GB"));

        // Languages that can not be UI languages are not present.
        Assert.assertFalse(containsLanguage(items, "xh"));
        Assert.assertFalse(containsLanguage(items, "wa"));

        // Check that the current UI language (Swahili) is not on the list.
        Assert.assertFalse(containsLanguage(items, "sw"));

        // Check that the first language is the system default language.
        Assert.assertTrue(AppLocaleUtils.isFollowSystemLanguage(items.get(0).getCode()));
        // Check that the second language is "en-US" from the Accept-Languages.
        Assert.assertEquals(items.get(1).getCode(), "en-US");

        // Set UI Language to system default.
        AppLocaleUtils.setAppLanguagePref(AppLocaleUtils.APP_LOCALE_USE_SYSTEM_LANGUAGE);

        items =
                LanguagesManager.getForProfile(mProfile)
                        .getPotentialLanguages(LanguagesManager.LanguageListType.UI_LANGUAGES);

        // Check that system default is not on the list and that German is.
        Assert.assertFalse(containsLanguage(items, AppLocaleUtils.APP_LOCALE_USE_SYSTEM_LANGUAGE));
        // Check that the fist languages are from the Accept-Languages.
        Assert.assertEquals(items.get(0).getCode(), "sw");
        Assert.assertEquals(items.get(1).getCode(), "en-US");
    }

    /** Tests for getting the all UI languages. */
    @Test
    @SmallTest
    public void testGetAllPossibleUiLanguages() {
        List<LanguageItem> items =
                LanguagesManager.getForProfile(mProfile).getAllPossibleUiLanguages();
        List<String> itemCodes = items.stream().map(i -> i.getCode()).collect(Collectors.toList());
        Assert.assertEquals(itemCodes, Arrays.asList("af", "en-GB", "en-US", "fil", "hi", "sw"));
    }

    /** Tests for getting the potential target languages. */
    @Test
    @SmallTest
    public void testGetPotentialTargetLanguages() {
        List<LanguageItem> items =
                LanguagesManager.getForProfile(mProfile)
                        .getPotentialLanguages(LanguagesManager.LanguageListType.TARGET_LANGUAGES);
        Assert.assertFalse(containsLanguage(items, "en-US"));
        Assert.assertFalse(containsLanguage(items, "en-GB"));
        Assert.assertTrue(containsLanguage(items, "fil"));

        // Check that the default target language "en" is not in the list.
        Assert.assertFalse(containsLanguage(items, "en"));

        // Check that the first language is "sw" from the Accept-Languages.
        Assert.assertEquals(items.get(0).getCode(), "sw");

        // Set the target language to "fil" (Filipino) which is "tl" as a Translate language.
        TranslateBridge.setDefaultTargetLanguage(mProfile, "fil");
        items =
                LanguagesManager.getForProfile(mProfile)
                        .getPotentialLanguages(LanguagesManager.LanguageListType.TARGET_LANGUAGES);
        Assert.assertFalse(containsLanguage(items, "fil"));
        Assert.assertTrue(containsLanguage(items, "en"));

        // Set the target language to "sw" (Swahili).
        TranslateBridge.setDefaultTargetLanguage(mProfile, "sw");
        items =
                LanguagesManager.getForProfile(mProfile)
                        .getPotentialLanguages(LanguagesManager.LanguageListType.TARGET_LANGUAGES);
        Assert.assertFalse(containsLanguage(items, "sw"));
        Assert.assertTrue(containsLanguage(items, "fil"));
    }

    /** Tests for getting the potential always translate languages. */
    @Test
    @SmallTest
    public void testGetPotentialAlwaysLanguages() {
        List<LanguageItem> items =
                LanguagesManager.getForProfile(mProfile)
                        .getPotentialLanguages(LanguagesManager.LanguageListType.ALWAYS_LANGUAGES);
        int itemsCount = items.size();

        Assert.assertTrue(containsLanguage(items, "en"));
        Assert.assertFalse(containsLanguage(items, "en-US"));
        Assert.assertFalse(containsLanguage(items, "en-GB"));
        Assert.assertTrue(containsLanguage(items, "fil"));

        // Check that the first language is "sw" from the Accept-Languages.
        Assert.assertEquals(items.get(0).getCode(), "sw");

        // Add English and Filipino to always translate languages.
        TranslateBridge.setLanguageAlwaysTranslateState(mProfile, "en", true);
        TranslateBridge.setLanguageAlwaysTranslateState(mProfile, "fil", true);

        items =
                LanguagesManager.getForProfile(mProfile)
                        .getPotentialLanguages(LanguagesManager.LanguageListType.ALWAYS_LANGUAGES);
        Assert.assertFalse(containsLanguage(items, "fil"));
        Assert.assertFalse(containsLanguage(items, "en"));
        Assert.assertEquals(itemsCount - 2, items.size());
    }

    /** Test for getting the potential never translate languages. */
    @Test
    @SmallTest
    public void testGetPotentialNeverLanguages() {
        List<LanguageItem> items =
                LanguagesManager.getForProfile(mProfile)
                        .getPotentialLanguages(LanguagesManager.LanguageListType.NEVER_LANGUAGES);
        int itemsCount = items.size();

        // English is the default never translate language.
        Assert.assertFalse(containsLanguage(items, "en"));
        Assert.assertFalse(containsLanguage(items, "en-US"));
        Assert.assertFalse(containsLanguage(items, "en-GB"));
        Assert.assertTrue(containsLanguage(items, "af"));
        Assert.assertTrue(containsLanguage(items, "fil"));

        // Check that the second language is "sw" from the Accept-Languages.
        Assert.assertEquals(items.get(0).getCode(), "sw");

        TranslateBridge.setLanguageBlockedState(mProfile, "fil", true);
        TranslateBridge.setLanguageBlockedState(mProfile, "sw", true);

        items =
                LanguagesManager.getForProfile(mProfile)
                        .getPotentialLanguages(LanguagesManager.LanguageListType.NEVER_LANGUAGES);
        Assert.assertFalse(containsLanguage(items, "fil"));
        Assert.assertFalse(containsLanguage(items, "sw"));
        Assert.assertEquals(itemsCount - 2, items.size());
    }

    /**
     * @param languageList List of LanguageItems.
     * @param language String language code to check for.
     * @return boolean True if |languageList has a {@link LanguageItem} matching |language|.
     */
    private boolean containsLanguage(List<LanguageItem> languageList, String language) {
        for (LanguageItem item : languageList) {
            if (TextUtils.equals(item.getCode(), language)) return true;
        }
        return false;
    }

    /** param languages List of LanguageItems in the order to set Accept-Languages to. */
    private void setOrder(List<LanguageItem> languages) {
        String[] codes = new String[languages.size()];
        int i = 0;
        for (LanguageItem item : languages) {
            codes[i++] = item.getCode();
        }
        TranslateBridge.setLanguageOrder(mProfile, codes);
    }
}

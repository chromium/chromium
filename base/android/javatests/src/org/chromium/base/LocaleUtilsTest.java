// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import android.annotation.SuppressLint;
import android.content.res.Configuration;
import android.os.Build;
import android.os.LocaleList;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.MinAndroidSdkLevel;

import java.util.Locale;

/** Tests for the LocaleUtils class. */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class LocaleUtilsTest {
    // This is also a part of test for toLanguageTag when API level is lower than 24
    @Test
    @SmallTest
    public void testGetUpdatedLanguageForChromium() {
        String language = "en";
        String updatedLanguage = LocaleUtils.getUpdatedLanguageForChromium(language);
        Assert.assertEquals(language, updatedLanguage);

        language = "iw";
        updatedLanguage = LocaleUtils.getUpdatedLanguageForChromium(language);
        Assert.assertEquals("he", updatedLanguage);

        language = "ji";
        updatedLanguage = LocaleUtils.getUpdatedLanguageForChromium(language);
        Assert.assertEquals("yi", updatedLanguage);

        language = "in";
        updatedLanguage = LocaleUtils.getUpdatedLanguageForChromium(language);
        Assert.assertEquals("id", updatedLanguage);

        language = "tl";
        updatedLanguage = LocaleUtils.getUpdatedLanguageForChromium(language);
        Assert.assertEquals("fil", updatedLanguage);

        language = "gom";
        updatedLanguage = LocaleUtils.getUpdatedLanguageForChromium(language);
        Assert.assertEquals("kok", updatedLanguage);
    }

    // This is also a part of test for toLanguageTags when API level is 24 or higher
    @Test
    @SmallTest
    public void testGetUpdatedLocaleForChromium() {
        Locale locale = new Locale("jp");
        Locale updatedLocale = LocaleUtils.getUpdatedLocaleForChromium(locale);
        Assert.assertEquals(locale, updatedLocale);

        locale = new Locale("iw");
        updatedLocale = LocaleUtils.getUpdatedLocaleForChromium(locale);
        Assert.assertEquals(new Locale("he"), updatedLocale);

        locale = new Locale("ji");
        updatedLocale = LocaleUtils.getUpdatedLocaleForChromium(locale);
        Assert.assertEquals(new Locale("yi"), updatedLocale);

        locale = new Locale("in");
        updatedLocale = LocaleUtils.getUpdatedLocaleForChromium(locale);
        Assert.assertEquals(new Locale("id"), updatedLocale);

        locale = new Locale("tl");
        updatedLocale = LocaleUtils.getUpdatedLocaleForChromium(locale);
        Assert.assertEquals(new Locale("fil"), updatedLocale);
    }

    // This is also a part of test for forLanguageTag when API level is lower than 21
    @Test
    @SmallTest
    public void testGetUpdatedLanguageForAndroid() {
        String language = "en";
        String updatedLanguage = LocaleUtils.getUpdatedLanguageForAndroid(language);
        Assert.assertEquals(language, updatedLanguage);

        language = "und";
        updatedLanguage = LocaleUtils.getUpdatedLanguageForAndroid(language);
        Assert.assertEquals("", updatedLanguage);

        language = "fil";
        updatedLanguage = LocaleUtils.getUpdatedLanguageForAndroid(language);
        Assert.assertEquals("tl", updatedLanguage);
    }

    // This is also a part of test for forLanguageTag when API level is 21 or higher
    @Test
    @SmallTest
    public void testGetUpdatedLocaleForAndroid() {
        Locale locale = new Locale("jp");
        Locale updatedLocale = LocaleUtils.getUpdatedLocaleForAndroid(locale);
        Assert.assertEquals(locale, updatedLocale);

        locale = new Locale("und");
        updatedLocale = LocaleUtils.getUpdatedLocaleForAndroid(locale);
        Assert.assertEquals(new Locale(""), updatedLocale);

        locale = new Locale("fil");
        updatedLocale = LocaleUtils.getUpdatedLocaleForAndroid(locale);
        Assert.assertEquals(new Locale("tl"), updatedLocale);
    }

    // Test for toLanguageTag when API level is lower than 24
    @Test
    @SmallTest
    public void testToLanguageTag() {
        Locale locale = new Locale("en", "US");
        String localeString = LocaleUtils.toLanguageTag(locale);
        Assert.assertEquals("en-US", localeString);

        locale = new Locale("jp");
        localeString = LocaleUtils.toLanguageTag(locale);
        Assert.assertEquals("jp", localeString);

        locale = new Locale("mas");
        localeString = LocaleUtils.toLanguageTag(locale);
        Assert.assertEquals("mas", localeString);

        locale = new Locale("es", "005");
        localeString = LocaleUtils.toLanguageTag(locale);
        Assert.assertEquals("es-005", localeString);

        locale = new Locale("iw");
        localeString = LocaleUtils.toLanguageTag(locale);
        Assert.assertEquals("he", localeString);

        locale = new Locale("ji");
        localeString = LocaleUtils.toLanguageTag(locale);
        Assert.assertEquals("yi", localeString);

        locale = new Locale("in", "ID");
        localeString = LocaleUtils.toLanguageTag(locale);
        Assert.assertEquals("id-ID", localeString);

        locale = new Locale("tl", "PH");
        localeString = LocaleUtils.toLanguageTag(locale);
        Assert.assertEquals("fil-PH", localeString);

        locale = new Locale("no", "NO", "NY");
        localeString = LocaleUtils.toLanguageTag(locale);
        Assert.assertEquals("nn-NO", localeString);
    }

    // Test for toLanguageTags when API level is 24 or higher
    @Test
    @SmallTest
    @MinAndroidSdkLevel(Build.VERSION_CODES.N)
    @SuppressLint("NewApi")
    public void testToLanguageTags() {
        Locale locale1 = new Locale("en", "US");
        Locale locale2 = new Locale("es", "005");
        LocaleList localeList = new LocaleList(locale1, locale2);
        String localeString = LocaleUtils.toLanguageTags(localeList);
        Assert.assertEquals("en-US,es-005", localeString);

        locale1 = new Locale("jp");
        locale2 = new Locale("mas");
        localeList = new LocaleList(locale1, locale2);
        localeString = LocaleUtils.toLanguageTags(localeList);
        Assert.assertEquals("jp,mas", localeString);

        locale1 = new Locale("iw");
        locale2 = new Locale("ji");
        localeList = new LocaleList(locale1, locale2);
        localeString = LocaleUtils.toLanguageTags(localeList);
        Assert.assertEquals("he,yi", localeString);

        locale1 = new Locale("in", "ID");
        locale2 = new Locale("tl", "PH");
        localeList = new LocaleList(locale1, locale2);
        localeString = LocaleUtils.toLanguageTags(localeList);
        Assert.assertEquals("id-ID,fil-PH", localeString);

        locale1 = new Locale("no", "NO", "NY");
        localeList = new LocaleList(locale1);
        localeString = LocaleUtils.toLanguageTags(localeList);
        Assert.assertEquals("nn-NO", localeString);
    }

    // Test for toLanguage.
    @Test
    @SmallTest
    public void testToLanguage() {
        Assert.assertEquals("en", LocaleUtils.toBaseLanguage("en-US"));
        Assert.assertEquals("en", LocaleUtils.toBaseLanguage("en"));
        Assert.assertEquals("", LocaleUtils.toBaseLanguage("-"));
        Assert.assertEquals("", LocaleUtils.toBaseLanguage("-US"));
        Assert.assertEquals("", LocaleUtils.toBaseLanguage(""));
        Assert.assertEquals("fil", LocaleUtils.toBaseLanguage("fil"));
    }

    // Test for isBaseLanguageEqual
    @Test
    @SmallTest
    public void testIsBaseLanguageEqual() {
        Assert.assertTrue(LocaleUtils.isBaseLanguageEqual("pt-PT", "pt-PT"));
        Assert.assertTrue(LocaleUtils.isBaseLanguageEqual("pt-PT", "pt"));
        Assert.assertTrue(LocaleUtils.isBaseLanguageEqual("pt", "pt-PT-xx"));
        Assert.assertTrue(LocaleUtils.isBaseLanguageEqual("zh-Hans-CN", "zh-HK"));
        Assert.assertTrue(LocaleUtils.isBaseLanguageEqual("", ""));

        Assert.assertFalse(LocaleUtils.isBaseLanguageEqual("en-US", "es-US"));
        Assert.assertFalse(LocaleUtils.isBaseLanguageEqual("af", "zu"));
        Assert.assertFalse(LocaleUtils.isBaseLanguageEqual("af", ""));
        Assert.assertFalse(LocaleUtils.isBaseLanguageEqual("", "zu"));
    }

    // Test for getConfigurationLocale < N
    @Test
    @SmallTest
    public void testGetConfigurationLocale() {
        Configuration config = new Configuration();
        Assert.assertEquals("", LocaleUtils.getConfigurationLanguage(config));

        config.setLocale(Locale.forLanguageTag("hi-IN"));
        Assert.assertEquals("hi-IN", LocaleUtils.getConfigurationLanguage(config));

        config.setLocale(new Locale("ar"));
        Assert.assertEquals("ar", LocaleUtils.getConfigurationLanguage(config));
    }

    // Test for getConfigurationLocale N+ (with LocaleList)
    @Test
    @SmallTest
    @MinAndroidSdkLevel(Build.VERSION_CODES.N)
    public void testGetConfigurationN() {
        Configuration config = new Configuration();

        Locale locale1 = new Locale("hi", "IN");
        Locale locale2 = new Locale("tl", "PH");
        LocaleList localeList = new LocaleList(locale1, locale2);
        config.setLocales(localeList);
        Assert.assertEquals("hi-IN", LocaleUtils.getConfigurationLanguage(config));

        locale1 = new Locale("ceb");
        locale2 = new Locale("tl", "PH");
        localeList = new LocaleList(locale1, locale2);
        config.setLocales(localeList);
        Assert.assertEquals("ceb", LocaleUtils.getConfigurationLanguage(config));
    }

    // Test for setDefaultLocalesFromConfiguration
    @Test
    @SmallTest
    public void testSetDefaultLocalesFromConfiguration() {
        Configuration config = new Configuration();
        config.setLocale(new Locale("tl", "PH"));
        LocaleUtils.setDefaultLocalesFromConfiguration(config);
        Assert.assertEquals("tl-PH", Locale.getDefault().toLanguageTag());

        config.setLocale(new Locale("es", "AR"));
        LocaleUtils.setDefaultLocalesFromConfiguration(config);
        Assert.assertEquals("es-AR", Locale.getDefault().toLanguageTag());
    }

    // Test for setDefaultLocalesFromConfiguration N+ (with LocaleList)
    @Test
    @SmallTest
    @MinAndroidSdkLevel(Build.VERSION_CODES.N)
    public void testSetDefaultLocalesFromConfigurationN() {
        Configuration config = new Configuration();
        String tags = "tl-PH,es-AR,en";
        config.setLocales(LocaleList.forLanguageTags(tags));
        LocaleUtils.setDefaultLocalesFromConfiguration(config);
        Assert.assertEquals("tl-PH", Locale.getDefault().toLanguageTag());
        Assert.assertEquals(tags, LocaleList.getDefault().toLanguageTags());

        tags = "en,en-US,en-GB";
        config.setLocales(LocaleList.forLanguageTags(tags));
        LocaleUtils.setDefaultLocalesFromConfiguration(config);
        Assert.assertEquals("en", Locale.getDefault().toLanguageTag());
        Assert.assertEquals(tags, LocaleList.getDefault().toLanguageTags());
    }

    // Test for prependToLocaleList
    @Test
    @SmallTest
    @MinAndroidSdkLevel(Build.VERSION_CODES.N)
    public void testPrependToLocaleList() {
        // Prepend to empty list
        LocaleList resultList = LocaleUtils.ApisN.prependToLocaleList("ceb-PH", new LocaleList());
        Assert.assertEquals("ceb-PH", resultList.toLanguageTags());

        // Prepend and not in list
        LocaleList baseList = LocaleList.forLanguageTags("en,es-ES,fr");
        resultList = LocaleUtils.ApisN.prependToLocaleList("zu", baseList);
        Assert.assertEquals("zu,en,es-ES,fr", resultList.toLanguageTags());

        // Prepend and in middle of list
        resultList = LocaleUtils.ApisN.prependToLocaleList("es-ES", baseList);
        Assert.assertEquals("es-ES,en,fr", resultList.toLanguageTags());

        // Prepend and at end of list
        resultList = LocaleUtils.ApisN.prependToLocaleList("fr", baseList);
        Assert.assertEquals("fr,en,es-ES", resultList.toLanguageTags());

        // Prepend and at front of list
        resultList = LocaleUtils.ApisN.prependToLocaleList("en", baseList);
        Assert.assertEquals("en,es-ES,fr", resultList.toLanguageTags());

        // Prepend to list of one
        baseList = LocaleList.forLanguageTags("fr");
        resultList = LocaleUtils.ApisN.prependToLocaleList("en", baseList);
        Assert.assertEquals("en,fr", resultList.toLanguageTags());

        // Prepend to list of one (self)
        resultList = LocaleUtils.ApisN.prependToLocaleList("fr", baseList);
        Assert.assertEquals("fr", resultList.toLanguageTags());
    }
}

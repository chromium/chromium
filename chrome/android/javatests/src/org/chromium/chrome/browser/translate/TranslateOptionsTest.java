// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.translate;

import android.support.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.infobar.TranslateOptions;

/**
 * Test for TranslateOptions.
 */
@RunWith(BaseJUnit4ClassRunner.class)
public class TranslateOptionsTest {
    private static final boolean ALWAYS_TRANSLATE = true;
    private static final String[] LANGUAGES = {"English", "Spanish", "French"};
    private static final String[] CODES = {"en", "es", "fr"};
    private static final int[] UMA_HASH_CODES = {10, 20, 30};

    @Before
    public void setUp() {}

    @Test
    @SmallTest
    @Feature({"Translate"})
    public void testNoChanges() {
        TranslateOptions options = TranslateOptions.create(
                "en", "es", LANGUAGES, CODES, ALWAYS_TRANSLATE, false, null);
        Assert.assertEquals("English", options.sourceLanguageName());
        Assert.assertEquals("Spanish", options.targetLanguageName());
        Assert.assertEquals("en", options.sourceLanguageCode());
        Assert.assertEquals("es", options.targetLanguageCode());
        Assert.assertFalse(options.getTranslateState(TranslateOptions.Type.NEVER_LANGUAGE));
        Assert.assertTrue(options.getTranslateState(TranslateOptions.Type.ALWAYS_LANGUAGE));
        Assert.assertFalse(options.getTranslateState(TranslateOptions.Type.NEVER_DOMAIN));
        Assert.assertFalse(options.optionsChanged());
        Assert.assertNull(options.getUMAHashCodeFromCode("en"));
    }

    @Test
    @SmallTest
    @Feature({"Translate"})
    public void testBasicLanguageChanges() {
        TranslateOptions options = TranslateOptions.create(
                "en", "es", LANGUAGES, CODES, !ALWAYS_TRANSLATE, true, UMA_HASH_CODES);
        options.setTargetLanguage("fr");
        options.setSourceLanguage("en");
        Assert.assertEquals("English", options.sourceLanguageName());
        Assert.assertEquals("French", options.targetLanguageName());
        Assert.assertEquals("en", options.sourceLanguageCode());
        Assert.assertEquals("fr", options.targetLanguageCode());
        Assert.assertTrue(options.triggeredFromMenu());
        Assert.assertEquals(Integer.valueOf(10), options.getUMAHashCodeFromCode("en"));
        Assert.assertEquals("English", options.getRepresentationFromCode("en"));

        Assert.assertTrue(options.optionsChanged());

        // Switch back to the original
        options.setSourceLanguage("en");
        options.setTargetLanguage("es");
        Assert.assertFalse(options.optionsChanged());
    }

    @Test
    @SmallTest
    @Feature({"Translate"})
    public void testInvalidLanguageChanges() {
        TranslateOptions options = TranslateOptions.create(
                "en", "es", LANGUAGES, CODES, ALWAYS_TRANSLATE, false, null);

        // Same target language as source
        Assert.assertFalse(options.setTargetLanguage("en"));
        Assert.assertFalse(options.optionsChanged());

        // Target language does not exist
        Assert.assertFalse(options.setTargetLanguage("aaa"));
        Assert.assertFalse(options.optionsChanged());

        // Same source and target
        Assert.assertFalse(options.setSourceLanguage("es"));
        Assert.assertFalse(options.optionsChanged());

        // Source language does not exist
        Assert.assertFalse(options.setSourceLanguage("bbb"));
        Assert.assertFalse(options.optionsChanged());
    }

    @Test
    @SmallTest
    @Feature({"Translate"})
    public void testBasicOptionsChanges() {
        TranslateOptions options = TranslateOptions.create(
                "en", "es", LANGUAGES, CODES, !ALWAYS_TRANSLATE, false, null);
        Assert.assertFalse(options.optionsChanged());
        options.toggleNeverTranslateDomainState(true);
        Assert.assertTrue(options.getTranslateState(TranslateOptions.Type.NEVER_DOMAIN));
        Assert.assertFalse(options.getTranslateState(TranslateOptions.Type.ALWAYS_LANGUAGE));
        Assert.assertFalse(options.getTranslateState(TranslateOptions.Type.NEVER_LANGUAGE));
        Assert.assertTrue(options.optionsChanged());
        options.toggleNeverTranslateDomainState(false);
        Assert.assertFalse(options.getTranslateState(TranslateOptions.Type.NEVER_DOMAIN));
        Assert.assertFalse(options.getTranslateState(TranslateOptions.Type.NEVER_LANGUAGE));
        Assert.assertFalse(options.getTranslateState(TranslateOptions.Type.ALWAYS_LANGUAGE));

        // We are back to the original state
        Assert.assertFalse(options.optionsChanged());
        options.toggleAlwaysTranslateLanguageState(true);
        Assert.assertFalse(options.getTranslateState(TranslateOptions.Type.NEVER_DOMAIN));
        Assert.assertFalse(options.getTranslateState(TranslateOptions.Type.NEVER_LANGUAGE));
        Assert.assertTrue(options.getTranslateState(TranslateOptions.Type.ALWAYS_LANGUAGE));
        Assert.assertTrue(options.optionsChanged());
    }

    @Test
    @SmallTest
    @Feature({"Translate"})
    public void testInvalidOptionsChanges() {
        TranslateOptions options = TranslateOptions.create(
                "en", "es", LANGUAGES, CODES, ALWAYS_TRANSLATE, false, null);

        // Never translate language should not work, but never translate domain
        // should
        Assert.assertFalse(options.toggleNeverTranslateLanguageState(true));
        options.toggleNeverTranslateDomainState(true);
        Assert.assertTrue(options.optionsChanged());

        Assert.assertTrue(options.toggleAlwaysTranslateLanguageState(false));

        // Never options are ok
        Assert.assertTrue(options.toggleNeverTranslateLanguageState(true));
        options.toggleNeverTranslateDomainState(true);

        // But always is not now
        Assert.assertFalse(options.toggleAlwaysTranslateLanguageState(true));
    }
}

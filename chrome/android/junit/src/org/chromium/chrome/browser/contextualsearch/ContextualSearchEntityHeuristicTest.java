// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextualsearch;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.when;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.BlockJUnit4ClassRunner;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.JniMocker;

import java.util.Locale;

/**
 * Tests parts of the {@link ContextualSearchEntityHeuristic} class.
 */
@RunWith(BlockJUnit4ClassRunner.class)
public class ContextualSearchEntityHeuristicTest {
    private static final String SAMPLE_TEXT =
            "Now Barack Obama, Michelle are not the best examples.  And Clinton is ambiguous.";
    private static final String UTF_8 = "UTF-8";

    @Rule
    public JniMocker mocker = new JniMocker();

    @Mock
    private ContextualSearchContext.Natives mContextJniMock;

    private ContextualSearchContext mContext;
    private ContextualSearchEntityHeuristic mEntityHeuristic;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mocker.mock(ContextualSearchContextJni.TEST_HOOKS, mContextJniMock);
    }

    private void setupInstanceToTest(Locale locale, int tapOffset) {
        mContext = new ContextualSearchContext.ChangeIgnoringContext();
        mContext.setSurroundingText(UTF_8, SAMPLE_TEXT, tapOffset, tapOffset);
        when(mContextJniMock.detectLanguage(anyLong(), eq(mContext)))
                .thenReturn(locale.getLanguage());
        mEntityHeuristic = ContextualSearchEntityHeuristic.testInstance(mContext, true);
    }

    private void setupTapInObama(Locale locale) {
        setupInstanceToTest(locale, "Now Barack Oba".length());
    }

    private void setupTapInEnglishStartOfBuffer() {
        setupInstanceToTest(Locale.US, 0);
    }

    private void setupTapInEnglishClinton() {
        setupInstanceToTest(Locale.US,
                "Now Barack Obama, Michelle are not the best examples.  And Clin".length());
    }

    private void setupTapInWordAfterComma() {
        setupInstanceToTest(Locale.US, "Now Barack Obama, Mich".length());
    }

    @Test
    @Feature({"ContextualSearch", "EntityHeuristic"})
    public void testTapInProperNounRecognized() {
        setupTapInObama(Locale.US);
        assertTrue(mEntityHeuristic.isProbablyEntityBasedOnCamelCase());
    }

    @Test
    @Feature({"ContextualSearch", "EntityHeuristic"})
    public void testTapInProperNounNotEnglishNotRecognized() {
        setupTapInObama(Locale.GERMANY);
        assertFalse(mEntityHeuristic.isProbablyEntityBasedOnCamelCase());
    }

    @Test
    @Feature({"ContextualSearch", "EntityHeuristic"})
    public void testTapInProperNounAfterPeriodNotRecognized() {
        setupTapInEnglishClinton();
        assertFalse(mEntityHeuristic.isProbablyEntityBasedOnCamelCase());
    }

    @Test
    @Feature({"ContextualSearch", "EntityHeuristic"})
    public void testTapInStartOfTextBufferNotRecognized() {
        setupTapInEnglishStartOfBuffer();
        assertFalse(mEntityHeuristic.isProbablyEntityBasedOnCamelCase());
    }

    @Test
    @Feature({"ContextualSearch", "EntityHeuristic"})
    public void testTapInSingleWordAfterCommaNotRecognized() {
        setupTapInWordAfterComma();
        assertFalse(mEntityHeuristic.isProbablyEntityBasedOnCamelCase());
    }

    private ContextualSearchEntityHeuristic setupHeuristic(
            String language, String start, String text) {
        ContextualSearchContext context = new ContextualSearchContext.ChangeIgnoringContext();
        when(mContextJniMock.detectLanguage(anyLong(), eq(context))).thenReturn(language);
        assert text.startsWith(start);
        int tapOffset = start.length();
        context.setSurroundingText(UTF_8, text, tapOffset, tapOffset);
        return ContextualSearchEntityHeuristic.testInstance(context, true);
    }

    @Test
    @Feature({"ContextualSearch", "EntityHeuristic"})
    public void testSpanish() {
        String spanish = "es";
        ContextualSearchEntityHeuristic entityHeuristicLo =
                setupHeuristic(spanish, "Lo", "Los comunicadores Ángel Monagas y Gervis Medina.");
        assertFalse(entityHeuristicLo.isProbablyEntityBasedOnCamelCase());
        ContextualSearchEntityHeuristic entityHeuristicAng = setupHeuristic(spanish,
                "Los comunicadores Áng", "Los comunicadores Ángel Monagas y Gervis Medina.");
        assertTrue(entityHeuristicAng.isProbablyEntityBasedOnCamelCase());
    }

    @Test
    @Feature({"ContextualSearch", "EntityHeuristic"})
    public void testRussian() {
        String russian = "ru";
        String text =
                "В этом заслуга в первую очередь издателя Михаила Гринберга, сумевшего обеспечить "
                + "высокий уровень редактирования, предпечатной подготовки и полиграфического "
                + "исполнения.";
        ContextualSearchEntityHeuristic entityHeuristicRussian = setupHeuristic(
                russian, "В этом заслуга в первую очередь издателя Михаила Гри", text);
        assertTrue(entityHeuristicRussian.isProbablyEntityBasedOnCamelCase());
        assertFalse(
                setupHeuristic(russian, "В этом засл", text).isProbablyEntityBasedOnCamelCase());
    }

    @Test
    @Feature({"ContextualSearch", "EntityHeuristic"})
    public void testTapWithDiacriticalLetters() {
        String english = Locale.ENGLISH.getLanguage();
        String phrase = "The former president of Brazil, Luiz Inácio Lula, is...";
        ContextualSearchEntityHeuristic entityHeuristicBrazil =
                setupHeuristic(english, "The former president of Braz", phrase);
        assertFalse(entityHeuristicBrazil.isProbablyEntityBasedOnCamelCase());
        ContextualSearchEntityHeuristic entityHeuristicLuiz =
                setupHeuristic(english, "The former president of Brazil, Lui", phrase);
        assertTrue(entityHeuristicLuiz.isProbablyEntityBasedOnCamelCase());
        ContextualSearchEntityHeuristic entityHeuristicInácio =
                setupHeuristic(english, "The former president of Brazil, Luiz Inác", phrase);
        assertTrue(entityHeuristicInácio.isProbablyEntityBasedOnCamelCase());
        ContextualSearchEntityHeuristic entityHeuristicLula =
                setupHeuristic(english, "The former president of Brazil, Luiz Inácio Lula", phrase);
        assertTrue(entityHeuristicLula.isProbablyEntityBasedOnCamelCase());
    }

    private ContextualSearchEntityHeuristic setupObamaHeuristic(String language) {
        String text = SAMPLE_TEXT;
        String start = text.substring(0, 7);
        return setupHeuristic(language, start, text);
    }

    @Test
    @Feature({"ContextualSearch", "EntityHeuristic"})
    public void testTapInManyLanguages() {
        assertTrue(setupObamaHeuristic("en").isProbablyEntityBasedOnCamelCase());
        assertTrue(setupObamaHeuristic("es").isProbablyEntityBasedOnCamelCase());
        assertTrue(setupObamaHeuristic("pt").isProbablyEntityBasedOnCamelCase());
        assertTrue(setupObamaHeuristic("ru").isProbablyEntityBasedOnCamelCase());
        assertTrue(setupObamaHeuristic("fr").isProbablyEntityBasedOnCamelCase());
        assertTrue(setupObamaHeuristic("it").isProbablyEntityBasedOnCamelCase());
        assertFalse(setupObamaHeuristic("de").isProbablyEntityBasedOnCamelCase());
    }
}

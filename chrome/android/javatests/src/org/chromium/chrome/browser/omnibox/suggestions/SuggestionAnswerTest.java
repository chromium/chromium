// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import android.support.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;

@RunWith(BaseJUnit4ClassRunner.class)
public class SuggestionAnswerTest {
    @Test
    @SmallTest
    public void testMalformedJsonReturnsNull() {
        String json = "} malformed json {";
        SuggestionAnswer answer = SuggestionAnswer.parseAnswerContents(json);
        Assert.assertNull(answer);
    }

    @Test
    @SmallTest
    public void testEmpyJsonReturnsNull() {
        String json = "";
        SuggestionAnswer answer = SuggestionAnswer.parseAnswerContents(json);
        Assert.assertNull(answer);
    }

    @Test
    @SmallTest
    public void testOneLineReturnsNull() {
        String json = "{ 'l': ["
                + "  { 'il': { 't': [{ 't': 'text', 'tt': 8 }] } }, "
                + "] }";
        SuggestionAnswer answer = SuggestionAnswer.parseAnswerContents(json);
        Assert.assertNull(answer);
    }

    @Test
    @SmallTest
    public void testTwoLinesDoesntReturnNull() {
        String json = "{ 'l': ["
                + "  { 'il': { 't': [{ 't': 'text', 'tt': 8 }] } }, "
                + "  { 'il': { 't': [{ 't': 'other text', 'tt': 5 }] } }"
                + "] }";
        SuggestionAnswer answer = SuggestionAnswer.parseAnswerContents(json);
        Assert.assertNotNull(answer);
    }

    @Test
    @SmallTest
    public void testThreeLinesReturnsNull() {
        String json = "{ 'l': ["
                + "  { 'il': { 't': [{ 't': 'text', 'tt': 8 }] } }, "
                + "  { 'il': { 't': [{ 't': 'other text', 'tt': 5 }] } }"
                + "  { 'il': { 't': [{ 't': 'yet more text', 'tt': 13 }] } }"
                + "] }";
        SuggestionAnswer answer = SuggestionAnswer.parseAnswerContents(json);
        Assert.assertNull(answer);
    }

    @Test
    @SmallTest
    public void testFiveLinesReturnsNull() {
        String json = "{ 'l': ["
                + "  { 'il': { 't': [{ 't': 'line 1', 'tt': 0 }] } }, "
                + "  { 'il': { 't': [{ 't': 'line 2', 'tt': 5 }] } }"
                + "  { 'il': { 't': [{ 't': 'line 3', 'tt': 13 }] } }"
                + "  { 'il': { 't': [{ 't': 'line 4', 'tt': 14 }] } }"
                + "  { 'il': { 't': [{ 't': 'line 5', 'tt': 5 }] } }"
                + "] }";
        SuggestionAnswer answer = SuggestionAnswer.parseAnswerContents(json);
        Assert.assertNull(answer);
    }

    @Test
    @SmallTest
    public void testPropertyPresence() {
        String json = "{ 'l': ["
                + "  { 'il': { 't': [{ 't': 'text', 'tt': 8 }, { 't': 'moar', 'tt': 0 }], "
                + "            'i': { 'd': 'http://example.com/foo.jpg' } } }, "
                + "  { 'il': { 't': [{ 't': 'other text', 'tt': 5 }], "
                + "            'at': { 't': 'slatfotf', 'tt': 42 }, "
                + "            'st': { 't': 'oh hi, Mark', 'tt': 7666 } } } "
                + "] }";
        SuggestionAnswer answer = SuggestionAnswer.parseAnswerContents(json);

        SuggestionAnswer.ImageLine firstLine = answer.getFirstLine();
        Assert.assertEquals(2, firstLine.getTextFields().size());
        Assert.assertFalse(firstLine.hasAdditionalText());
        Assert.assertFalse(firstLine.hasStatusText());
        Assert.assertTrue(firstLine.hasImage());

        SuggestionAnswer.ImageLine secondLine = answer.getSecondLine();
        Assert.assertEquals(1, secondLine.getTextFields().size());
        Assert.assertTrue(secondLine.hasAdditionalText());
        Assert.assertTrue(secondLine.hasStatusText());
        Assert.assertFalse(secondLine.hasImage());
    }

    @Test
    @SmallTest
    public void testContents() {
        String json = "{ 'l': ["
                + "  { 'il': { 't': [{ 't': 'text', 'tt': 8 }, { 't': 'moar', 'tt': 0 }], "
                + "            'at': { 't': 'hi there', 'tt': 7 } } }, "
                + "  { 'il': { 't': [{ 't': 'ftw', 'tt': 6006 }], "
                + "            'st': { 't': 'shop S-Mart', 'tt': 666 }, "
                + "            'i': { 'd': '//Npb24gZnJvbSBvdGhlciBhbmltYWxzLCB3aGlj' } } } "
                + "] }";
        SuggestionAnswer answer = SuggestionAnswer.parseAnswerContents(json);

        SuggestionAnswer.ImageLine firstLine = answer.getFirstLine();
        Assert.assertEquals("text", firstLine.getTextFields().get(0).getText());
        Assert.assertEquals(8, firstLine.getTextFields().get(0).getType());
        Assert.assertEquals("moar", firstLine.getTextFields().get(1).getText());
        Assert.assertEquals(0, firstLine.getTextFields().get(1).getType());
        Assert.assertEquals("hi there", firstLine.getAdditionalText().getText());
        Assert.assertEquals(7, firstLine.getAdditionalText().getType());

        SuggestionAnswer.ImageLine secondLine = answer.getSecondLine();
        Assert.assertEquals("ftw", secondLine.getTextFields().get(0).getText());
        Assert.assertEquals(6006, secondLine.getTextFields().get(0).getType());
        Assert.assertEquals("shop S-Mart", secondLine.getStatusText().getText());
        Assert.assertEquals(666, secondLine.getStatusText().getType());
        Assert.assertEquals(
                "https://Npb24gZnJvbSBvdGhlciBhbmltYWxzLCB3aGlj", secondLine.getImage());
    }
}

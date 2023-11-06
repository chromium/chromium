// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextualsearch;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.app.Activity;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.ui.touch_selection.SelectionEventType;

/**
 * Tests a few methods of the ContextualSearchSelectionController. <ul>
 * <li>That the ContextualSearchSelectionController#handleSelectionmethod supports usages
 * from Smart Text Selection and from normal user driven selection.</li>
 * <li>That the ContextualSearchSelectionController#isSelectionPartOfUrl handles all the different
 * URL cases.</li>
 * </ul>
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(
        shadows = {
            ContextualSearchSelectionControllerTest.ShadowContextualSearchSelectionController.class
        })
public final class ContextualSearchSelectionControllerTest {
    private static final String USER_SELECTION = "user selection";

    /** Stores the selection set by ContextualSearchSelectionController#handleSelection. */
    private static String sSelectionSetByHandleSelection;

    /** The instance under test, which is also shadowed. */
    private ContextualSearchSelectionController mSelectionControllerUnderTest;

    @Before
    public void setUp() throws Exception {
        Activity activity = Robolectric.buildActivity(Activity.class).setup().get();
        mSelectionControllerUnderTest =
                new ContextualSearchSelectionController(activity, null, null);
        mSelectionControllerUnderTest.setSelectedText(USER_SELECTION);
        sSelectionSetByHandleSelection = null;
    }

    /**
     * Shadows the class that we are testing: ContextualSearchSelectionController.
     * This allows us to stub out some methods without needing to make mocks or stubs for the things
     * those methods call.
     */
    @Implements(ContextualSearchSelectionController.class)
    public static class ShadowContextualSearchSelectionController {
        public ShadowContextualSearchSelectionController() {}

        /**
         * Implements #handleSelection by remembering the selection param, which can later be
         * retrieved by calling #getSelectionSetByHandleSelection
         * @param selection The text that was selected.
         * @param type The type of selection made by the user.
         */
        @Implementation
        protected void handleSelection(
                String selection, @ContextualSearchSelectionController.SelectionType int type) {
            sSelectionSetByHandleSelection = selection;
        }

        /** Returns the selection that was set by the call to #handleSelection. */
        static String getSelectionSetByHandleSelection() {
            return sSelectionSetByHandleSelection;
        }
    }

    /**
     * Returns the string that the selection controller has sent to the Contextual Search Manager
     * in #handleSelection.
     */
    private String getSelectionSetByHandleSelection() {
        return ShadowContextualSearchSelectionController.getSelectionSetByHandleSelection();
    }

    // ============================================================================================
    // Selection manipulation with and without Smart Text Selection
    // ============================================================================================

    @Test
    @Feature({"ContextualSearchSelectionController"})
    public void testUserDrivenSelectionSequence() {
        mSelectionControllerUnderTest.handleSelectionEvent(
                SelectionEventType.SELECTION_HANDLE_DRAG_STARTED, 0f, 0f);
        final String unexpectedSelectionSent =
                "User flow for ContextualSearchSelectionController#handleSelectionEvent sent a "
                        + "selection to the Manager that was unexpected.";
        Assert.assertNull(unexpectedSelectionSent, getSelectionSetByHandleSelection());
        mSelectionControllerUnderTest.handleSelectionEvent(
                SelectionEventType.SELECTION_HANDLES_MOVED, 0f, 0f);
        Assert.assertNull(unexpectedSelectionSent, getSelectionSetByHandleSelection());
        mSelectionControllerUnderTest.handleSelectionEvent(
                SelectionEventType.SELECTION_HANDLE_DRAG_STOPPED, 0f, 0f);
        Assert.assertEquals(
                "User flow for ContextualSearchSelectionController#handleSelectionEvent "
                        + "sent an unexpected selection to the Manager. Maybe something broke "
                        + "longpress selection modification for Contextual Search.",
                USER_SELECTION,
                getSelectionSetByHandleSelection());
    }

    @Test
    @Feature({"ContextualSearchSelectionController"})
    public void testSmartTextSelectionIntegration() {
        // When Smart Text Selection is active a SELECTION_HANDLES_MOVED event is sent without a
        // SELECTION_HANDLE_DRAG_STARTED event.
        mSelectionControllerUnderTest.handleSelectionEvent(
                SelectionEventType.SELECTION_HANDLES_MOVED, 0f, 0f);
        // Make sure we did not establish any selection.
        Assert.assertNull(
                "Smart Text Selection interaction with Contextual Search "
                        + "through the ContextualSearchSelectionController#handleSelectionEvent "
                        + "sent a selection to the Manager that was unexpected. Smart Text "
                        + "Selection with the intelligent Long-press gesture may be broken.",
                getSelectionSetByHandleSelection());
    }

    // ============================================================================================
    // isSelectionPartOfUrl test cases
    // ============================================================================================

    @Test
    @Feature({"ContextualSearchSelectionController"})
    public void testBasicUrlExtraction() {
        String testSentence = "A sentence containing a http://www.example.com valid url";

        // Select "com".
        assertEquals("com", testSentence.subSequence(43, 46));
        assertTrue(ContextualSearchSelectionController.isSelectionPartOfUrl(testSentence, 43, 46));

        // Select "http".
        assertEquals("http", testSentence.subSequence(24, 28));
        assertTrue(ContextualSearchSelectionController.isSelectionPartOfUrl(testSentence, 24, 28));

        // Select "www".
        assertEquals("www", testSentence.subSequence(31, 34));
        assertTrue(ContextualSearchSelectionController.isSelectionPartOfUrl(testSentence, 31, 34));

        // Select "example".
        assertEquals("example", testSentence.subSequence(35, 42));
        assertTrue(ContextualSearchSelectionController.isSelectionPartOfUrl(testSentence, 35, 42));

        // Select "containing".
        assertEquals("containing", testSentence.subSequence(11, 21));
        assertFalse(ContextualSearchSelectionController.isSelectionPartOfUrl(testSentence, 11, 21));

        // Select "url".
        assertEquals("url", testSentence.subSequence(53, 56));
        assertFalse(ContextualSearchSelectionController.isSelectionPartOfUrl(testSentence, 53, 56));
    }

    @Test
    @Feature({"ContextualSearchSelectionController"})
    public void testUrlWithNoScheme() {
        String testSentence = "This is a sentence about example.com/foo#bar.";

        // Select "foo".
        assertEquals("foo", testSentence.subSequence(37, 40));
        assertFalse(ContextualSearchSelectionController.isSelectionPartOfUrl(testSentence, 37, 40));
    }

    @Test
    @Feature({"ContextualSearchSelectionController"})
    public void testUrlWithAnchor() {
        String testSentence = "This is a sentence about http://example.com/foo#bar.";

        // Select "foo".
        assertEquals("foo", testSentence.subSequence(44, 47));
        assertTrue(ContextualSearchSelectionController.isSelectionPartOfUrl(testSentence, 44, 47));

        // Select "bar".
        assertEquals("bar", testSentence.subSequence(48, 51));
        assertTrue(ContextualSearchSelectionController.isSelectionPartOfUrl(testSentence, 48, 51));

        // Select "This".
        assertEquals("This", testSentence.subSequence(0, 4));
        assertFalse(ContextualSearchSelectionController.isSelectionPartOfUrl(testSentence, 0, 4));
    }

    @Test
    @Feature({"ContextualSearchSelectionController"})
    public void testUrlSurroundedByParens() {
        String testSentence = "This is another sentence (http://example.com).";

        // Select "com".
        assertEquals("com", testSentence.subSequence(41, 44));
        assertTrue(ContextualSearchSelectionController.isSelectionPartOfUrl(testSentence, 41, 44));

        // Select "(".
        assertEquals("(", testSentence.subSequence(25, 26));
        assertFalse(ContextualSearchSelectionController.isSelectionPartOfUrl(testSentence, 25, 26));
    }

    @Test
    @Feature({"ContextualSearchSelectionController"})
    public void testUrlWithHttpsSchema() {
        String testSentence = "https://supersecure.net.";

        // Select "supersecure".
        assertEquals("supersecure", testSentence.subSequence(8, 19));
        assertTrue(ContextualSearchSelectionController.isSelectionPartOfUrl(testSentence, 8, 19));
    }

    @Test
    @Feature({"ContextualSearchSelectionController"})
    public void testUrlWithFileSchema() {
        String testSentence = "file://some_text_file.txt";

        // Select "text".
        assertEquals("text", testSentence.subSequence(12, 16));
        assertTrue(ContextualSearchSelectionController.isSelectionPartOfUrl(testSentence, 12, 16));
    }

    @Test
    @Feature({"ContextualSearchSelectionController"})
    public void testUrlWithFtpSchema() {
        String testSentence = "ftp://some_text_file.txt";

        // Select "text".
        assertEquals("text", testSentence.subSequence(11, 15));
        assertTrue(ContextualSearchSelectionController.isSelectionPartOfUrl(testSentence, 11, 15));
    }

    @Test
    @Feature({"ContextualSearchSelectionController"})
    public void testUrlWithSshSchema() {
        String testSentence = "ssh://some_text_file.txt";

        // Select "text".
        assertEquals("text", testSentence.subSequence(11, 15));
        assertTrue(ContextualSearchSelectionController.isSelectionPartOfUrl(testSentence, 11, 15));
    }

    @Test
    @Feature({"ContextualSearchSelectionController"})
    public void testUrlWithPortAndQuery() {
        String testSentence = "http://website.com:8080/html?query";

        // Select "8080".
        assertEquals("8080", testSentence.subSequence(19, 23));
        assertTrue(ContextualSearchSelectionController.isSelectionPartOfUrl(testSentence, 19, 23));

        // Select "query".
        assertEquals("query", testSentence.subSequence(29, 34));
        assertTrue(ContextualSearchSelectionController.isSelectionPartOfUrl(testSentence, 29, 34));
    }

    @Test
    @Feature({"ContextualSearchSelectionController"})
    public void testIpAddress() {
        String testSentence = "127.0.0.1";

        // Select "0".
        assertEquals("0", testSentence.subSequence(4, 5));
        assertFalse(ContextualSearchSelectionController.isSelectionPartOfUrl(testSentence, 4, 5));
    }

    @Test
    @Feature({"ContextualSearchSelectionController"})
    public void testEmailAddress() {
        String testSentence = "Please email me at username@domain.com or call...";

        // Select "username".
        assertEquals("username", testSentence.subSequence(19, 27));
        assertFalse(ContextualSearchSelectionController.isSelectionPartOfUrl(testSentence, 19, 27));
    }

    @Test
    @Feature({"ContextualSearchSelectionController"})
    public void testUrlLikeSyntax() {
        String testSentence = "Example sentence with no URLs, but.weird.syntax";

        // Select "syntax".
        assertEquals("weird", testSentence.subSequence(35, 40));
        assertFalse(ContextualSearchSelectionController.isSelectionPartOfUrl(testSentence, 35, 40));
    }
}

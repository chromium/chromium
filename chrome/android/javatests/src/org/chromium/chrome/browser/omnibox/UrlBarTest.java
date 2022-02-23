// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import static org.hamcrest.core.IsEqual.equalTo;

import android.support.test.InstrumentationRegistry;
import android.text.TextUtils;
import android.view.KeyEvent;
import android.view.View;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.ClassRule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mockito;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.test.params.ParameterAnnotations.UseRunnerDelegate;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.FlakyTest;
import org.chromium.base.test.util.Restriction;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.OmniboxTestUtils;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.content_public.common.ContentUrlConstants;
import org.chromium.ui.test.util.UiRestriction;

import java.util.concurrent.TimeoutException;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.atomic.AtomicReference;

/**
 * Tests for the URL bar UI component.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@Batch(Batch.PER_CLASS)
public class UrlBarTest {
    @ClassRule
    public static ChromeTabbedActivityTestRule sActivityTestRule =
            new ChromeTabbedActivityTestRule();

    private UrlBar mUrlBar;
    private OmniboxTestUtils mOmnibox;

    @BeforeClass
    public static void setUpClass() throws Exception {
        sActivityTestRule.startMainActivityWithURL(ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL);
        // Needed to make sure all the necessary ChromeFeatureFlags are populated.
        sActivityTestRule.waitForDeferredStartup();
    }

    @Before
    public void setUpTest() throws Exception {
        mOmnibox = new OmniboxTestUtils(sActivityTestRule.getActivity());
        mUrlBar = sActivityTestRule.getActivity().findViewById(R.id.url_bar);
        sActivityTestRule.loadUrl(ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL);
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        mOmnibox.disableLiveAutocompletion();
        mOmnibox.requestFocus();
    }

    private static class AutocompleteState {
        public final boolean hasAutocomplete;
        public final String textWithoutAutocomplete;
        public final String textWithAutocomplete;

        public AutocompleteState(
                boolean hasAutocomplete, String textWithoutAutocomplete,
                String textWithAutocomplete) {
            this.hasAutocomplete = hasAutocomplete;
            this.textWithoutAutocomplete = textWithoutAutocomplete;
            this.textWithAutocomplete = textWithAutocomplete;
        }
    }

    private AutocompleteState getAutocompleteState(final Runnable action) {
        final AtomicBoolean hasAutocomplete = new AtomicBoolean();
        final AtomicReference<String> textWithoutAutocomplete = new AtomicReference<String>();
        final AtomicReference<String> textWithAutocomplete = new AtomicReference<String>();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            if (action != null) action.run();
            textWithoutAutocomplete.set(mUrlBar.getTextWithoutAutocomplete());
            textWithAutocomplete.set(mUrlBar.getTextWithAutocomplete());
            hasAutocomplete.set(mUrlBar.hasAutocomplete());
        });

        return new AutocompleteState(
                hasAutocomplete.get(), textWithoutAutocomplete.get(), textWithAutocomplete.get());
    }

    private AutocompleteState setSelection(final int selectionStart, final int selectionEnd) {
        return getAutocompleteState(() -> mUrlBar.setSelection(selectionStart, selectionEnd));
    }

    private void setTextAndVerifyTextDirection(String text, int expectedDirection)
            throws TimeoutException {
        CallbackHelper directionCallback = new CallbackHelper();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mUrlBar.setUrlDirectionListener((direction) -> {
                if (direction == expectedDirection) directionCallback.notifyCalled();
            });
        });
        mOmnibox.setText(text);
        directionCallback.waitForFirst(
                "Direction never reached expected direction: " + expectedDirection);
        assertUrlDirection(expectedDirection);
        TestThreadUtils.runOnUiThreadBlocking(() -> mUrlBar.setUrlDirectionListener(null));
    }

    private void assertUrlDirection(int expectedDirection) {
        int actualDirection =
                TestThreadUtils.runOnUiThreadBlockingNoException(() -> mUrlBar.getUrlDirection());
        Assert.assertEquals(expectedDirection, actualDirection);
    }

    @Test
    @SmallTest
    @FlakyTest(message = "crbug.com/1291515")
    public void testRefocusing() {
        for (int i = 0; i < 5; i++) {
            mOmnibox.requestFocus();
            mOmnibox.clearFocus();
        }
    }

    @Test
    @SmallTest
    @FlakyTest(message = "https://crbug.com/1291159")
    public void testAutocompleteUpdatedOnSetText() {
        // Verify that setting a new string will clear the autocomplete.
        mOmnibox.setText("test");
        mOmnibox.setAutocompleteText("ing is fun");

        // Replace part of the non-autocomplete text
        mOmnibox.setText("test");
        mOmnibox.setAutocompleteText("ing is fun");
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mUrlBar.setText(mUrlBar.getText().replace(1, 2, "a")); });
        mOmnibox.checkText(equalTo("tast"), null);

        // Replace part of the autocomplete text.
        mOmnibox.setText("test");
        mOmnibox.setAutocompleteText("ing is fun");
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mUrlBar.setText(mUrlBar.getText().replace(8, 10, "no")); });
        mOmnibox.checkText(equalTo("test"), null);
    }

    private void verifySelectionState(String text, String inlineAutocomplete, int selectionStart,
            int selectionEnd, boolean expectedHasAutocomplete,
            String expectedTextWithoutAutocomplete, String expectedTextWithAutocomplete,
            boolean expectedPreventInline, String expectedRequestedAutocompleteText)
            throws TimeoutException {
        mOmnibox.setText(text);
        mOmnibox.setAutocompleteText(inlineAutocomplete);

        final CallbackHelper autocompleteHelper = new CallbackHelper();
        final AtomicReference<String> requestedAutocompleteText = new AtomicReference<String>();
        final AtomicBoolean didPreventInlineAutocomplete = new AtomicBoolean();
        mUrlBar.setUrlTextChangeListener((textWithoutAutocomplete, textWithAutocomplete) -> {
            autocompleteHelper.notifyCalled();
            requestedAutocompleteText.set(textWithoutAutocomplete);
            didPreventInlineAutocomplete.set(!mUrlBar.shouldAutocomplete());
            mUrlBar.setUrlTextChangeListener(null);
        });

        AutocompleteState state = setSelection(selectionStart, selectionEnd);
        Assert.assertEquals("Has autocomplete", expectedHasAutocomplete, state.hasAutocomplete);
        Assert.assertEquals("Text w/o Autocomplete", expectedTextWithoutAutocomplete,
                state.textWithoutAutocomplete);
        Assert.assertEquals(
                "Text w/ Autocomplete", expectedTextWithAutocomplete, state.textWithAutocomplete);

        autocompleteHelper.waitForCallback(0);
        Assert.assertEquals("Prevent inline autocomplete", expectedPreventInline,
                didPreventInlineAutocomplete.get());
        Assert.assertEquals("Requested autocomplete text", expectedRequestedAutocompleteText,
                requestedAutocompleteText.get());
    }

    @Test
    @SmallTest
    @FlakyTest(message = "https://crbug.com/1291968")
    public void testAutocompleteUpdatedOnSelection() throws TimeoutException {
        // Verify that setting a selection before the autocomplete clears it.
        verifySelectionState("test", "ing is fun", 1, 1, false, "test", "test", true, "test");

        // Verify that setting a selection range before the autocomplete clears it.
        verifySelectionState("test", "ing is fun", 0, 4, false, "test", "test", true, "test");

        // Verify that setting a selection range that covers a portion of the non-autocomplete
        // and autocomplete text does not delete the autocomplete text.
        verifySelectionState("test", "ing is fun", 2, 5,
                false, "testing is fun", "testing is fun", true, "testing is fun");

        // Verify that setting a selection range that over the entire string does not delete
        // the autocomplete text.
        verifySelectionState("test", "ing is fun", 0, 14,
                false, "testing is fun", "testing is fun", true, "testing is fun");

        // Note: with new model touching the beginning of the autocomplete text is a no-op.
        // Verify that setting a selection at the end of the text does not delete the
        // autocomplete text.
        verifySelectionState("test", "ing is fun", 14, 14, false, "testing is fun",
                "testing is fun", true, "testing is fun");

        // Verify that setting a selection in the middle of the autocomplete text does not delete
        // the autocomplete text.
        verifySelectionState("test", "ing is fun", 9, 9,
                false, "testing is fun", "testing is fun", true, "testing is fun");

        // Verify that setting a selection range in the middle of the autocomplete text does not
        // delete the autocomplete text.
        verifySelectionState("test", "ing is fun", 8, 11,
                false, "testing is fun", "testing is fun", true, "testing is fun");

        // Select autocomplete text. As we do not expect the suggestions to be refreshed, we test
        // this slightly differently than the other cases.
        mOmnibox.setText("test");
        mOmnibox.setAutocompleteText("ing is fun");
        TestThreadUtils.runOnUiThreadBlocking(() -> mUrlBar.setSelection(4, 14));
        mOmnibox.checkText(equalTo("testing is fun"), null);
    }

    /**
     * Ensure that we send cursor position with autocomplete requests.
     *
     * When reading this test, it helps to remember that autocomplete requests are not sent
     * with the user simply moves the cursor.  They're only sent on text modifications.
     */
    @Test
    @SmallTest
    @FlakyTest(message = "https://crbug.com/1291968")
    public void testSendCursorPosition() throws TimeoutException {
        final CallbackHelper autocompleteHelper = new CallbackHelper();
        final AtomicInteger cursorPositionUsed = new AtomicInteger();
        mUrlBar.setUrlTextChangeListener((textWithoutAutocomplete, textWithAutocomplete) -> {
            int cursorPosition = mUrlBar.getSelectionEnd() == mUrlBar.getSelectionStart()
                    ? mUrlBar.getSelectionStart()
                    : -1;
            cursorPositionUsed.set(cursorPosition);
            autocompleteHelper.notifyCalled();
        });

        // Add "a" to the omnibox and leave the cursor at the end of the new
        // text.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mUrlBar.getInputConnection().commitText("a", 1); });
        autocompleteHelper.waitForCallback(0);
        // omnmibox text: a|
        Assert.assertEquals(1, cursorPositionUsed.get());

        // Append "cd" to the omnibox and leave the cursor at the end of the new
        // text.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mUrlBar.getInputConnection().commitText("cd", 1); });
        autocompleteHelper.waitForCallback(1);
        // omnmibox text: acd|
        Assert.assertEquals(3, cursorPositionUsed.get());

        // Move the cursor.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mUrlBar.getInputConnection().setSelection(1, 1); });
        // omnmibox text: a|cd
        // Moving the cursor shouldn't have caused a new call.
        Assert.assertEquals(2, autocompleteHelper.getCallCount());
        // The cursor position used on the last call should be the old position.
        Assert.assertEquals(3, cursorPositionUsed.get());

        // Insert "b" at the current cursor position and leave the cursor at
        // the end of the new text.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mUrlBar.getInputConnection().commitText("b", 1); });
        autocompleteHelper.waitForCallback(2);
        // omnmibox text: ab|cd
        Assert.assertEquals(2, cursorPositionUsed.get());

        // Delete the character before the cursor.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mUrlBar.getInputConnection().deleteSurroundingText(1, 0); });
        autocompleteHelper.waitForCallback(3);
        // omnmibox text: a|cd
        Assert.assertEquals(1, cursorPositionUsed.get());

        // Delete the character before the cursor (again).
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mUrlBar.getInputConnection().deleteSurroundingText(1, 0); });
        autocompleteHelper.waitForCallback(4);
        // omnmibox text: |cd
        Assert.assertEquals(0, cursorPositionUsed.get());

        mUrlBar.setUrlTextChangeListener(null);
    }

    /**
     * Ensure that we allow inline autocomplete when the text gets shorter but is not an explicit
     * delete action by the user.
     *
     * If you focus the omnibox and there is the selected text "[about:blank]", then typing new text
     * should clear that entirely and allow autocomplete on the newly entered text.
     *
     * If we assume deletes happen any time the text gets shorter, then this would be prevented.
     */
    @Test
    @SmallTest
    public void testAutocompleteAllowedWhenReplacingText() throws TimeoutException {
        final String textToBeEntered = "c";

        final CallbackHelper autocompleteHelper = new CallbackHelper();
        final AtomicBoolean didPreventInlineAutocomplete = new AtomicBoolean();
        mUrlBar.setUrlTextChangeListener((textWithoutAutocomplete, textWithAutocomplete) -> {
            if (!TextUtils.equals(textToBeEntered, mUrlBar.getTextWithoutAutocomplete())) return;
            didPreventInlineAutocomplete.set(!mUrlBar.shouldAutocomplete());
            autocompleteHelper.notifyCalled();
            mUrlBar.setUrlTextChangeListener(null);
        });

        mOmnibox.typeText(textToBeEntered, false);

        autocompleteHelper.waitForCallback(0);
        Assert.assertFalse(
                "Inline autocomplete incorrectly prevented.", didPreventInlineAutocomplete.get());
    }

    /**
     * Ensure that if the user deletes just the inlined autocomplete text that the suggestions are
     * regenerated.
     */
    @Test
    @SmallTest
    public void testSuggestionsUpdatedWhenDeletingInlineAutocomplete() throws TimeoutException {
        mOmnibox.setText("test");
        mOmnibox.setAutocompleteText("ing");

        final CallbackHelper autocompleteHelper = new CallbackHelper();
        final AtomicBoolean didPreventInlineAutocomplete = new AtomicBoolean();
        mUrlBar.setUrlTextChangeListener((textWithoutAutocomplete, textWithAutocomplete) -> {
            if (!TextUtils.equals("test", mUrlBar.getTextWithoutAutocomplete())) return;
            didPreventInlineAutocomplete.set(!mUrlBar.shouldAutocomplete());
            autocompleteHelper.notifyCalled();
            mUrlBar.setUrlTextChangeListener(null);
        });

        mOmnibox.sendKey(KeyEvent.KEYCODE_DEL);

        mOmnibox.checkText(equalTo("test"), null);

        autocompleteHelper.waitForCallback(0);
        Assert.assertTrue("Inline autocomplete incorrectly allowed after delete.",
                didPreventInlineAutocomplete.get());
    }

    @Test
    @SmallTest
    public void testSelectionChangesIgnoredInBatchMode() {
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.SPANNABLE_INLINE_AUTOCOMPLETE)) {
            // Note: with the new model, we remove autocomplete text at the beginning of a batch
            // edit and add it at the end of a batch edit.
            return;
        }

        mOmnibox.setText("test");
        mOmnibox.setAutocompleteText("ing is fun");

        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mUrlBar.getInputConnection().beginBatchEdit(); });
        // Ensure the autocomplete is not modified if in batch mode.
        TestThreadUtils.runOnUiThreadBlocking(() -> mUrlBar.setSelection(1, 1));
        mOmnibox.checkText(equalTo("test"), equalTo("testing is fun"));
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mUrlBar.getInputConnection().endBatchEdit(); });
        mOmnibox.checkText(equalTo("test"), null);
    }

    @Test
    @SmallTest
    @FlakyTest(message = "https://crbug.com/1291159")
    public void testBatchModeChangesTriggerCorrectSuggestions() {
        final AtomicReference<String> requestedAutocompleteText = new AtomicReference<String>();
        mOmnibox.setText("test");
        mOmnibox.setAutocompleteText("ing is fun");

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mUrlBar.getInputConnection().beginBatchEdit();
            mUrlBar.getInputConnection().commitText("y", 1);
            mUrlBar.getInputConnection().endBatchEdit();
        });

        mOmnibox.checkText(equalTo("testy"), null);
    }

    @Test
    @SmallTest
    @FlakyTest(message = "https://crbug.com/1291159")
    public void testAutocompleteCorrectlyPerservedOnBatchMode() {
        // Valid case (cursor at the end of text, single character, matches previous autocomplete).
        mOmnibox.setText("g");
        mOmnibox.setAutocompleteText("oogle.com");
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mUrlBar.getInputConnection().commitText("o", 1); });
        mOmnibox.checkText(equalTo("go"), equalTo("google.com"));

        // Invalid case (cursor not at the end of the text)
        mOmnibox.setText("g");
        mOmnibox.setAutocompleteText("oogle.com");
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mUrlBar.getInputConnection().beginBatchEdit();
            mUrlBar.getInputConnection().commitText("o", 1);
            mUrlBar.getInputConnection().setSelection(0, 0);
            mUrlBar.getInputConnection().endBatchEdit();
        });
        mOmnibox.checkText(equalTo("go"), null);

        // Invalid case (next character did not match previous autocomplete)
        mOmnibox.setText("g");
        mOmnibox.setAutocompleteText("oogle.com");
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mUrlBar.getInputConnection().commitText("a", 1); });
        mOmnibox.checkText(equalTo("ga"), null);

        // Multiple characters entered instead of 1.
        mOmnibox.setText("g");
        mOmnibox.setAutocompleteText("oogle.com");
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mUrlBar.getInputConnection().commitText("oogl", 1); });
        mOmnibox.checkText(equalTo("googl"), equalTo("google.com"));
    }

    @Test
    @SmallTest
    @FlakyTest(message = "https://crbug.com/1291159")
    public void testAutocompleteSpanClearedOnNonMatchingCommitText() {
        mOmnibox.setText("a");
        mOmnibox.setAutocompleteText("mazon.com");

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mUrlBar.getInputConnection().beginBatchEdit();
            mUrlBar.getInputConnection().commitText("l", 1);
            mUrlBar.getInputConnection().setComposingText("", 1);
            mUrlBar.getInputConnection().endBatchEdit();
        });

        mOmnibox.checkText(equalTo("al"), null);
    }

    @Test
    @SmallTest
    public void testAutocompleteClearedOnComposition() {
        mOmnibox.setText("test");
        mOmnibox.setAutocompleteText("ing is fun");

        mOmnibox.setComposingText("ing compose", 4, 4);
        mOmnibox.checkComposingText(equalTo("testing compose"), 4, 15);
    }

    @Test
    @SmallTest
    public void testDelayedCompositionCorrectedWithAutocomplete() {
        // Test with a single autocomplete
        mOmnibox.setText("chrome://f");
        mOmnibox.setAutocompleteText("lags");

        mOmnibox.setComposingText("f", 13, 14);
        mOmnibox.checkComposingText(equalTo("chrome://ff"), 10, 11);

        // Test with > 1 characters in composition.
        mOmnibox.setText("chrome://fl");
        mOmnibox.setAutocompleteText("ags");

        mOmnibox.setComposingText("fl", 12, 14);
        mOmnibox.checkComposingText(equalTo("chrome://flfl"), 11, 13);

        // Test with non-matching composition.  Should just append to the URL text.
        mOmnibox.setText("chrome://f");
        mOmnibox.setAutocompleteText("lags");

        mOmnibox.setComposingText("g", 13, 14);
        mOmnibox.checkComposingText(equalTo("chrome://fg"), 10, 11);

        // Test with composition text that matches the entire text w/o autocomplete.
        mOmnibox.setText("chrome://f");
        mOmnibox.setAutocompleteText("lags");

        mOmnibox.setComposingText("chrome://f", 13, 14);
        mOmnibox.checkComposingText(equalTo("chrome://fchrome://f"), 10, 20);

        // Test with composition text longer than the URL text.
        // Shouldn't crash and should just append text.
        mOmnibox.setText("chrome://f");
        mOmnibox.setAutocompleteText("lags");

        mOmnibox.setComposingText("blahblahblah", 13, 14);
        mOmnibox.checkComposingText(equalTo("chrome://fblahblahblah"), 10, 22);
    }

    @Test
    @SmallTest
    public void testUrlTextChangeListener() {
        UrlBar.UrlTextChangeListener listener = Mockito.mock(UrlBar.UrlTextChangeListener.class);
        mUrlBar.setUrlTextChangeListener(listener);

        mOmnibox.setText("onomatop");
        Mockito.verify(listener).onTextChanged("onomatop", "onomatop");

        // Setting autocomplete does not send a change update.
        mOmnibox.setAutocompleteText("oeia");

        mOmnibox.setText("");
        Mockito.verify(listener).onTextChanged("", "");
    }

    @Test
    @SmallTest
    public void testSetAutocompleteText_ShrinkingText() {
        mOmnibox.setText("test");
        mOmnibox.setAutocompleteText("ing is awesome");
        mOmnibox.setAutocompleteText("ing is hard");
        mOmnibox.setAutocompleteText("ingz");
        mOmnibox.checkText(equalTo("test"), equalTo("testingz"), 4, 8);
    }

    @Test
    @SmallTest
    public void testSetAutocompleteText_GrowingText() {
        mOmnibox.setText("test");
        mOmnibox.setAutocompleteText("ingz");
        mOmnibox.setAutocompleteText("ing is hard");
        mOmnibox.setAutocompleteText("ing is awesome");
        mOmnibox.checkText(equalTo("test"), equalTo("testing is awesome"), 4, 18);
    }

    @Test
    @SmallTest
    public void testSetAutocompleteText_DuplicateText() {
        mOmnibox.setText("test");
        mOmnibox.setAutocompleteText("ingz");
        mOmnibox.setAutocompleteText("ingz");
        mOmnibox.setAutocompleteText("ingz");
        mOmnibox.checkText(equalTo("test"), equalTo("testingz"), 4, 8);
    }

    @Test
    @SmallTest
    public void testUrlDirection() throws TimeoutException {
        setTextAndVerifyTextDirection("ل", View.LAYOUT_DIRECTION_RTL);
        setTextAndVerifyTextDirection("a", View.LAYOUT_DIRECTION_LTR);
        setTextAndVerifyTextDirection("للك", View.LAYOUT_DIRECTION_RTL);
        setTextAndVerifyTextDirection("f", View.LAYOUT_DIRECTION_LTR);
    }

    @Test
    @SmallTest
    public void testAutocompleteUpdatedOnDefocus() throws InterruptedException {
        sActivityTestRule.loadUrl(ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL);
        mOmnibox.requestFocus();
        mOmnibox.typeText("test", false);
        mOmnibox.clearFocus();
        mOmnibox.checkText(equalTo(ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL), null);
    }

    @Test
    @SmallTest
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE})
    public void testDarkThemeColor() throws Throwable {
        // Execute HTML from within the omnibox and observe the style change.
        mOmnibox.typeText(UrlUtils.encodeHtmlDataUri(
                                  "<html><meta name=\"theme-color\" content=\"#000000\" /></html>"),
                true);

        CriteriaHelper.pollUiThread(() -> {
            final int expectedTextColor =
                    ApiCompatibilityUtils.getColor(sActivityTestRule.getActivity().getResources(),
                            R.color.branded_url_text_on_dark_bg);
            Criteria.checkThat(mUrlBar.getCurrentTextColor(), equalTo(expectedTextColor));
        });
    }
}

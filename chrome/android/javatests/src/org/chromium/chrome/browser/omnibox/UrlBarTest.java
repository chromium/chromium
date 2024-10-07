// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import static org.hamcrest.core.IsEqual.equalTo;
import static org.junit.Assume.assumeFalse;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoInteractions;

import android.text.TextUtils;
import android.view.KeyEvent;
import android.view.View;
import android.view.inputmethod.InputConnection;

import androidx.test.InstrumentationRegistry;
import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.ClassRule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mockito;

import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.params.ParameterAnnotations.UseRunnerDelegate;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.RequiresRestart;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.util.OmniboxTestUtils;
import org.chromium.components.omnibox.OmniboxFeatures;
import org.chromium.content_public.common.ContentUrlConstants;
import org.chromium.ui.base.Clipboard;

import java.util.List;
import java.util.Optional;
import java.util.concurrent.TimeoutException;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.atomic.AtomicReference;

/**
 * Tests for the URL bar UI component.
 *
 * <p>TODO(ender): Wrap the UrlBar in a separate standalone activity to focus testing on the
 * component alone. This should help deflake several tests here and focus on the logic and behavior.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@Batch(Batch.PER_CLASS)
public class UrlBarTest {
    public static @ClassRule ChromeTabbedActivityTestRule sActivityTestRule =
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
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();

        // Start with an empty Omnibox and disable all automatic features.
        mOmnibox.disableLiveAutocompletion();
        mOmnibox.requestFocus();
        mOmnibox.setText("");
    }

    private static class AutocompleteState {
        public final boolean hasAutocomplete;
        public final String textWithoutAutocomplete;
        public final String textWithAutocomplete;
        public final String additionalText;

        public AutocompleteState(
                boolean hasAutocomplete,
                String textWithoutAutocomplete,
                String textWithAutocomplete,
                String additionalText) {
            this.hasAutocomplete = hasAutocomplete;
            this.textWithoutAutocomplete = textWithoutAutocomplete;
            this.textWithAutocomplete = textWithAutocomplete;
            this.additionalText = additionalText;
        }
    }

    private AutocompleteState getAutocompleteState(final Runnable action) {
        final AtomicBoolean hasAutocomplete = new AtomicBoolean();
        final AtomicReference<String> textWithoutAutocomplete = new AtomicReference<String>();
        final AtomicReference<String> textWithAutocomplete = new AtomicReference<String>();
        final AtomicReference<String> additionalText = new AtomicReference<String>();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    if (action != null) action.run();
                    hasAutocomplete.set(mUrlBar.hasAutocomplete());
                    textWithoutAutocomplete.set(mUrlBar.getTextWithoutAutocomplete());
                    textWithAutocomplete.set(mUrlBar.getTextWithAutocomplete());
                    additionalText.set(mUrlBar.getAdditionalText().orElse(""));
                });

        return new AutocompleteState(
                hasAutocomplete.get(),
                textWithoutAutocomplete.get(),
                textWithAutocomplete.get(),
                additionalText.get());
    }

    private AutocompleteState setSelection(final int selectionStart, final int selectionEnd) {
        return getAutocompleteState(() -> mUrlBar.setSelection(selectionStart, selectionEnd));
    }

    private void setTextAndVerifyTextDirection(String text, int expectedDirection)
            throws TimeoutException {
        CallbackHelper directionCallback = new CallbackHelper();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mUrlBar.setUrlDirectionListener(
                            (direction) -> {
                                if (direction == expectedDirection) {
                                    directionCallback.notifyCalled();
                                }
                            });
                });
        mOmnibox.setText(text);
        directionCallback.waitForOnly(
                "Direction never reached expected direction: " + expectedDirection);
        assertUrlDirection(expectedDirection);
        ThreadUtils.runOnUiThreadBlocking(() -> mUrlBar.setUrlDirectionListener(null));
    }

    private void assertUrlDirection(int expectedDirection) {
        int actualDirection = ThreadUtils.runOnUiThreadBlocking(() -> mUrlBar.getUrlDirection());
        Assert.assertEquals(expectedDirection, actualDirection);
    }

    @Test
    @SmallTest
    public void testRefocusing() {
        // This test is flaky, because of the asynchronous nature of keyboard management that does
        // not involve canceling previously scheduled tasks.
        // For cases where keyboard is requested and dismissed rapidly, tasks begin to compete with
        // each other, and eventually a subsequent action's keyboard callup / dismiss is scheduled
        // before the previous action's counter-request, leading to a flake.
        for (int i = 0; i < 5; i++) {
            mOmnibox.requestFocus();
            mOmnibox.clearFocus();
        }
    }

    @Test
    @SmallTest
    public void testAutocompleteUpdatedOnSetText() {
        // Verify that setting a new string will clear the autocomplete.
        mOmnibox.setText("test");
        mOmnibox.setAutocompleteText("ing is fun", Optional.empty());

        // Replace part of the non-autocomplete text
        mOmnibox.setText("test");
        mOmnibox.setAutocompleteText("ing is fun", Optional.empty());
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mUrlBar.setText(mUrlBar.getText().replace(1, 2, "a"));
                });
        mOmnibox.checkText(equalTo("tast"), null);

        // Replace part of the autocomplete text.
        mOmnibox.setText("test");
        mOmnibox.setAutocompleteText("ing is fun", Optional.empty());
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mUrlBar.setText(mUrlBar.getText().replace(8, 10, "no"));
                });
        mOmnibox.checkText(equalTo("test"), null);
    }

    private void verifySelectionState(
            String text,
            String inlineAutocomplete,
            String additionalText,
            int selectionStart,
            int selectionEnd,
            boolean expectedHasAutocomplete,
            String expectedTextWithoutAutocomplete,
            String expectedTextWithAutocomplete,
            boolean expectedPreventInline,
            String expectedRequestedAutocompleteText)
            throws TimeoutException {
        mOmnibox.setText(text);
        mOmnibox.setAutocompleteText(inlineAutocomplete, Optional.of(additionalText));

        final CallbackHelper autocompleteHelper = new CallbackHelper();
        final AtomicReference<String> requestedAutocompleteText = new AtomicReference<String>();
        final AtomicBoolean didPreventInlineAutocomplete = new AtomicBoolean();
        mUrlBar.setTextChangeListener(
                (textWithoutAutocomplete) -> {
                    autocompleteHelper.notifyCalled();
                    requestedAutocompleteText.set(textWithoutAutocomplete);
                    didPreventInlineAutocomplete.set(!mUrlBar.shouldAutocomplete());
                    mUrlBar.setTextChangeListener(null);
                });

        AutocompleteState state = setSelection(selectionStart, selectionEnd);
        Assert.assertEquals("Has autocomplete", expectedHasAutocomplete, state.hasAutocomplete);
        Assert.assertEquals(
                "Text w/o Autocomplete",
                expectedTextWithoutAutocomplete,
                state.textWithoutAutocomplete);
        Assert.assertEquals(
                "Text w/ Autocomplete", expectedTextWithAutocomplete, state.textWithAutocomplete);
        Assert.assertEquals("Addition Text", additionalText, state.additionalText);

        autocompleteHelper.waitForCallback(0);
        Assert.assertEquals(
                "Prevent inline autocomplete",
                expectedPreventInline,
                didPreventInlineAutocomplete.get());
        Assert.assertEquals(
                "Requested autocomplete text",
                expectedRequestedAutocompleteText,
                requestedAutocompleteText.get());
    }

    @Test
    @SmallTest
    public void testAutocompleteUpdatedOnSelection() throws TimeoutException {
        // Verify that setting a selection before the autocomplete clears it.
        verifySelectionState(
                "test", "ing is fun", "foo.com", 1, 1, false, "test", "test", true, "test");

        // Verify that setting a selection range before the autocomplete clears it.
        verifySelectionState(
                "test", "ing is fun", "foo.com", 0, 4, false, "test", "test", true, "test");

        // Verify that setting a selection range that covers a portion of the non-autocomplete
        // and autocomplete text does not delete the autocomplete text.
        verifySelectionState(
                "test",
                "ing is fun",
                "foo.com",
                2,
                5,
                false,
                "testing is fun",
                "testing is fun",
                true,
                "testing is fun");

        // Verify that setting a selection range that over the entire string does not delete
        // the autocomplete text.
        verifySelectionState(
                "test",
                "ing is fun",
                "foo.com",
                0,
                14,
                false,
                "testing is fun",
                "testing is fun",
                true,
                "testing is fun");

        // Note: with new model touching the beginning of the autocomplete text is a no-op.
        // Verify that setting a selection at the end of the text does not delete the
        // autocomplete text.
        verifySelectionState(
                "test",
                "ing is fun",
                "foo.com",
                14,
                14,
                false,
                "testing is fun",
                "testing is fun",
                true,
                "testing is fun");

        // Verify that setting a selection in the middle of the autocomplete text does not delete
        // the autocomplete text.
        verifySelectionState(
                "test",
                "ing is fun",
                "foo.com",
                9,
                9,
                false,
                "testing is fun",
                "testing is fun",
                true,
                "testing is fun");

        // Verify that setting a selection range in the middle of the autocomplete text does not
        // delete the autocomplete text.
        verifySelectionState(
                "test",
                "ing is fun",
                "foo.com",
                8,
                11,
                false,
                "testing is fun",
                "testing is fun",
                true,
                "testing is fun");

        // Select autocomplete text. As we do not expect the suggestions to be refreshed, we test
        // this slightly differently than the other cases.
        mOmnibox.setText("test");
        mOmnibox.setAutocompleteText("ing is fun", Optional.of("www.bar.com"));
        ThreadUtils.runOnUiThreadBlocking(() -> mUrlBar.setSelection(4, 14));
        mOmnibox.checkText(equalTo("testing is fun"), null, equalTo("www.bar.com"));
    }

    /**
     * Ensure that we send cursor position with autocomplete requests.
     *
     * <p>When reading this test, it helps to remember that autocomplete requests are not sent with
     * the user simply moves the cursor. They're only sent on text modifications.
     */
    @Test
    @SmallTest
    public void testSendCursorPosition() throws TimeoutException {
        final CallbackHelper autocompleteHelper = new CallbackHelper();
        final AtomicInteger cursorPositionUsed = new AtomicInteger();
        mUrlBar.setTextChangeListener(
                (textWithoutAutocomplete) -> {
                    int cursorPosition =
                            mUrlBar.getSelectionEnd() == mUrlBar.getSelectionStart()
                                    ? mUrlBar.getSelectionStart()
                                    : -1;
                    cursorPositionUsed.set(cursorPosition);
                    autocompleteHelper.notifyCalled();
                });

        // User types "a".
        // Omnibox: a|
        mOmnibox.typeText("a", false);
        autocompleteHelper.waitForCallback(0);
        Assert.assertEquals(1, cursorPositionUsed.get());

        // Keyboard autocompletes "cd".
        // Omnibox: acd|
        mOmnibox.commitText("cd", true);
        autocompleteHelper.waitForCallback(1);
        Assert.assertEquals(3, cursorPositionUsed.get());

        // User moves the cursor.
        mOmnibox.sendKey(KeyEvent.KEYCODE_DPAD_LEFT);
        mOmnibox.sendKey(KeyEvent.KEYCODE_DPAD_LEFT);

        // Omnibox: a|cd.
        // No new events sent - cursor position movements don't count as autocomplete events.
        Assert.assertEquals(2, autocompleteHelper.getCallCount());
        Assert.assertEquals(3, cursorPositionUsed.get());

        // User appends "b"
        // Omnibox: ab|cd.
        mOmnibox.typeText("b", false);
        autocompleteHelper.waitForCallback(2);
        Assert.assertEquals(2, cursorPositionUsed.get());

        // User deletes "b"
        // Omnibox text: a|cd
        mOmnibox.sendKey(KeyEvent.KEYCODE_DEL);
        autocompleteHelper.waitForCallback(3);
        Assert.assertEquals(1, cursorPositionUsed.get());

        // User deletes "a"
        // Omnibox text: |cd
        mOmnibox.sendKey(KeyEvent.KEYCODE_DEL);
        autocompleteHelper.waitForCallback(4);
        Assert.assertEquals(0, cursorPositionUsed.get());

        mUrlBar.setTextChangeListener(null);
    }

    /**
     * Ensure that we allow inline autocomplete when the text gets shorter but is not an explicit
     * delete action by the user.
     *
     * <p>If you focus the omnibox and there is the selected text "[about:blank]", then typing new
     * text should clear that entirely and allow autocomplete on the newly entered text.
     *
     * <p>If we assume deletes happen any time the text gets shorter, then this would be prevented.
     */
    @Test
    @SmallTest
    public void testAutocompleteAllowedWhenReplacingText() throws TimeoutException {
        final String textToBeEntered = "c";

        final CallbackHelper autocompleteHelper = new CallbackHelper();
        final AtomicBoolean didPreventInlineAutocomplete = new AtomicBoolean();
        mUrlBar.setTextChangeListener(
                (textWithoutAutocomplete) -> {
                    if (!TextUtils.equals(textToBeEntered, mUrlBar.getTextWithoutAutocomplete())) {
                        return;
                    }
                    didPreventInlineAutocomplete.set(!mUrlBar.shouldAutocomplete());
                    autocompleteHelper.notifyCalled();
                    mUrlBar.setTextChangeListener(null);
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
        mOmnibox.setAutocompleteText("ing", Optional.empty());

        final CallbackHelper autocompleteHelper = new CallbackHelper();
        final AtomicBoolean didPreventInlineAutocomplete = new AtomicBoolean();
        mUrlBar.setTextChangeListener(
                (textWithoutAutocomplete) -> {
                    if (!TextUtils.equals("test", mUrlBar.getTextWithoutAutocomplete())) return;
                    didPreventInlineAutocomplete.set(!mUrlBar.shouldAutocomplete());
                    autocompleteHelper.notifyCalled();
                    mUrlBar.setTextChangeListener(null);
                });

        mOmnibox.sendKey(KeyEvent.KEYCODE_DEL);

        mOmnibox.checkText(equalTo("test"), null);

        autocompleteHelper.waitForCallback(0);
        Assert.assertTrue(
                "Inline autocomplete incorrectly allowed after delete.",
                didPreventInlineAutocomplete.get());
    }

    @Test
    @SmallTest
    public void testAutocorrectionChangesTriggerCorrectSuggestions() {
        mOmnibox.setComposingText("test", 0, 4);
        mOmnibox.setAutocompleteText("ing is fun", Optional.empty());
        mOmnibox.checkText(equalTo("test"), equalTo("testing is fun"));
        mOmnibox.commitText("rest", false);
        mOmnibox.checkText(equalTo("rest"), null);
    }

    @Test
    @SmallTest
    public void testAutocompletionChangesTriggerCorrectSuggestions() {
        // Type text. Make sure it appears as composing text for the IME.
        mOmnibox.setComposingText("test", 0, 4);
        mOmnibox.setAutocompleteText("ing is fun", Optional.empty());
        mOmnibox.checkText(equalTo("test"), equalTo("testing is fun"));
        mOmnibox.commitText("y", true);
        mOmnibox.checkText(equalTo("testy"), null);
    }

    @Test
    @SmallTest
    public void testAutocompleteCorrectlyPerservedOnBatchMode() {
        // Valid case (cursor at the end of text, single character, matches previous autocomplete).
        mOmnibox.setText("g");
        mOmnibox.setAutocompleteText("oogle.com", Optional.empty());
        mOmnibox.typeText("o", false);
        mOmnibox.checkText(equalTo("go"), equalTo("google.com"));

        // Invalid case (cursor not at the end of the text).
        mOmnibox.setText("g");
        mOmnibox.setAutocompleteText("oogle.com", Optional.empty());
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    InputConnection conn = mUrlBar.getInputConnection();
                    conn.beginBatchEdit();
                    conn.finishComposingText();
                    conn.commitText("o", 1);
                    conn.setSelection(0, 0);
                    conn.endBatchEdit();
                });
        mOmnibox.checkText(equalTo("go"), null);

        // Invalid case (next character did not match previous autocomplete)
        mOmnibox.setText("g");
        mOmnibox.setAutocompleteText("oogle.com", Optional.empty());
        mOmnibox.typeText("a", false);
        mOmnibox.checkText(equalTo("ga"), null);

        // Multiple characters entered instead of 1.
        mOmnibox.setText("g");
        mOmnibox.setAutocompleteText("oogle.com", Optional.empty());
        mOmnibox.commitText("oogl", true);
        mOmnibox.checkText(equalTo("googl"), equalTo("google.com"));
    }

    @Test
    @SmallTest
    public void testAutocompleteSpanClearedOnNonMatchingCommitText() {
        mOmnibox.setText("a");
        mOmnibox.setAutocompleteText("mazon.com", Optional.empty());
        mOmnibox.checkText(equalTo("a"), equalTo("amazon.com"));

        mOmnibox.typeText("l", false);
        mOmnibox.checkText(equalTo("al"), null);
    }

    @Test
    @SmallTest
    public void testAutocompleteClearedOnComposition() {
        mOmnibox.setText("test");
        mOmnibox.setAutocompleteText("ing is fun", Optional.empty());

        mOmnibox.setComposingText("ing compose", 4, 4);
        mOmnibox.checkText(equalTo("testing compose"), null);
    }

    @Test
    @SmallTest
    public void testDelayedCompositionCorrectedWithAutocomplete() {
        // Test with a single IME autocomplete
        mOmnibox.typeText("chrome://f", false);
        mOmnibox.setAutocompleteText("lags", Optional.empty());
        mOmnibox.setComposingText("l", 13, 14);
        mOmnibox.checkText(equalTo("chrome://fl"), equalTo("chrome://flags"));

        // Test with > 1 characters in composition.
        mOmnibox.setText("chrome://fl");
        mOmnibox.setAutocompleteText("ags", Optional.empty());
        mOmnibox.checkText(equalTo("chrome://fl"), equalTo("chrome://flags"));
        mOmnibox.setComposingText("fl", 12, 14);
        mOmnibox.checkText(equalTo("chrome://flfl"), null);

        // Test with non-matching composition.  Should just append to the URL text.
        mOmnibox.setText("chrome://f");
        mOmnibox.setAutocompleteText("lags", Optional.empty());
        mOmnibox.checkText(equalTo("chrome://f"), equalTo("chrome://flags"));
        mOmnibox.setComposingText("g", 13, 14);
        mOmnibox.checkText(equalTo("chrome://fg"), null);

        // Test with composition text that matches the entire text w/o autocomplete.
        mOmnibox.setText("chrome://f");
        mOmnibox.setAutocompleteText("lags", Optional.empty());
        mOmnibox.checkText(equalTo("chrome://f"), equalTo("chrome://flags"));
        mOmnibox.setComposingText("chrome://f", 13, 14);
        mOmnibox.checkText(equalTo("chrome://fchrome://f"), null);

        // Test with composition text longer than the URL text.
        // Shouldn't crash and should just append text.
        mOmnibox.setText("chrome://f");
        mOmnibox.setAutocompleteText("lags", Optional.empty());
        mOmnibox.checkText(equalTo("chrome://f"), equalTo("chrome://flags"));
        mOmnibox.setComposingText("blahblahblah", 13, 14);
        mOmnibox.checkText(equalTo("chrome://fblahblahblah"), null);
    }

    @Test
    @SmallTest
    @DisabledTest(message = "Disabled because of b/333536371")
    public void testUrlTextChangeListener() {
        Callback<String> listener = mock(Callback.class);
        mUrlBar.setTextChangeListener(listener);

        mOmnibox.setText("onomatop");
        Mockito.verify(listener).onResult("onomatop");

        // Setting autocomplete does not send a change update.
        mOmnibox.setAutocompleteText("oeia", Optional.empty());

        mOmnibox.setText("");
        Mockito.verify(listener).onResult("");
    }

    @Test
    @SmallTest
    public void testSetAutocompleteText_ShrinkingText() {
        mOmnibox.setText("test");
        mOmnibox.setAutocompleteText("ing is awesome", Optional.empty());
        mOmnibox.setAutocompleteText("ing is hard", Optional.empty());
        mOmnibox.setAutocompleteText("ingz", Optional.empty());
        mOmnibox.checkText(equalTo("test"), equalTo("testingz"), null, 4, 8);
    }

    @Test
    @SmallTest
    public void testSetAutocompleteTextWithAdditionalText_ShrinkingText() {
        mOmnibox.setText("test");
        mOmnibox.setAutocompleteText("ing is awesome", Optional.of("www.foobar.com"));
        mOmnibox.setAutocompleteText("ing is hard", Optional.of("www.bar.com"));
        mOmnibox.setAutocompleteText("ingz", Optional.of("www.foo.com"));
        mOmnibox.checkText(equalTo("test"), equalTo("testingz"), equalTo("www.foo.com"), 4, 8);
    }

    @Test
    @SmallTest
    public void testSetAutocompleteText_GrowingText() {
        mOmnibox.setText("test");
        mOmnibox.setAutocompleteText("ingz", Optional.empty());
        mOmnibox.setAutocompleteText("ing is hard", Optional.empty());
        mOmnibox.setAutocompleteText("ing is awesome", Optional.empty());
        mOmnibox.checkText(equalTo("test"), equalTo("testing is awesome"), null, 4, 18);
    }

    @Test
    @SmallTest
    public void testSetAutocompleteTextWithAdditionalText_GrowingText() {
        mOmnibox.setText("test");
        mOmnibox.setAutocompleteText("ingz", Optional.of("www.foo.com"));
        mOmnibox.setAutocompleteText("ing is hard", Optional.of("www.bar.com"));
        mOmnibox.setAutocompleteText("ing is awesome", Optional.of("www.foobar.com"));
        mOmnibox.checkText(
                equalTo("test"), equalTo("testing is awesome"), equalTo("www.foobar.com"), 4, 18);
    }

    @Test
    @SmallTest
    public void testSetAutocompleteText_DuplicateText() {
        mOmnibox.setText("test");
        mOmnibox.setAutocompleteText("ingz", Optional.empty());
        mOmnibox.setAutocompleteText("ingz", Optional.empty());
        mOmnibox.setAutocompleteText("ingz", Optional.empty());
        mOmnibox.checkText(equalTo("test"), equalTo("testingz"), null, 4, 8);
    }

    @Test
    @SmallTest
    public void testSetAutocompleteTextWithAdditionalText_DuplicateText() {
        mOmnibox.setText("test");
        mOmnibox.setAutocompleteText("ingz", Optional.of("www.foo.com"));
        mOmnibox.setAutocompleteText("ingz", Optional.of("www.foo.com"));
        mOmnibox.setAutocompleteText("ingz", Optional.of("www.foo.com"));
        mOmnibox.checkText(equalTo("test"), equalTo("testingz"), equalTo("www.foo.com"), 4, 8);
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
    public void typingStarted_emittedOncePerFocus() {
        typingStarted_emittedOncePerFocus(
                /* expectRetainOmniboxOnFocus= */ ThreadUtils.runOnUiThreadBlocking(
                        OmniboxFeatures::shouldRetainOmniboxOnFocus));
    }

    @Test
    @SmallTest
    public void typingStarted_emittedOncePerFocusWithRetainOmniboxOnFocusDisabled() {
        OmniboxFeatures.setShouldRetainOmniboxOnFocusForTesting(Boolean.FALSE);
        typingStarted_emittedOncePerFocus(/* expectRetainOmniboxOnFocus= */ false);
    }

    @Test
    @SmallTest
    public void typingStarted_emittedOncePerFocusWithRetainOmniboxOnFocusEnabled() {
        OmniboxFeatures.setShouldRetainOmniboxOnFocusForTesting(Boolean.TRUE);
        typingStarted_emittedOncePerFocus(/* expectRetainOmniboxOnFocus= */ true);
    }

    private void typingStarted_emittedOncePerFocus(boolean expectRetainOmniboxOnFocus) {
        assumeFalse(
                "TODO(crbug.com/347632178): Fix emit timing when retaining omnibox on focus.",
                expectRetainOmniboxOnFocus);

        var listener = mock(Runnable.class);

        mOmnibox.clearFocus();
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        mUrlBar.setTypingStartedListener(listener);
        mOmnibox.requestFocus();

        verifyNoInteractions(listener);

        // Verify that UrlBar emits a single Typing Started event.
        mOmnibox.typeText("a", false);
        verify(listener).run();

        clearInvocations(listener);

        // Verify no subsequent events emitted.
        mOmnibox.typeText("a", false);
        verifyNoInteractions(listener);
    }

    @Test
    @SmallTest
    public void typingStarted_emittedOnceEveryFocus() {
        typingStarted_emittedOnceEveryFocus(
                /* expectRetainOmniboxOnFocus= */ ThreadUtils.runOnUiThreadBlocking(
                        OmniboxFeatures::shouldRetainOmniboxOnFocus));
    }

    @Test
    @SmallTest
    public void typingStarted_emittedOnceEveryFocusWithRetainOmniboxOnFocusDisabled() {
        OmniboxFeatures.setShouldRetainOmniboxOnFocusForTesting(Boolean.FALSE);
        typingStarted_emittedOnceEveryFocus(/* expectRetainOmniboxOnFocus= */ false);
    }

    @Test
    @SmallTest
    public void typingStarted_emittedOnceEveryFocusWithRetainOmniboxOnFocusEnabled() {
        OmniboxFeatures.setShouldRetainOmniboxOnFocusForTesting(Boolean.TRUE);
        typingStarted_emittedOnceEveryFocus(/* expectRetainOmniboxOnFocus= */ true);
    }

    private void typingStarted_emittedOnceEveryFocus(boolean expectRetainOmniboxOnFocus) {
        assumeFalse(
                "TODO(crbug.com/347632178): Fix emit timing when retaining omnibox on focus.",
                expectRetainOmniboxOnFocus);

        var listener = mock(Runnable.class);

        mOmnibox.clearFocus();
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        mUrlBar.setTypingStartedListener(listener);
        mOmnibox.requestFocus();

        verifyNoInteractions(listener);

        // Verify that UrlBar emits a single Typing Started event.
        mOmnibox.typeText("a", false);
        verify(listener).run();

        mOmnibox.clearFocus();
        clearInvocations(listener);
        mOmnibox.requestFocus();

        // Verify no subsequent events emitted.
        mOmnibox.typeText("a", false);
        verify(listener).run();
    }

    @Test
    @SmallTest
    @RequiresRestart("crbug.com/358170962")
    public void typingStarted_notEmittedForNonTypingCharacters() {
        typingStarted_notEmittedForNonTypingCharacters(
                /* expectRetainOmniboxOnFocus= */ ThreadUtils.runOnUiThreadBlocking(
                        OmniboxFeatures::shouldRetainOmniboxOnFocus));
    }

    @Test
    @SmallTest
    @RequiresRestart("crbug.com/358170962")
    public void typingStarted_notEmittedForNonTypingCharactersWithRetainOmniboxOnFocusDisabled() {
        OmniboxFeatures.setShouldRetainOmniboxOnFocusForTesting(Boolean.FALSE);
        typingStarted_notEmittedForNonTypingCharacters(/* expectRetainOmniboxOnFocus= */ false);
    }

    @Test
    @SmallTest
    @RequiresRestart("crbug.com/358170962")
    public void typingStarted_notEmittedForNonTypingCharactersWithRetainOmniboxOnFocusEnabled() {
        OmniboxFeatures.setShouldRetainOmniboxOnFocusForTesting(Boolean.TRUE);
        typingStarted_notEmittedForNonTypingCharacters(/* expectRetainOmniboxOnFocus= */ true);
    }

    private void typingStarted_notEmittedForNonTypingCharacters(
            boolean expectRetainOmniboxOnFocus) {
        assumeFalse(
                "TODO(crbug.com/347632178): Fix emit timing when retaining omnibox on focus.",
                expectRetainOmniboxOnFocus);

        var listener = mock(Runnable.class);

        mOmnibox.clearFocus();
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        mUrlBar.setTypingStartedListener(listener);
        mOmnibox.requestFocus();

        var nonTypingKeys =
                List.of(
                        KeyEvent.KEYCODE_F1,
                        KeyEvent.KEYCODE_TAB,
                        KeyEvent.KEYCODE_SHIFT_LEFT,
                        KeyEvent.KEYCODE_DEL,
                        KeyEvent.KEYCODE_PAGE_UP,
                        KeyEvent.KEYCODE_DPAD_LEFT);

        for (int key : nonTypingKeys) {
            mOmnibox.sendKey(key);
            verifyNoInteractions(listener);
        }
    }

    @Test
    @SmallTest
    public void typingStarted_clipboardPasteTriggersTypingStarted() {
        typingStarted_clipboardPasteTriggersTypingStarted(
                /* expectRetainOmniboxOnFocus= */ ThreadUtils.runOnUiThreadBlocking(
                        OmniboxFeatures::shouldRetainOmniboxOnFocus));
    }

    @Test
    @SmallTest
    public void
            typingStarted_clipboardPasteTriggersTypingStartedWithRetainOmniboxOnFocusDisabled() {
        OmniboxFeatures.setShouldRetainOmniboxOnFocusForTesting(Boolean.FALSE);
        typingStarted_clipboardPasteTriggersTypingStarted(/* expectRetainOmniboxOnFocus= */ false);
    }

    @Test
    @SmallTest
    public void typingStarted_clipboardPasteTriggersTypingStartedWithRetainOmniboxOnFocusEnabled() {
        OmniboxFeatures.setShouldRetainOmniboxOnFocusForTesting(Boolean.TRUE);
        typingStarted_clipboardPasteTriggersTypingStarted(/* expectRetainOmniboxOnFocus= */ true);
    }

    private void typingStarted_clipboardPasteTriggersTypingStarted(
            boolean expectRetainOmniboxOnFocus) {
        assumeFalse(
                "TODO(crbug.com/347632178): Fix emit timing when retaining omnibox on focus.",
                expectRetainOmniboxOnFocus);

        var listener = mock(Runnable.class);

        mOmnibox.clearFocus();
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        mUrlBar.setTypingStartedListener(listener);
        mOmnibox.requestFocus();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Clipboard.getInstance().setText("");
                    // Paste directly. This is because Keyboard paste normally goes through an IME,
                    // which requires a lengthier process, rendering test flaky.
                    mUrlBar.onTextContextMenuItem(android.R.id.paste);
                });
        verifyNoInteractions(listener);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Clipboard.getInstance().setText("asdf");
                    // Paste directly. This is because Keyboard paste normally goes through an IME,
                    // which requires a lengthier process, rendering test flaky.
                    mUrlBar.onTextContextMenuItem(android.R.id.paste);
                });
        verify(listener).run();
    }
}

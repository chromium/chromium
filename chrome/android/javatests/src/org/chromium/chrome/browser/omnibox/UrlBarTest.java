// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import static org.chromium.base.test.util.Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE;

import android.annotation.SuppressLint;
import android.content.Context;
import android.content.res.Resources;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.SmallTest;
import android.text.Editable;
import android.text.TextUtils;
import android.view.KeyEvent;
import android.view.ViewGroup;
import android.view.inputmethod.BaseInputConnection;
import android.view.inputmethod.InputMethodManager;
import android.widget.FrameLayout;
import android.widget.LinearLayout;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.params.ParameterAnnotations.ClassParameter;
import org.chromium.base.test.params.ParameterAnnotations.UseRunnerDelegate;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Restriction;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.omnibox.UrlBar.UrlBarDelegate;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.ui.DummyUiActivityTestCase;
import org.chromium.chrome.test.util.OmniboxTestUtils;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.KeyUtils;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.util.Arrays;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.concurrent.Callable;
import java.util.concurrent.TimeoutException;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.atomic.AtomicReference;

/**
 * Tests for the URL bar UI component.
 */
@RunWith(ParameterizedRunner.class)
@UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
public class UrlBarTest extends DummyUiActivityTestCase {
    @ClassParameter
    private static List<ParameterSet> sClassParams =
            Arrays.asList(new ParameterSet().value(false).name("DisableSpannableInline"),
                    new ParameterSet().value(true).name("EnableSpannableInline"));

    private UrlBar mUrlBar;
    @Mock
    private UrlBarDelegate mUrlBarDelegate;

    public UrlBarTest(boolean enableSpannableInline) {
        Map<String, Boolean> featureList = new HashMap<>();
        featureList.put(ChromeFeatureList.SPANNABLE_INLINE_AUTOCOMPLETE, enableSpannableInline);
        ChromeFeatureList.setTestFeatures(featureList);
    }

    @Override
    public void setUpTest() throws Exception {
        super.setUpTest();
        MockitoAnnotations.initMocks(this);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ViewGroup view = new LinearLayout(getActivity());
            FrameLayout.LayoutParams params = new FrameLayout.LayoutParams(
                    ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT);
            getActivity().setContentView(view, params);

            ViewGroup urlBarContainer = new FrameLayout(getActivity());
            urlBarContainer.setFocusable(true);
            urlBarContainer.setFocusableInTouchMode(true);

            Resources res = getActivity().getResources();
            view.addView(urlBarContainer,
                    new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT,
                            res.getDimensionPixelSize(R.dimen.toolbar_height_no_shadow)
                                    - 2
                                            * res.getDimensionPixelSize(
                                                    R.dimen.location_bar_vertical_margin)));

            mUrlBar = (UrlBar) getActivity().getLayoutInflater().inflate(R.layout.url_bar, null);
            mUrlBar.setDelegate(mUrlBarDelegate);

            urlBarContainer.addView(mUrlBar, new FrameLayout.LayoutParams(params));
        });
    }

    // Prevent real keyboard app from interfering with test result. After calling this function,
    // real keyboard app will interact with null InputConnection while the test can still interact
    // with BaseInputConnection's method and thus affects EditText's Editable through
    // {@link UrlBar#getInputConnection()}. https://crbug.com/723901 for details.
    private void startIgnoringImeUntilRestart() {
        mUrlBar.setIgnoreImeForTest(true);
        InputMethodManager imm =
                (InputMethodManager) getActivity().getSystemService(Context.INPUT_METHOD_SERVICE);
        imm.restartInput(mUrlBar);
    }

    private void toggleFocusAndIgnoreImeOperations(final UrlBar urlBar, final boolean gainFocus) {
        OmniboxTestUtils.toggleUrlBarFocus(urlBar, gainFocus);
        if (gainFocus) {
            TestThreadUtils.runOnUiThreadBlocking(() -> {
                urlBar.setIgnoreTextChangesForAutocomplete(false);
                startIgnoringImeUntilRestart();
            });
            CriteriaHelper.pollUiThread(() -> urlBar.getInputConnection() != null,
                    "Input connection never initialized for URL bar.");
        }
    }

    private void runInputConnectionMethodOnUiThreadBlocking(final Runnable runnable) {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            // Note: in order for this to work correctly, the following conditions should be met
            // 1) Unset and set ignoreImeForTest within one UI loop.
            // 2) Do not restartInput() in between.
            mUrlBar.setIgnoreImeForTest(false);
            runnable.run();
            mUrlBar.setIgnoreImeForTest(true);
        });
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

    private Editable getUrlBarText() {
        return TestThreadUtils.runOnUiThreadBlockingNoException(() -> mUrlBar.getText());
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

    private void setTextAndVerifyNoAutocomplete(final String text) {
        AutocompleteState state = getAutocompleteState(() -> {
            mUrlBar.setText(text);
            mUrlBar.setSelection(text.length());
        });

        Assert.assertEquals(text, state.textWithoutAutocomplete);
        Assert.assertEquals(text, state.textWithAutocomplete);
        Assert.assertFalse(state.hasAutocomplete);
    }

    private void setAutocomplete(final String userText, final String autocompleteText) {
        AutocompleteState state =
                getAutocompleteState(() -> mUrlBar.setAutocompleteText(userText, autocompleteText));

        Assert.assertEquals(userText, state.textWithoutAutocomplete);
        Assert.assertEquals(userText + autocompleteText, state.textWithAutocomplete);
        Assert.assertTrue(state.hasAutocomplete);
    }

    private AutocompleteState setSelection(final int selectionStart, final int selectionEnd) {
        return getAutocompleteState(() -> mUrlBar.setSelection(selectionStart, selectionEnd));
    }

    @Test
    @SmallTest
    @Feature({"Omnibox"})
    @RetryOnFailure
    @DisabledTest
    public void testRefocusing() {
        Assert.assertFalse(OmniboxTestUtils.doesUrlBarHaveFocus(mUrlBar));
        OmniboxTestUtils.checkUrlBarRefocus(mUrlBar, 5);
    }

    @Test
    @SmallTest
    @Feature({"Omnibox"})
    @RetryOnFailure
    public void testAutocompleteUpdatedOnSetText() {
        toggleFocusAndIgnoreImeOperations(mUrlBar, true);

        // Verify that setting a new string will clear the autocomplete.
        setTextAndVerifyNoAutocomplete("test");
        setAutocomplete("test", "ing is fun");
        setTextAndVerifyNoAutocomplete("new string");

        // Replace part of the non-autocomplete text
        setTextAndVerifyNoAutocomplete("test");
        setAutocomplete("test", "ing is fun");
        AutocompleteState state = getAutocompleteState(new Runnable() {
            @Override
            public void run() {
                mUrlBar.setText(mUrlBar.getText().replace(1, 2, "a"));
            }
        });
        Assert.assertFalse(state.hasAutocomplete);
        // Clears autocomplete text when non-IME change has been made.
        // The autocomplete gets removed.
        Assert.assertEquals("tast", state.textWithoutAutocomplete);
        Assert.assertEquals("tast", state.textWithAutocomplete);

        // Replace part of the autocomplete text.
        setTextAndVerifyNoAutocomplete("test");
        setAutocomplete("test", "ing is fun");
        state = getAutocompleteState(new Runnable() {
            @Override
            public void run() {
                mUrlBar.setText(mUrlBar.getText().replace(8, 10, "no"));
            }
        });
        Assert.assertFalse(state.hasAutocomplete);
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.SPANNABLE_INLINE_AUTOCOMPLETE)) {
            // Note: new model clears autocomplete text when non-IME change has been made.
            // The autocomplete gets removed.
            Assert.assertEquals("test", state.textWithoutAutocomplete);
            Assert.assertEquals("test", state.textWithAutocomplete);
        } else {
            // The autocomplete gets committed.
            Assert.assertEquals("testing no fun", state.textWithoutAutocomplete);
            Assert.assertEquals("testing no fun", state.textWithAutocomplete);
        }
    }

    private void verifySelectionState(String text, String inlineAutocomplete, int selectionStart,
            int selectionEnd, boolean expectedHasAutocomplete,
            String expectedTextWithoutAutocomplete, String expectedTextWithAutocomplete,
            boolean expectedPreventInline, String expectedRequestedAutocompleteText)
            throws TimeoutException {
        setTextAndVerifyNoAutocomplete(text);
        setAutocomplete(text, inlineAutocomplete);

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
    @Feature({"Omnibox"})
    @RetryOnFailure
    public void testAutocompleteUpdatedOnSelection() throws TimeoutException {
        toggleFocusAndIgnoreImeOperations(mUrlBar, true);

        // Verify that setting a selection before the autocomplete clears it.
        verifySelectionState("test", "ing is fun", 1, 1, false, "test", "test", true, "test");

        // Verify that setting a selection range before the autocomplete clears it.
        verifySelectionState("test", "ing is fun", 0, 4, false, "test", "test", true, "test");

        // Note: with new model touching the beginning of the autocomplete text is a no-op.
        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.SPANNABLE_INLINE_AUTOCOMPLETE)) {
            // Verify that setting a selection at the start of the autocomplete clears it.
            verifySelectionState("test", "ing is fun", 4, 4, false, "test", "test", true, "test");
        }

        // Verify that setting a selection range that covers a portion of the non-autocomplete
        // and autocomplete text does not delete the autocomplete text.
        verifySelectionState("test", "ing is fun", 2, 5,
                false, "testing is fun", "testing is fun", true, "testing is fun");

        // Verify that setting a selection range that over the entire string does not delete
        // the autocomplete text.
        verifySelectionState("test", "ing is fun", 0, 14,
                false, "testing is fun", "testing is fun", true, "testing is fun");

        if (ChromeFeatureList.isEnabled(ChromeFeatureList.SPANNABLE_INLINE_AUTOCOMPLETE)) {
            // Note: with new model touching the beginning of the autocomplete text is a no-op.
            // Verify that setting a selection at the end of the text does not delete the
            // autocomplete text.
            verifySelectionState("test", "ing is fun", 14, 14, false, "testing is fun",
                    "testing is fun", true, "testing is fun");
        } else {
            // Verify that setting a selection at the end of the text does not delete the
            // autocomplete text.
            verifySelectionState("test", "ing is fun", 14, 14, false, "testing is fun",
                    "testing is fun", false, "testing is fun");
        }
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
        setTextAndVerifyNoAutocomplete("test");
        setAutocomplete("test", "ing is fun");
        AutocompleteState state = setSelection(4, 14);

        if (ChromeFeatureList.isEnabled(ChromeFeatureList.SPANNABLE_INLINE_AUTOCOMPLETE)) {
            // Note: with new model selecting the autocomplete text will commit autocomplete.
            Assert.assertEquals("Has autocomplete", false, state.hasAutocomplete);
            Assert.assertEquals(
                    "Text w/o Autocomplete", "testing is fun", state.textWithoutAutocomplete);
            Assert.assertEquals(
                    "Text w/ Autocomplete", "testing is fun", state.textWithAutocomplete);
        } else {
            // Verify that setting the same selection does not clear the autocomplete text.
            Assert.assertEquals("Has autocomplete", true, state.hasAutocomplete);
            Assert.assertEquals("Text w/o Autocomplete", "test", state.textWithoutAutocomplete);
            Assert.assertEquals(
                    "Text w/ Autocomplete", "testing is fun", state.textWithAutocomplete);
        }
    }

    /**
     * Ensure that we send cursor position with autocomplete requests.
     *
     * When reading this test, it helps to remember that autocomplete requests are not sent
     * with the user simply moves the cursor.  They're only sent on text modifications.
     */
    @Test
    @SmallTest
    @Feature({"Omnibox"})
    @RetryOnFailure
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
        toggleFocusAndIgnoreImeOperations(mUrlBar, true);

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
    @Feature({"Omnibox"})
    @RetryOnFailure
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

        toggleFocusAndIgnoreImeOperations(mUrlBar, true);

        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mUrlBar.getInputConnection().commitText(textToBeEntered, 1); });
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
    @Feature({"Omnibox"})
    @RetryOnFailure
    public void testSuggestionsUpdatedWhenDeletingInlineAutocomplete() throws TimeoutException {
        toggleFocusAndIgnoreImeOperations(mUrlBar, true);

        setTextAndVerifyNoAutocomplete("test");
        setAutocomplete("test", "ing");

        final CallbackHelper autocompleteHelper = new CallbackHelper();
        final AtomicBoolean didPreventInlineAutocomplete = new AtomicBoolean();
        mUrlBar.setUrlTextChangeListener((textWithoutAutocomplete, textWithAutocomplete) -> {
            if (!TextUtils.equals("test", mUrlBar.getTextWithoutAutocomplete())) return;
            didPreventInlineAutocomplete.set(!mUrlBar.shouldAutocomplete());
            autocompleteHelper.notifyCalled();
            mUrlBar.setUrlTextChangeListener(null);
        });

        runInputConnectionMethodOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                KeyUtils.singleKeyEventView(InstrumentationRegistry.getInstrumentation(), mUrlBar,
                        KeyEvent.KEYCODE_DEL);
            }
        });

        CriteriaHelper.pollUiThread(Criteria.equals("test", new Callable<String>() {
            @Override
            public String call() {
                return mUrlBar.getText().toString();
            }
        }));

        autocompleteHelper.waitForCallback(0);
        Assert.assertTrue("Inline autocomplete incorrectly allowed after delete.",
                didPreventInlineAutocomplete.get());
    }

    @Test
    @SmallTest
    @Feature({"Omnibox"})
    @RetryOnFailure
    public void testSelectionChangesIgnoredInBatchMode() {
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.SPANNABLE_INLINE_AUTOCOMPLETE)) {
            // Note: with the new model, we remove autocomplete text at the beginning of a batch
            // edit and add it at the end of a batch edit.
            return;
        }
        toggleFocusAndIgnoreImeOperations(mUrlBar, true);

        setTextAndVerifyNoAutocomplete("test");
        setAutocomplete("test", "ing is fun");

        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mUrlBar.getInputConnection().beginBatchEdit(); });
        // Ensure the autocomplete is not modified if in batch mode.
        AutocompleteState state = setSelection(1, 1);
        Assert.assertTrue(state.hasAutocomplete);
        Assert.assertEquals("test", state.textWithoutAutocomplete);
        Assert.assertEquals("testing is fun", state.textWithAutocomplete);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mUrlBar.getInputConnection().endBatchEdit(); });
        // Ensure that after batch mode has ended that the autocomplete is cleared due to the
        // invalid selection range.
        state = getAutocompleteState(null);
        Assert.assertFalse(state.hasAutocomplete);
        Assert.assertEquals("test", state.textWithoutAutocomplete);
        Assert.assertEquals("test", state.textWithAutocomplete);
    }

    @Test
    @SmallTest
    @Feature({"Omnibox"})
    @RetryOnFailure
    public void testBatchModeChangesTriggerCorrectSuggestions() {
        final AtomicReference<String> requestedAutocompleteText = new AtomicReference<String>();
        mUrlBar.setUrlTextChangeListener(
                (textWithoutAutocomplete, textWithAutocomplete)
                        -> requestedAutocompleteText.set(mUrlBar.getTextWithoutAutocomplete()));

        toggleFocusAndIgnoreImeOperations(mUrlBar, true);

        setTextAndVerifyNoAutocomplete("test");
        setAutocomplete("test", "ing is fun");

        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mUrlBar.getInputConnection().beginBatchEdit(); });
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mUrlBar.getInputConnection().commitText("y", 1); });
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mUrlBar.getInputConnection().endBatchEdit(); });

        CriteriaHelper.pollUiThread(Criteria.equals("testy", new Callable<String>() {
            @Override
            public String call() {
                return requestedAutocompleteText.get();
            }
        }));

        mUrlBar.setUrlTextChangeListener(null);
    }

    @Test
    @SmallTest
    @Feature("Omnibox")
    @RetryOnFailure
    public void testAutocompleteCorrectlyPerservedOnBatchMode() {
        toggleFocusAndIgnoreImeOperations(mUrlBar, true);
        OmniboxTestUtils.waitForFocusAndKeyboardActive(mUrlBar, true);

        // Valid case (cursor at the end of text, single character, matches previous autocomplete).
        setTextAndVerifyNoAutocomplete("g");
        setAutocomplete("g", "oogle.com");
        AutocompleteState state = getAutocompleteState(new Runnable() {
            @Override
            // TODO(crbug.com/635567): Fix this properly.
            @SuppressLint("SetTextI18n")
            public void run() {
                mUrlBar.getInputConnection().commitText("o", 1);
            }
        });
        Assert.assertTrue(state.hasAutocomplete);
        Assert.assertEquals("google.com", state.textWithAutocomplete);
        Assert.assertEquals("go", state.textWithoutAutocomplete);

        // Invalid case (cursor not at the end of the text)
        setTextAndVerifyNoAutocomplete("g");
        setAutocomplete("g", "oogle.com");
        state = getAutocompleteState(new Runnable() {
            @Override
            // TODO(crbug.com/635567): Fix this properly.
            @SuppressLint("SetTextI18n")
            public void run() {
                mUrlBar.getInputConnection().beginBatchEdit();
                mUrlBar.getInputConnection().commitText("o", 1);
                mUrlBar.getInputConnection().setSelection(0, 0);
                mUrlBar.getInputConnection().endBatchEdit();
            }
        });
        Assert.assertFalse(state.hasAutocomplete);

        // Invalid case (next character did not match previous autocomplete)
        setTextAndVerifyNoAutocomplete("g");
        setAutocomplete("g", "oogle.com");
        state = getAutocompleteState(new Runnable() {
            @Override
            // TODO(crbug.com/635567): Fix this properly.
            @SuppressLint("SetTextI18n")
            public void run() {
                mUrlBar.getInputConnection().commitText("a", 1);
            }
        });
        Assert.assertFalse(state.hasAutocomplete);

        // Multiple characters entered instead of 1.
        setTextAndVerifyNoAutocomplete("g");
        setAutocomplete("g", "oogle.com");
        state = getAutocompleteState(new Runnable() {
            @Override
            // TODO(crbug.com/635567): Fix this properly.
            @SuppressLint("SetTextI18n")
            public void run() {
                mUrlBar.getInputConnection().commitText("oogl", 1);
            }
        });
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.SPANNABLE_INLINE_AUTOCOMPLETE)) {
            // Note: new model allows multiple characters because usually keyboard app's UI and
            // InputConnection threads are separated and user may type fast enough.
            Assert.assertTrue(state.hasAutocomplete);
        } else {
            Assert.assertFalse(state.hasAutocomplete);
        }
    }

    @Test
    @SmallTest
    @Feature("Omnibox")
    @RetryOnFailure
    public void testAutocompleteSpanClearedOnNonMatchingCommitText() {
        toggleFocusAndIgnoreImeOperations(mUrlBar, true);
        OmniboxTestUtils.waitForFocusAndKeyboardActive(mUrlBar, true);

        setTextAndVerifyNoAutocomplete("a");
        setAutocomplete("a", "mazon.com");

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mUrlBar.getInputConnection().beginBatchEdit();
            mUrlBar.getInputConnection().commitText("l", 1);
            mUrlBar.getInputConnection().setComposingText("", 1);
            mUrlBar.getInputConnection().endBatchEdit();
        });

        CriteriaHelper.pollUiThread(Criteria.equals("al", new Callable<String>() {
            @Override
            public String call() {
                return mUrlBar.getText().toString();
            }
        }));
    }

    @Test
    @SmallTest
    @Feature({"Omnibox"})
    @RetryOnFailure
    public void testAutocompleteClearedOnComposition() {
        toggleFocusAndIgnoreImeOperations(mUrlBar, true);
        OmniboxTestUtils.waitForFocusAndKeyboardActive(mUrlBar, true);

        setTextAndVerifyNoAutocomplete("test");
        setAutocomplete("test", "ing is fun");

        Assert.assertNotNull(mUrlBar.getInputConnection());
        AutocompleteState state = getAutocompleteState(new Runnable() {
            @Override
            public void run() {
                mUrlBar.getInputConnection().setComposingText("ing compose", 4);
            }
        });
        Assert.assertFalse(state.hasAutocomplete);

        Editable urlText = getUrlBarText();
        Assert.assertEquals("testing compose", urlText.toString());
        // TODO(tedchoc): Investigate why this fails on x86.
        //assertEquals(4, BaseInputConnection.getComposingSpanStart(urlText));
        //assertEquals(15, BaseInputConnection.getComposingSpanEnd(urlText));
    }

    @Test
    @SmallTest
    @Feature("Omnibox")
    @RetryOnFailure
    @Restriction({RESTRICTION_TYPE_NON_LOW_END_DEVICE}) // crbug.com/635714
    public void testDelayedCompositionCorrectedWithAutocomplete() {
        toggleFocusAndIgnoreImeOperations(mUrlBar, true);
        OmniboxTestUtils.waitForFocusAndKeyboardActive(mUrlBar, true);

        Assert.assertNotNull(mUrlBar.getInputConnection());

        // Test with a single autocomplete

        setTextAndVerifyNoAutocomplete("chrome://f");
        setAutocomplete("chrome://f", "lags");

        AutocompleteState state = getAutocompleteState(new Runnable() {
            @Override
            public void run() {
                mUrlBar.getInputConnection().setComposingRegion(13, 14);
                mUrlBar.getInputConnection().setComposingText("f", 1);
            }
        });

        Editable urlText = getUrlBarText();
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.SPANNABLE_INLINE_AUTOCOMPLETE)) {
            // Note: the new model hides autocomplete text from IME.
            // setComposingRegion fails because autocomplete is hidden from IME. In reality, IME
            // shouldn't do this.
            Assert.assertFalse(state.hasAutocomplete);
            Assert.assertEquals("chrome://ff", urlText.toString());
            Assert.assertEquals(BaseInputConnection.getComposingSpanStart(urlText), 10);
            Assert.assertEquals(BaseInputConnection.getComposingSpanEnd(urlText), 11);
        } else {
            Assert.assertFalse(state.hasAutocomplete);
            Assert.assertEquals("chrome://f", urlText.toString());
            Assert.assertEquals(BaseInputConnection.getComposingSpanStart(urlText), 9);
            Assert.assertEquals(BaseInputConnection.getComposingSpanEnd(urlText), 10);
        }

        // Test with > 1 characters in composition.

        setTextAndVerifyNoAutocomplete("chrome://fl");
        setAutocomplete("chrome://fl", "ags");

        state = getAutocompleteState(new Runnable() {
            @Override
            public void run() {
                mUrlBar.getInputConnection().setComposingRegion(12, 14);
                mUrlBar.getInputConnection().setComposingText("fl", 1);
            }
        });
        urlText = getUrlBarText();

        if (ChromeFeatureList.isEnabled(ChromeFeatureList.SPANNABLE_INLINE_AUTOCOMPLETE)) {
            // Note: the new model hides autocomplete text from IME.
            // setComposingRegion fails because autocomplete is hidden from IME. In reality, IME
            // shouldn't do this.
            Assert.assertFalse(state.hasAutocomplete);
            Assert.assertEquals("chrome://flfl", urlText.toString());
            Assert.assertEquals(BaseInputConnection.getComposingSpanStart(urlText), 11);
            Assert.assertEquals(BaseInputConnection.getComposingSpanEnd(urlText), 13);
        } else {
            Assert.assertFalse(state.hasAutocomplete);
            Assert.assertEquals("chrome://fl", urlText.toString());
            Assert.assertEquals(BaseInputConnection.getComposingSpanStart(urlText), 9);
            Assert.assertEquals(BaseInputConnection.getComposingSpanEnd(urlText), 11);
        }

        // Test with non-matching composition.  Should just append to the URL text.

        setTextAndVerifyNoAutocomplete("chrome://f");
        setAutocomplete("chrome://f", "lags");

        state = getAutocompleteState(new Runnable() {
            @Override
            public void run() {
                mUrlBar.getInputConnection().setComposingRegion(13, 14);
                mUrlBar.getInputConnection().setComposingText("g", 1);
            }
        });
        Assert.assertFalse(state.hasAutocomplete);

        urlText = getUrlBarText();
        Assert.assertEquals("chrome://fg", urlText.toString());
        Assert.assertEquals(BaseInputConnection.getComposingSpanStart(urlText), 10);
        Assert.assertEquals(BaseInputConnection.getComposingSpanEnd(urlText), 11);

        // Test with composition text that matches the entire text w/o autocomplete.

        setTextAndVerifyNoAutocomplete("chrome://f");
        setAutocomplete("chrome://f", "lags");

        state = getAutocompleteState(new Runnable() {
            @Override
            public void run() {
                mUrlBar.getInputConnection().setComposingRegion(13, 14);
                mUrlBar.getInputConnection().setComposingText("chrome://f", 1);
            }
        });
        urlText = getUrlBarText();
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.SPANNABLE_INLINE_AUTOCOMPLETE)) {
            // Note: the new model hides autocomplete text from IME.
            // setComposingRegion fails because autocomplete is hidden from IME. In reality, IME
            // shouldn't do this.
            Assert.assertFalse(state.hasAutocomplete);
            Assert.assertEquals("chrome://fchrome://f", urlText.toString());
            Assert.assertEquals(BaseInputConnection.getComposingSpanStart(urlText), 10);
            Assert.assertEquals(BaseInputConnection.getComposingSpanEnd(urlText), 20);
        } else {
            Assert.assertFalse(state.hasAutocomplete);
            Assert.assertEquals("chrome://f", urlText.toString());
            Assert.assertEquals(BaseInputConnection.getComposingSpanStart(urlText), 0);
            Assert.assertEquals(BaseInputConnection.getComposingSpanEnd(urlText), 10);
        }

        // Test with composition text longer than the URL text.  Shouldn't crash and should
        // just append text.

        setTextAndVerifyNoAutocomplete("chrome://f");
        setAutocomplete("chrome://f", "lags");

        state = getAutocompleteState(new Runnable() {
            @Override
            public void run() {
                mUrlBar.getInputConnection().setComposingRegion(13, 14);
                mUrlBar.getInputConnection().setComposingText("blahblahblah", 1);
            }
        });
        Assert.assertFalse(state.hasAutocomplete);

        urlText = getUrlBarText();
        Assert.assertEquals("chrome://fblahblahblah", urlText.toString());
        Assert.assertEquals(BaseInputConnection.getComposingSpanStart(urlText), 10);
        Assert.assertEquals(BaseInputConnection.getComposingSpanEnd(urlText), 22);
    }
}

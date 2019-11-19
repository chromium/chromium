// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.omnibox;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.inOrder;
import static org.mockito.Mockito.spy;

import android.app.Activity;
import android.content.Context;
import android.text.SpannableString;
import android.text.TextUtils;
import android.util.AttributeSet;
import android.view.KeyEvent;
import android.view.accessibility.AccessibilityEvent;
import android.view.inputmethod.BaseInputConnection;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputConnection;
import android.widget.LinearLayout;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.InOrder;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.Shadows;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowAccessibilityManager;
import org.robolectric.shadows.ShadowLog;

import org.chromium.base.Log;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;

import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.atomic.AtomicReference;

/**
 * A robolectric test for {@link AutocompleteEditText} class.
 * TODO(changwan): switch to ParameterizedRobolectricTest once crbug.com/733324 is fixed.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class AutocompleteEditTextTest {
    private static final String TAG = "AutocompleteTest";

    private static final boolean DEBUG = false;

    // Robolectric's ShadowAccessibilityManager has a bug (crbug.com/756707). Turn this on once it's
    // fixed, and you can turn this off temporarily when upgrading robolectric library.
    private static final boolean TEST_ACCESSIBILITY = true;

    @Rule
    public TestRule mProcessor = new Features.JUnitProcessor();

    private InOrder mInOrder;
    private TestAutocompleteEditText mAutocomplete;
    private LinearLayout mFocusPlaceHolder;

    private Context mContext;
    private InputConnection mInputConnection;
    private Verifier mVerifier;
    private ShadowAccessibilityManager mShadowAccessibilityManager;
    private boolean mIsShown;

    // Limits the target of InOrder#verify.
    private static class Verifier {
        public void onAutocompleteTextStateChanged(boolean updateDisplay) {
            if (DEBUG) Log.i(TAG, "onAutocompleteTextStateChanged(%b)", updateDisplay);
        }

        public void onUpdateSelection(int selStart, int selEnd) {
            if (DEBUG) Log.i(TAG, "onUpdateSelection(%d, %d)", selStart, selEnd);
        }

        public void onPopulateAccessibilityEvent(int eventType, String text, String beforeText,
                int itemCount, int fromIndex, int toIndex, int removedCount, int addedCount) {
            if (DEBUG) {
                Log.i(TAG, "onPopulateAccessibilityEvent: TYP[%d] TXT[%s] BEF[%s] CNT[%d] "
                        + "FROM[%d] TO[%d] REM[%d] ADD[%d]",
                        eventType, text, beforeText, itemCount, fromIndex, toIndex, removedCount,
                        addedCount);
            }
        }
    }

    private class TestAutocompleteEditText extends AutocompleteEditText {
        private AtomicInteger mVerifierCallCount = new AtomicInteger();
        private AtomicInteger mAccessibilityVerifierCallCount = new AtomicInteger();
        private AtomicReference<String> mKeyboardPackageName = new AtomicReference<>("dummy.ime");

        public TestAutocompleteEditText(Context context, AttributeSet attrs) {
            super(context, attrs);
            if (DEBUG) Log.i(TAG, "TestAutocompleteEditText constructor");
        }

        @Override
        public void onAutocompleteTextStateChanged(boolean updateDisplay) {
            mVerifier.onAutocompleteTextStateChanged(updateDisplay);
            mVerifierCallCount.incrementAndGet();
        }

        @Override
        public void onUpdateSelectionForTesting(int selStart, int selEnd) {
            mVerifier.onUpdateSelection(selStart, selEnd);
            mVerifierCallCount.incrementAndGet();
        }

        @Override
        public void onPopulateAccessibilityEvent(AccessibilityEvent event) {
            super.onPopulateAccessibilityEvent(event);
            if (TEST_ACCESSIBILITY) {
                mVerifier.onPopulateAccessibilityEvent(event.getEventType(), getText(event),
                        getBeforeText(event), event.getItemCount(), event.getFromIndex(),
                        event.getToIndex(), event.getRemovedCount(), event.getAddedCount());
                mAccessibilityVerifierCallCount.incrementAndGet();
            }
        }

        private String getText(AccessibilityEvent event) {
            if (event.getText() != null && event.getText().size() > 0) {
                return event.getText().get(0).toString();
            }
            return "";
        }

        private String getBeforeText(AccessibilityEvent event) {
            return event.getBeforeText() == null ? "" : event.getBeforeText().toString();
        }

        @Override
        public boolean isShown() {
            return mIsShown;
        }

        public int getAndResetVerifierCallCount() {
            return mVerifierCallCount.getAndSet(0);
        }

        public int getAndResetAccessibilityVerifierCallCount() {
            return mAccessibilityVerifierCallCount.getAndSet(0);
        }

        @Override
        public String getKeyboardPackageName() {
            return mKeyboardPackageName.get();
        }

        public void setKeyboardPackageName(String packageName) {
            mKeyboardPackageName.set(packageName);
        }
    }

    public AutocompleteEditTextTest() {
        if (DEBUG) ShadowLog.stream = System.out;
    }

    private boolean isUsingSpannableModel() {
        return ChromeFeatureList.isEnabled(ChromeFeatureList.SPANNABLE_INLINE_AUTOCOMPLETE);
    }

    @Before
    public void setUp() {
        if (DEBUG) Log.i(TAG, "setUp started.");
        MockitoAnnotations.initMocks(this);
        mContext = RuntimeEnvironment.application;

        mVerifier = spy(new Verifier());
        mAutocomplete = new TestAutocompleteEditText(mContext, null);
        mFocusPlaceHolder = new LinearLayout(mContext);
        mFocusPlaceHolder.setFocusable(true);
        mFocusPlaceHolder.addView(mAutocomplete);
        assertNotNull(mAutocomplete);

        // Pretend that the view is shown in the activity hierarchy, which is for accessibility
        // testing.
        Activity activity = Robolectric.buildActivity(Activity.class).create().get();
        activity.setContentView(mFocusPlaceHolder);
        assertNotNull(mFocusPlaceHolder.getParent());
        mIsShown = true;
        assertTrue(mAutocomplete.isShown());

        // Enable accessibility.
        mShadowAccessibilityManager =
                Shadows.shadowOf(mAutocomplete.getAccessibilityManagerForTesting());
        mShadowAccessibilityManager.setEnabled(true);
        mShadowAccessibilityManager.setTouchExplorationEnabled(true);
        assertTrue(mAutocomplete.getAccessibilityManagerForTesting().isEnabled());
        assertTrue(mAutocomplete.getAccessibilityManagerForTesting().isTouchExplorationEnabled());

        mInOrder = inOrder(mVerifier);
        assertTrue(mAutocomplete.requestFocus());
        verifyOnPopulateAccessibilityEvent(
                AccessibilityEvent.TYPE_VIEW_FOCUSED, "", "", 2, -1, -1, -1, -1);
        assertNotNull(mAutocomplete.onCreateInputConnection(new EditorInfo()));
        mInputConnection = mAutocomplete.getInputConnection();
        assertNotNull(mInputConnection);
        mInOrder.verifyNoMoreInteractions();
        assertVerifierCallCounts(0, 1);

        // Feeder should call this at the beginning.
        mAutocomplete.setIgnoreTextChangesForAutocomplete(false);

        if (DEBUG) Log.i(TAG, "setUp finished.");
    }

    private void assertTexts(String userText, String autocompleteText) {
        assertEquals(userText, mAutocomplete.getTextWithoutAutocomplete());
        assertEquals(userText + autocompleteText, mAutocomplete.getTextWithAutocomplete());
        assertEquals(autocompleteText.length(), mAutocomplete.getAutocompleteLength());
        assertEquals(!TextUtils.isEmpty(autocompleteText), mAutocomplete.hasAutocomplete());
    }

    private void assertVerifierCallCounts(
            int nonAccessibilityCallCount, int accessibilityCallCount) {
        assertEquals(nonAccessibilityCallCount, mAutocomplete.getAndResetVerifierCallCount());
        if (!TEST_ACCESSIBILITY) return;
        assertEquals(
                accessibilityCallCount, mAutocomplete.getAndResetAccessibilityVerifierCallCount());
    }

    private void verifyOnPopulateAccessibilityEvent(int eventType, String text, String beforeText,
            int itemCount, int fromIndex, int toIndex, int removedCount, int addedCount) {
        if (!TEST_ACCESSIBILITY) return;
        mInOrder.verify(mVerifier).onPopulateAccessibilityEvent(eventType, text, beforeText,
                itemCount, fromIndex, toIndex, removedCount, addedCount);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.SPANNABLE_INLINE_AUTOCOMPLETE)
    public void testAppend_CommitTextWithSpannableModel() {
        internalTestAppend_CommitText();
    }

    @Test
    @DisableFeatures(ChromeFeatureList.SPANNABLE_INLINE_AUTOCOMPLETE)
    public void testAppend_CommitTextWithoutSpannableModel() {
        internalTestAppend_CommitText();
    }

    private void internalTestAppend_CommitText() {
        // User types "h".
        assertTrue(mInputConnection.commitText("h", 1));
        if (isUsingSpannableModel()) {
            mInOrder.verify(mVerifier).onUpdateSelection(1, 1);
            verifyOnPopulateAccessibilityEvent(
                    AccessibilityEvent.TYPE_VIEW_TEXT_CHANGED, "h", "", -1, 0, -1, 0, 1);
            verifyOnPopulateAccessibilityEvent(
                    AccessibilityEvent.TYPE_VIEW_TEXT_SELECTION_CHANGED, "h", "", 1, 1, 1, -1, -1);
            mInOrder.verify(mVerifier).onAutocompleteTextStateChanged(false);
            assertVerifierCallCounts(2, 2);
        } else {
            mInOrder.verify(mVerifier).onAutocompleteTextStateChanged(true);
            verifyOnPopulateAccessibilityEvent(
                    AccessibilityEvent.TYPE_VIEW_TEXT_CHANGED, "h", "", -1, 0, -1, 0, 1);
            mInOrder.verify(mVerifier).onUpdateSelection(1, 1);
            verifyOnPopulateAccessibilityEvent(
                    AccessibilityEvent.TYPE_VIEW_TEXT_SELECTION_CHANGED, "h", "", 1, 1, 1, -1, -1);
            assertVerifierCallCounts(2, 2);
        }
        mInOrder.verifyNoMoreInteractions();
        assertTrue(mAutocomplete.shouldAutocomplete());

        // The controller kicks in.
        mAutocomplete.setAutocompleteText("h", "ello world");
        if (isUsingSpannableModel()) {
            assertFalse(mAutocomplete.isCursorVisible());
            verifyOnPopulateAccessibilityEvent(AccessibilityEvent.TYPE_VIEW_TEXT_CHANGED,
                    "hello world", "h", -1, 1, -1, 0, 10);
            assertVerifierCallCounts(0, 1);
        } else {
            // The non-spannable model changes selection in two steps.
            verifyOnPopulateAccessibilityEvent(AccessibilityEvent.TYPE_VIEW_TEXT_CHANGED,
                    "hello world", "h", -1, 1, -1, 0, 10);
            mInOrder.verify(mVerifier).onUpdateSelection(11, 11);
            verifyOnPopulateAccessibilityEvent(AccessibilityEvent.TYPE_VIEW_TEXT_SELECTION_CHANGED,
                    "hello world", "", 11, 11, 11, -1, -1);
            mInOrder.verify(mVerifier).onUpdateSelection(1, 11);
            verifyOnPopulateAccessibilityEvent(AccessibilityEvent.TYPE_VIEW_TEXT_SELECTION_CHANGED,
                    "hello world", "", 11, 1, 11, -1, -1);
            assertVerifierCallCounts(2, 3);
        }
        mInOrder.verifyNoMoreInteractions();
        assertTrue(mAutocomplete.shouldAutocomplete());

        // User types "he".
        assertTrue(mInputConnection.commitText("e", 1));
        if (isUsingSpannableModel()) {
            mInOrder.verify(mVerifier).onUpdateSelection(2, 2);
            verifyOnPopulateAccessibilityEvent(AccessibilityEvent.TYPE_VIEW_TEXT_SELECTION_CHANGED,
                    "hello world", "", 11, 2, 2, -1, -1);
            verifyOnPopulateAccessibilityEvent(AccessibilityEvent.TYPE_VIEW_TEXT_CHANGED,
                    "hello world", "he", -1, 2, -1, 0, 9);
            mInOrder.verify(mVerifier).onAutocompleteTextStateChanged(false);
            assertVerifierCallCounts(2, 2);
        } else {
            verifyOnPopulateAccessibilityEvent(
                    AccessibilityEvent.TYPE_VIEW_TEXT_CHANGED, "hello world", "h", -1, 1, -1, 0, 1);
            mInOrder.verify(mVerifier).onUpdateSelection(2, 11);
            verifyOnPopulateAccessibilityEvent(AccessibilityEvent.TYPE_VIEW_TEXT_SELECTION_CHANGED,
                    "hello world", "", 11, 2, 11, -1, -1);
            mInOrder.verify(mVerifier).onAutocompleteTextStateChanged(false);
            assertVerifierCallCounts(2, 2);
        }
        mInOrder.verifyNoMoreInteractions();
        assertTrue(mAutocomplete.shouldAutocomplete());
        // The controller kicks in.
        mAutocomplete.setAutocompleteText("he", "llo world");
        if (isUsingSpannableModel()) assertFalse(mAutocomplete.isCursorVisible());

        mInOrder.verifyNoMoreInteractions();
        assertTexts("he", "llo world");
        assertTrue(mAutocomplete.shouldAutocomplete());

        // User types "hello".
        assertTrue(mInputConnection.commitText("llo", 1));
        if (isUsingSpannableModel()) {
            mInOrder.verify(mVerifier).onUpdateSelection(5, 5);
            verifyOnPopulateAccessibilityEvent(AccessibilityEvent.TYPE_VIEW_TEXT_SELECTION_CHANGED,
                    "hello world", "", 11, 5, 5, -1, -1);
            verifyOnPopulateAccessibilityEvent(AccessibilityEvent.TYPE_VIEW_TEXT_CHANGED,
                    "hello world", "hello", -1, 5, -1, 0, 6);
            mInOrder.verify(mVerifier).onAutocompleteTextStateChanged(false);
            assertVerifierCallCounts(2, 2);
        } else {
            mInOrder.verify(mVerifier).onAutocompleteTextStateChanged(false);
            mInOrder.verify(mVerifier).onUpdateSelection(11, 11);
            verifyOnPopulateAccessibilityEvent(AccessibilityEvent.TYPE_VIEW_TEXT_SELECTION_CHANGED,
                    "hello world", "", 11, 11, 11, -1, -1);
            mInOrder.verify(mVerifier).onAutocompleteTextStateChanged(true);
            verifyOnPopulateAccessibilityEvent(AccessibilityEvent.TYPE_VIEW_TEXT_CHANGED, "hello",
                    "hello world", -1, 2, -1, 9, 3);
            mInOrder.verify(mVerifier).onUpdateSelection(5, 5);
            // The old model does not continue the existing autocompletion when two letters are
            // typed together, which can cause a slight flicker.
            verifyOnPopulateAccessibilityEvent(AccessibilityEvent.TYPE_VIEW_TEXT_SELECTION_CHANGED,
                    "hello", "", 5, 5, 5, -1, -1);
            assertVerifierCallCounts(4, 3);
        }
        mInOrder.verifyNoMoreInteractions();
        assertTrue(mAutocomplete.shouldAutocomplete());
        // The controller kicks in.
        mAutocomplete.setAutocompleteText("hello", " world");
        if (isUsingSpannableModel()) {
            assertFalse(mAutocomplete.isCursorVisible());
            assertVerifierCallCounts(0, 0);
        } else {
            verifyOnPopulateAccessibilityEvent(AccessibilityEvent.TYPE_VIEW_TEXT_CHANGED,
                    "hello world", "hello", -1, 5, -1, 0, 6);
            mInOrder.verify(mVerifier).onUpdateSelection(11, 11);
            verifyOnPopulateAccessibilityEvent(AccessibilityEvent.TYPE_VIEW_TEXT_SELECTION_CHANGED,
                    "hello world", "", 11, 11, 11, -1, -1);
            mInOrder.verify(mVerifier).onUpdateSelection(5, 11);
            verifyOnPopulateAccessibilityEvent(AccessibilityEvent.TYPE_VIEW_TEXT_SELECTION_CHANGED,
                    "hello world", "", 11, 5, 11, -1, -1);
            assertVerifierCallCounts(2, 3);
        }
        mInOrder.verifyNoMoreInteractions();
        assertTexts("hello", " world");
        assertTrue(mAutocomplete.shouldAutocomplete());

        // User types a space inside a batch edit.
        assertTrue(mInputConnection.beginBatchEdit());
        // We should still show the intermediate autocomplete text to the user even in the middle of
        // a batch edit. Otherwise, the user may see flickering of autocomplete text.
        assertEquals("hello world", mAutocomplete.getText().toString());
        assertTrue(mInputConnection.commitText(" ", 1));
        assertEquals("hello world", mAutocomplete.getText().toString());
        assertFalse(mAutocomplete.shouldAutocomplete());
        assertEquals("hello world", mAutocomplete.getText().toString());

        if (!isUsingSpannableModel()) {
            verifyOnPopulateAccessibilityEvent(AccessibilityEvent.TYPE_VIEW_TEXT_CHANGED,
                    "hello world", "hello", -1, 5, -1, 0, 1);
            verifyOnPopulateAccessibilityEvent(AccessibilityEvent.TYPE_VIEW_TEXT_SELECTION_CHANGED,
                    "hello world", "", 11, 6, 11, -1, -1);
            assertVerifierCallCounts(0, 2);
        }
        mInOrder.verifyNoMoreInteractions();
        assertTrue(mInputConnection.endBatchEdit());

        // Autocomplete text gets redrawn.
        assertTexts("hello ", "world");
        assertTrue(mAutocomplete.shouldAutocomplete());
        if (isUsingSpannableModel()) {
            mInOrder.verify(mVerifier).onUpdateSelection(6, 6);
            verifyOnPopulateAccessibilityEvent(AccessibilityEvent.TYPE_VIEW_TEXT_SELECTION_CHANGED,
                    "hello world", "", 11, 6, 6, -1, -1);
            verifyOnPopulateAccessibilityEvent(AccessibilityEvent.TYPE_VIEW_TEXT_CHANGED,
                    "hello world", "hello ", -1, 6, -1, 0, 5);
            mInOrder.verify(mVerifier).onAutocompleteTextStateChanged(false);
            assertVerifierCallCounts(2, 2);
        } else {
            mInOrder.verify(mVerifier).onAutocompleteTextStateChanged(true);
            mInOrder.verify(mVerifier).onUpdateSelection(6, 11);
            assertVerifierCallCounts(2, 0);
        }

        mAutocomplete.setAutocompleteText("hello ", "world");
        if (isUsingSpannableModel()) assertFalse(mAutocomplete.isCursorVisible());
        assertTexts("hello ", "world");
        assertVerifierCallCounts(0, 0);
        mInOrder.verifyNoMoreInteractions();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.SPANNABLE_INLINE_AUTOCOMPLETE)
    public void testAppend_SetComposingTextWithSpannableModel() {
        internalTestAppend_SetComposingText();
    }

    @Test
    @DisableFeatures(ChromeFeatureList.SPANNABLE_INLINE_AUTOCOMPLETE)
    public void testAppend_SetComposingTextWithoutSpannableModel() {
        internalTestAppend_SetComposingText();
    }

    private void internalTestAppend_SetComposingText() {
        // User types "h".
        assertTrue(mInputConnection.setComposingText("h", 1));

        if (isUsingSpannableModel()) {
            mInOrder.verify(mVerifier).onUpdateSelection(1, 1);
            verifyOnPopulateAccessibilityEvent(
                    AccessibilityEvent.TYPE_VIEW_TEXT_CHANGED, "h", "", -1, 0, -1, 0, 1);
            verifyOnPopulateAccessibilityEvent(
                    AccessibilityEvent.TYPE_VIEW_TEXT_SELECTION_CHANGED, "h", "", 1, 1, 1, -1, -1);
            mInOrder.verify(mVerifier).onAutocompleteTextStateChanged(false);
            assertVerifierCallCounts(2, 2);
        } else {
            mInOrder.verify(mVerifier).onAutocompleteTextStateChanged(true);
            verifyOnPopulateAccessibilityEvent(
                    AccessibilityEvent.TYPE_VIEW_TEXT_CHANGED, "h", "", -1, 0, -1, 0, 1);
            mInOrder.verify(mVerifier).onUpdateSelection(1, 1);
            verifyOnPopulateAccessibilityEvent(
                    AccessibilityEvent.TYPE_VIEW_TEXT_SELECTION_CHANGED, "h", "", 1, 1, 1, -1, -1);
            assertVerifierCallCounts(2, 2);
        }
        mInOrder.verifyNoMoreInteractions();

        // The old model does not allow autocompletion here.
        assertEquals(isUsingSpannableModel(), mAutocomplete.shouldAutocomplete());
        if (isUsingSpannableModel()) {
            // The controller kicks in.
            mAutocomplete.setAutocompleteText("h", "ello world");
            verifyOnPopulateAccessibilityEvent(AccessibilityEvent.TYPE_VIEW_TEXT_CHANGED,
                    "hello world", "h", -1, 1, -1, 0, 10);
            assertFalse(mAutocomplete.isCursorVisible());
            assertTexts("h", "ello world");
            assertVerifierCallCounts(0, 1);
        } else {
            assertTexts("h", "");
            assertVerifierCallCounts(0, 0);
        }
        mInOrder.verifyNoMoreInteractions();

        // User types "hello".
        assertTrue(mInputConnection.setComposingText("hello", 1));
        if (isUsingSpannableModel()) {
            mInOrder.verify(mVerifier).onUpdateSelection(5, 5);
            verifyOnPopulateAccessibilityEvent(AccessibilityEvent.TYPE_VIEW_TEXT_SELECTION_CHANGED,
                    "hello world", "", 11, 5, 5, -1, -1);
            verifyOnPopulateAccessibilityEvent(AccessibilityEvent.TYPE_VIEW_TEXT_CHANGED,
                    "hello world", "hello", -1, 5, -1, 0, 6);
            mInOrder.verify(mVerifier).onAutocompleteTextStateChanged(false);
            assertVerifierCallCounts(2, 2);
        } else {
            mInOrder.verify(mVerifier).onAutocompleteTextStateChanged(true);
            verifyOnPopulateAccessibilityEvent(
                    AccessibilityEvent.TYPE_VIEW_TEXT_CHANGED, "hello", "h", -1, 0, -1, 1, 5);
            mInOrder.verify(mVerifier).onUpdateSelection(5, 5);
            verifyOnPopulateAccessibilityEvent(AccessibilityEvent.TYPE_VIEW_TEXT_SELECTION_CHANGED,
                    "hello", "", 5, 5, 5, -1, -1);
            assertVerifierCallCounts(2, 2);
        }
        mInOrder.verifyNoMoreInteractions();
        if (isUsingSpannableModel()) {
            assertTexts("hello", " world");
        } else {
            // The old model does not work with composition.
            assertTexts("hello", "");
        }

        // The old model does not allow autocompletion here.
        assertEquals(isUsingSpannableModel(), mAutocomplete.shouldAutocomplete());
        if (isUsingSpannableModel()) {
            // The controller kicks in.
            mAutocomplete.setAutocompleteText("hello", " world");
            assertFalse(mAutocomplete.isCursorVisible());
            assertTexts("hello", " world");
        } else {
            assertTexts("hello", "");
        }
        assertVerifierCallCounts(0, 0);
        mInOrder.verifyNoMoreInteractions();

        // User types a space.
        assertTrue(mInputConnection.beginBatchEdit());
        // We should still show the intermediate autocomplete text to the user even in the middle of
        // a batch edit. Otherwise, the user may see flickering of autocomplete text.
        if (isUsingSpannableModel()) {
            assertEquals("hello world", mAutocomplete.getText().toString());
        }

        assertTrue(mInputConnection.finishComposingText());

        if (isUsingSpannableModel()) {
            assertEquals("hello world", mAutocomplete.getText().toString());
        }

        assertTrue(mInputConnection.commitText(" ", 1));

        if (isUsingSpannableModel()) {
            assertEquals("hello world", mAutocomplete.getText().toString());
            assertVerifierCallCounts(0, 0);
        } else {
            verifyOnPopulateAccessibilityEvent(
                    AccessibilityEvent.TYPE_VIEW_TEXT_CHANGED, "hello ", "hello", -1, 5, -1, 0, 1);
            verifyOnPopulateAccessibilityEvent(AccessibilityEvent.TYPE_VIEW_TEXT_SELECTION_CHANGED,
                    "hello ", "", 6, 6, 6, -1, -1);
            assertVerifierCallCounts(0, 2);
        }
        mInOrder.verifyNoMoreInteractions();

        assertTrue(mInputConnection.endBatchEdit());

        if (isUsingSpannableModel()) {
            mInOrder.verify(mVerifier).onUpdateSelection(6, 6);
            verifyOnPopulateAccessibilityEvent(AccessibilityEvent.TYPE_VIEW_TEXT_SELECTION_CHANGED,
                    "hello world", "", 11, 6, 6, -1, -1);
            verifyOnPopulateAccessibilityEvent(AccessibilityEvent.TYPE_VIEW_TEXT_CHANGED,
                    "hello world", "hello ", -1, 6, -1, 0, 5);
            mInOrder.verify(mVerifier).onAutocompleteTextStateChanged(false);
            assertVerifierCallCounts(2, 2);
        } else {
            mInOrder.verify(mVerifier).onAutocompleteTextStateChanged(true);
            mInOrder.verify(mVerifier).onUpdateSelection(6, 6);
            assertVerifierCallCounts(2, 0);
        }
        mInOrder.verifyNoMoreInteractions();

        if (isUsingSpannableModel()) {
            // Autocomplete text has been drawn at endBatchEdit().
            assertTexts("hello ", "world");
        } else {
            assertTexts("hello ", "");
        }

        // The old model can also autocomplete now.
        assertTrue(mAutocomplete.shouldAutocomplete());
        mAutocomplete.setAutocompleteText("hello ", "world");
        assertTexts("hello ", "world");
        if (isUsingSpannableModel()) {
            assertFalse(mAutocomplete.isCursorVisible());
            assertVerifierCallCounts(0, 0);
        } else {
            verifyOnPopulateAccessibilityEvent(AccessibilityEvent.TYPE_VIEW_TEXT_CHANGED,
                    "hello world", "hello ", -1, 6, -1, 0, 5);
            mInOrder.verify(mVerifier).onUpdateSelection(11, 11);
            verifyOnPopulateAccessibilityEvent(AccessibilityEvent.TYPE_VIEW_TEXT_SELECTION_CHANGED,
                    "hello world", "", 11, 11, 11, -1, -1);
            mInOrder.verify(mVerifier).onUpdateSelection(6, 11);
            verifyOnPopulateAccessibilityEvent(AccessibilityEvent.TYPE_VIEW_TEXT_SELECTION_CHANGED,
                    "hello world", "", 11, 6, 11, -1, -1);
            assertVerifierCallCounts(2, 3);
        }
        mInOrder.verifyNoMoreInteractions();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.SPANNABLE_INLINE_AUTOCOMPLETE)
    public void testAppend_DispatchKeyEventWithSpannableModel() {
        internalTestAppend_DispatchKeyEvent();
    }

    @Test
    @DisableFeatures(ChromeFeatureList.SPANNABLE_INLINE_AUTOCOMPLETE)
    public void testAppend_DispatchKeyEventWithoutSpannableModel() {
        internalTestAppend_DispatchKeyEvent();
    }

    private void internalTestAppend_DispatchKeyEvent() {
        // User types "h".
        mAutocomplete.dispatchKeyEvent(new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_H));
        mAutocomplete.dispatchKeyEvent(new KeyEvent(KeyEvent.ACTION_UP, KeyEvent.KEYCODE_H));
        if (isUsingSpannableModel()) {
            mInOrder.verify(mVerifier).onUpdateSelection(1, 1);
            verifyOnPopulateAccessibilityEvent(
                    AccessibilityEvent.TYPE_VIEW_TEXT_CHANGED, "h", "", -1, 0, -1, 0, 1);
            verifyOnPopulateAccessibilityEvent(
                    AccessibilityEvent.TYPE_VIEW_TEXT_SELECTION_CHANGED, "h", "", 1, 1, 1, -1, -1);
            mInOrder.verify(mVerifier).onAutocompleteTextStateChanged(false);
            assertVerifierCallCounts(2, 2);
        } else {
            mInOrder.verify(mVerifier).onAutocompleteTextStateChanged(true);
            verifyOnPopulateAccessibilityEvent(
                    AccessibilityEvent.TYPE_VIEW_TEXT_CHANGED, "h", "", -1, 0, -1, 0, 1);
            mInOrder.verify(mVerifier).onUpdateSelection(1, 1);
            verifyOnPopulateAccessibilityEvent(
                    AccessibilityEvent.TYPE_VIEW_TEXT_SELECTION_CHANGED, "h", "", 1, 1, 1, -1, -1);
            assertVerifierCallCounts(2, 2);
        }
        mInOrder.verifyNoMoreInteractions();
        assertTrue(mAutocomplete.shouldAutocomplete());

        // The controller kicks in.
        mAutocomplete.setAutocompleteText("h", "ello world");
        // The non-spannable model changes selection in two steps.
        if (isUsingSpannableModel()) {
            verifyOnPopulateAccessibilityEvent(AccessibilityEvent.TYPE_VIEW_TEXT_CHANGED,
                    "hello world", "h", -1, 1, -1, 0, 10);
            assertVerifierCallCounts(0, 1);
            assertFalse(mAutocomplete.isCursorVisible());
        } else {
            verifyOnPopulateAccessibilityEvent(AccessibilityEvent.TYPE_VIEW_TEXT_CHANGED,
                    "hello world", "h", -1, 1, -1, 0, 10);
            mInOrder.verify(mVerifier).onUpdateSelection(11, 11);
            verifyOnPopulateAccessibilityEvent(AccessibilityEvent.TYPE_VIEW_TEXT_SELECTION_CHANGED,
                    "hello world", "", 11, 11, 11, -1, -1);
            mInOrder.verify(mVerifier).onUpdateSelection(1, 11);
            verifyOnPopulateAccessibilityEvent(AccessibilityEvent.TYPE_VIEW_TEXT_SELECTION_CHANGED,
                    "hello world", "", 11, 1, 11, -1, -1);
            assertVerifierCallCounts(2, 3);
        }
        mInOrder.verifyNoMoreInteractions();
        assertTrue(mAutocomplete.shouldAutocomplete());

        // User types "he".
        mAutocomplete.dispatchKeyEvent(new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_E));
        mAutocomplete.dispatchKeyEvent(new KeyEvent(KeyEvent.ACTION_UP, KeyEvent.KEYCODE_E));
        if (isUsingSpannableModel()) {
            mInOrder.verify(mVerifier).onUpdateSelection(2, 2);
            verifyOnPopulateAccessibilityEvent(AccessibilityEvent.TYPE_VIEW_TEXT_SELECTION_CHANGED,
                    "hello world", "", 11, 2, 2, -1, -1);
            verifyOnPopulateAccessibilityEvent(AccessibilityEvent.TYPE_VIEW_TEXT_CHANGED,
                    "hello world", "he", -1, 2, -1, 0, 9);
            mInOrder.verify(mVerifier).onAutocompleteTextStateChanged(false);
            // The new model tries to reuse autocomplete text.
            assertTexts("he", "llo world");
            assertVerifierCallCounts(2, 2);
        } else {
            mInOrder.verify(mVerifier).onAutocompleteTextStateChanged(false);
            mInOrder.verify(mVerifier).onUpdateSelection(11, 11);
            verifyOnPopulateAccessibilityEvent(AccessibilityEvent.TYPE_VIEW_TEXT_SELECTION_CHANGED,
                    "hello world", "", 11, 11, 11, -1, -1);
            mInOrder.verify(mVerifier).onAutocompleteTextStateChanged(true);
            verifyOnPopulateAccessibilityEvent(AccessibilityEvent.TYPE_VIEW_TEXT_CHANGED, "he",
                    "hello world", -1, 1, -1, 10, 1);
            mInOrder.verify(mVerifier).onUpdateSelection(2, 2);
            verifyOnPopulateAccessibilityEvent(
                    AccessibilityEvent.TYPE_VIEW_TEXT_SELECTION_CHANGED, "he", "", 2, 2, 2, -1, -1);
            assertVerifierCallCounts(4, 3);
        }
        mInOrder.verifyNoMoreInteractions();
        assertTrue(mAutocomplete.shouldAutocomplete());
        // The controller kicks in.
        mAutocomplete.setAutocompleteText("he", "llo world");
        if (isUsingSpannableModel()) {
            assertFalse(mAutocomplete.isCursorVisible());
            assertVerifierCallCounts(0, 0);
        } else {
            verifyOnPopulateAccessibilityEvent(AccessibilityEvent.TYPE_VIEW_TEXT_CHANGED,
                    "hello world", "he", -1, 2, -1, 0, 9);
            mInOrder.verify(mVerifier).onUpdateSelection(11, 11);
            verifyOnPopulateAccessibilityEvent(AccessibilityEvent.TYPE_VIEW_TEXT_SELECTION_CHANGED,
                    "hello world", "", 11, 11, 11, -1, -1);
            mInOrder.verify(mVerifier).onUpdateSelection(2, 11);
            verifyOnPopulateAccessibilityEvent(AccessibilityEvent.TYPE_VIEW_TEXT_SELECTION_CHANGED,
                    "hello world", "", 11, 2, 11, -1, -1);
            assertVerifierCallCounts(2, 3);
        }
        mInOrder.verifyNoMoreInteractions();
        assertTexts("he", "llo world");
        assertTrue(mAutocomplete.shouldAutocomplete());
        mInOrder.verifyNoMoreInteractions();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.SPANNABLE_INLINE_AUTOCOMPLETE)
    public void testDelete_CommitTextWithSpannableModel() {
        internalTestDelete_CommitText();
    }

    @Test
    @DisableFeatures(ChromeFeatureList.SPANNABLE_INLINE_AUTOCOMPLETE)
    public void testDelete_CommitTextWithoutSpannableModel() {
        internalTestDelete_CommitText();
    }

    private void internalTestDelete_CommitText() {
        // User types "hello".
        assertTrue(mInputConnection.commitText("hello", 1));
        if (isUsingSpannableModel()) {
            mInOrder.verify(mVerifier).onUpdateSelection(5, 5);
            verifyOnPopulateAccessibilityEvent(
                    AccessibilityEvent.TYPE_VIEW_TEXT_CHANGED, "hello", "", -1, 0, -1, 0, 5);
            verifyOnPopulateAccessibilityEvent(AccessibilityEvent.TYPE_VIEW_TEXT_SELECTION_CHANGED,
                    "hello", "", 5, 5, 5, -1, -1);
            mInOrder.verify(mVerifier).onAutocompleteTextStateChanged(false);
            assertVerifierCallCounts(2, 2);
        } else {
            mInOrder.verify(mVerifier).onAutocompleteTextStateChanged(true);
            verifyOnPopulateAccessibilityEvent(
                    AccessibilityEvent.TYPE_VIEW_TEXT_CHANGED, "hello", "", -1, 0, -1, 0, 5);
            mInOrder.verify(mVerifier).onUpdateSelection(5, 5);
            verifyOnPopulateAccessibilityEvent(AccessibilityEvent.TYPE_VIEW_TEXT_SELECTION_CHANGED,
                    "hello", "", 5, 5, 5, -1, -1);
            assertVerifierCallCounts(2, 2);
        }
        mInOrder.verifyNoMoreInteractions();
        assertTrue(mAutocomplete.shouldAutocomplete());
        // The controller kicks in.
        mAutocomplete.setAutocompleteText("hello", " world");
        if (isUsingSpannableModel()) {
            verifyOnPopulateAccessibilityEvent(AccessibilityEvent.TYPE_VIEW_TEXT_CHANGED,
                    "hello world", "hello", -1, 5, -1, 0, 6);
            assertVerifierCallCounts(0, 1);
            assertFalse(mAutocomplete.isCursorVisible());
        } else {
            verifyOnPopulateAccessibilityEvent(AccessibilityEvent.TYPE_VIEW_TEXT_CHANGED,
                    "hello world", "hello", -1, 5, -1, 0, 6);
            mInOrder.verify(mVerifier).onUpdateSelection(11, 11);
            verifyOnPopulateAccessibilityEvent(AccessibilityEvent.TYPE_VIEW_TEXT_SELECTION_CHANGED,
                    "hello world", "", 11, 11, 11, -1, -1);
            mInOrder.verify(mVerifier).onUpdateSelection(5, 11);
            verifyOnPopulateAccessibilityEvent(AccessibilityEvent.TYPE_VIEW_TEXT_SELECTION_CHANGED,
                    "hello world", "", 11, 5, 11, -1, -1);
            assertVerifierCallCounts(2, 3);
        }
        assertTexts("hello", " world");
        mInOrder.verifyNoMoreInteractions();

        // User deletes autocomplete.
        if (isUsingSpannableModel()) {
            assertTrue(mInputConnection.deleteSurroundingText(1, 0)); // deletes one character
        } else {
            assertTrue(mInputConnection.commitText("", 1)); // deletes selection.
        }

        if (isUsingSpannableModel()) {
            // Pretend that we have deleted 'o' first.
            mInOrder.verify(mVerifier).onUpdateSelection(4, 4);
            // We restore 'o', and clear autocomplete text instead.
            mInOrder.verify(mVerifier).onUpdateSelection(5, 5);
            assertTrue(mAutocomplete.isCursorVisible());
            // Autocomplete removed.
            verifyOnPopulateAccessibilityEvent(AccessibilityEvent.TYPE_VIEW_TEXT_CHANGED, "hello",
                    "hello world", -1, 5, -1, 6, 0);
            mInOrder.verify(mVerifier).onAutocompleteTextStateChanged(false);
            assertVerifierCallCounts(3, 1);
        } else {
            mInOrder.verify(mVerifier).onAutocompleteTextStateChanged(false);
            mInOrder.verify(mVerifier).onUpdateSelection(11, 11);
            verifyOnPopulateAccessibilityEvent(AccessibilityEvent.TYPE_VIEW_TEXT_SELECTION_CHANGED,
                    "hello world", "", 11, 11, 11, -1, -1);
            mInOrder.verify(mVerifier).onAutocompleteTextStateChanged(true);
            verifyOnPopulateAccessibilityEvent(AccessibilityEvent.TYPE_VIEW_TEXT_CHANGED, "hello",
                    "hello world", -1, 5, -1, 6, 0);
            mInOrder.verify(mVerifier).onUpdateSelection(5, 5);
            verifyOnPopulateAccessibilityEvent(AccessibilityEvent.TYPE_VIEW_TEXT_SELECTION_CHANGED,
                    "hello", "", 5, 5, 5, -1, -1);
            assertVerifierCallCounts(4, 3);
        }
        mInOrder.verifyNoMoreInteractions();
        assertFalse(mAutocomplete.shouldAutocomplete());
        assertTexts("hello", "");

        // Keyboard app checks the current state.
        assertEquals("hello", mInputConnection.getTextBeforeCursor(10, 0));
        assertTrue(mAutocomplete.isCursorVisible());
        assertVerifierCallCounts(0, 0);
        mInOrder.verifyNoMoreInteractions();
        assertFalse(mAutocomplete.shouldAutocomplete());
        assertTexts("hello", "");
    }

    private boolean isComposing() {
        return BaseInputConnection.getComposingSpanStart(mAutocomplete.getText())
                != BaseInputConnection.getComposingSpanEnd(mAutocomplete.getText());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.SPANNABLE_INLINE_AUTOCOMPLETE)
    public void testDelete_SetComposingTextWithSpannableModel() {
        // User types "hello".
        assertTrue(mInputConnection.setComposingText("hello", 1));
        assertTrue(isComposing());
        mInOrder.verify(mVerifier).onUpdateSelection(5, 5);
        verifyOnPopulateAccessibilityEvent(
                AccessibilityEvent.TYPE_VIEW_TEXT_CHANGED, "hello", "", -1, 0, -1, 0, 5);
        verifyOnPopulateAccessibilityEvent(
                AccessibilityEvent.TYPE_VIEW_TEXT_SELECTION_CHANGED, "hello", "", 5, 5, 5, -1, -1);
        mInOrder.verify(mVerifier).onAutocompleteTextStateChanged(false);
        assertVerifierCallCounts(2, 2);
        mInOrder.verifyNoMoreInteractions();
        assertTrue(mAutocomplete.shouldAutocomplete());
        // The controller kicks in.
        mAutocomplete.setAutocompleteText("hello", " world");
        verifyOnPopulateAccessibilityEvent(
                AccessibilityEvent.TYPE_VIEW_TEXT_CHANGED, "hello world", "hello", -1, 5, -1, 0, 6);
        assertFalse(mAutocomplete.isCursorVisible());
        assertTexts("hello", " world");
        assertVerifierCallCounts(0, 1);
        mInOrder.verifyNoMoreInteractions();

        // User deletes autocomplete.
        assertTrue(mInputConnection.setComposingText("hell", 1));
        // Pretend that we have deleted 'o'.
        mInOrder.verify(mVerifier).onUpdateSelection(4, 4);
        // We restore 'o', finish composition, and clear autocomplete text instead.
        mInOrder.verify(mVerifier).onUpdateSelection(5, 5);
        assertTrue(mAutocomplete.isCursorVisible());
        assertFalse(isComposing());
        // Remove autocomplete.
        verifyOnPopulateAccessibilityEvent(
                AccessibilityEvent.TYPE_VIEW_TEXT_CHANGED, "hello", "hello world", -1, 5, -1, 6, 0);
        mInOrder.verify(mVerifier).onAutocompleteTextStateChanged(false);
        assertVerifierCallCounts(3, 1);
        mInOrder.verifyNoMoreInteractions();
        assertFalse(mAutocomplete.shouldAutocomplete());
        assertTexts("hello", "");

        // Keyboard app checks the current state.
        assertEquals("hello", mInputConnection.getTextBeforeCursor(10, 0));
        assertTrue(mAutocomplete.isCursorVisible());
        assertVerifierCallCounts(0, 0);
        mInOrder.verifyNoMoreInteractions();
        assertFalse(mAutocomplete.shouldAutocomplete());
        assertTexts("hello", "");
    }

    @Test
    @EnableFeatures(ChromeFeatureList.SPANNABLE_INLINE_AUTOCOMPLETE)
    public void testDelete_SamsungKeyboardWithSpannableModel() {
        mAutocomplete.setKeyboardPackageName("com.sec.android.inputmethod");
        // User types "hello".
        assertTrue(mInputConnection.setComposingText("hello", 1));
        assertTrue(isComposing());
        assertTrue(mAutocomplete.shouldAutocomplete());
        // The controller kicks in.
        mAutocomplete.setAutocompleteText("hello", " world");
        assertTexts("hello", " world");

        // User deletes autocomplete.
        assertTrue(mInputConnection.setComposingText("hell", 1));
        // Remove autocomplete.
        assertFalse(mAutocomplete.shouldAutocomplete());
        assertTexts("hello", "");
        // Make sure that we do not finish composing text for Samsung keyboard - it does not update
        // its internal states when we ask this. (crbug.com/766888).
        assertTrue(isComposing());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.SPANNABLE_INLINE_AUTOCOMPLETE)
    public void testDelete_SetComposingTextInBatchEditWithSpannableModel() {
        // User types "hello".
        assertTrue(mInputConnection.setComposingText("hello", 1));

        mInOrder.verify(mVerifier).onUpdateSelection(5, 5);
        verifyOnPopulateAccessibilityEvent(
                AccessibilityEvent.TYPE_VIEW_TEXT_CHANGED, "hello", "", -1, 0, -1, 0, 5);
        verifyOnPopulateAccessibilityEvent(
                AccessibilityEvent.TYPE_VIEW_TEXT_SELECTION_CHANGED, "hello", "", 5, 5, 5, -1, -1);
        mInOrder.verify(mVerifier).onAutocompleteTextStateChanged(false);
        mInOrder.verifyNoMoreInteractions();
        assertVerifierCallCounts(2, 2);
        assertTrue(mAutocomplete.shouldAutocomplete());
        // The controller kicks in.
        mAutocomplete.setAutocompleteText("hello", " world");
        assertFalse(mAutocomplete.isCursorVisible());
        assertTexts("hello", " world");
        verifyOnPopulateAccessibilityEvent(
                AccessibilityEvent.TYPE_VIEW_TEXT_CHANGED, "hello world", "hello", -1, 5, -1, 0, 6);
        mInOrder.verifyNoMoreInteractions();
        assertVerifierCallCounts(0, 1);

        // User deletes 'o' in a batch edit.
        assertTrue(mInputConnection.beginBatchEdit());
        assertTrue(mInputConnection.setComposingText("hell", 1));

        // We restore 'o', and clear autocomplete text instead.
        assertTrue(mAutocomplete.isCursorVisible());
        assertFalse(mAutocomplete.shouldAutocomplete());

        // The user will see "hello" even in the middle of a batch edit.
        assertEquals("hello", mAutocomplete.getText().toString());

        // Keyboard app checks the current state.
        assertEquals("hell", mInputConnection.getTextBeforeCursor(10, 0));
        assertTrue(mAutocomplete.isCursorVisible());
        assertFalse(mAutocomplete.shouldAutocomplete());

        mInOrder.verifyNoMoreInteractions();
        assertVerifierCallCounts(0, 0);

        assertTrue(mInputConnection.endBatchEdit());
        mInOrder.verify(mVerifier).onUpdateSelection(4, 4);
        mInOrder.verify(mVerifier).onUpdateSelection(5, 5);
        verifyOnPopulateAccessibilityEvent(
                AccessibilityEvent.TYPE_VIEW_TEXT_CHANGED, "hello", "hello world", -1, 5, -1, 6, 0);
        mInOrder.verify(mVerifier).onAutocompleteTextStateChanged(false);
        assertFalse(mAutocomplete.shouldAutocomplete());
        assertTexts("hello", "");
        mInOrder.verifyNoMoreInteractions();
        assertVerifierCallCounts(3, 1);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.SPANNABLE_INLINE_AUTOCOMPLETE)
    public void testSelect_SelectAutocompleteWithSpannableModel() {
        internalTestSelect_SelectAutocomplete();
    }

    @Test
    @DisableFeatures(ChromeFeatureList.SPANNABLE_INLINE_AUTOCOMPLETE)
    public void testSelect_SelectAutocompleteWithoutSpannableModel() {
        internalTestSelect_SelectAutocomplete();
    }

    private void internalTestSelect_SelectAutocomplete() {
        // User types "hello".
        assertTrue(mInputConnection.commitText("hello", 1));
        if (isUsingSpannableModel()) {
            mInOrder.verify(mVerifier).onUpdateSelection(5, 5);
            verifyOnPopulateAccessibilityEvent(
                    AccessibilityEvent.TYPE_VIEW_TEXT_CHANGED, "hello", "", -1, 0, -1, 0, 5);
            verifyOnPopulateAccessibilityEvent(AccessibilityEvent.TYPE_VIEW_TEXT_SELECTION_CHANGED,
                    "hello", "", 5, 5, 5, -1, -1);
            mInOrder.verify(mVerifier).onAutocompleteTextStateChanged(false);
            assertVerifierCallCounts(2, 2);
        } else {
            mInOrder.verify(mVerifier).onAutocompleteTextStateChanged(true);
            verifyOnPopulateAccessibilityEvent(
                    AccessibilityEvent.TYPE_VIEW_TEXT_CHANGED, "hello", "", -1, 0, -1, 0, 5);
            mInOrder.verify(mVerifier).onUpdateSelection(5, 5);
            verifyOnPopulateAccessibilityEvent(AccessibilityEvent.TYPE_VIEW_TEXT_SELECTION_CHANGED,
                    "hello", "", 5, 5, 5, -1, -1);
            assertVerifierCallCounts(2, 2);
        }
        mInOrder.verifyNoMoreInteractions();
        assertTrue(mAutocomplete.shouldAutocomplete());
        // The controller kicks in.
        mAutocomplete.setAutocompleteText("hello", " world");
        assertTexts("hello", " world");
        if (isUsingSpannableModel()) {
            verifyOnPopulateAccessibilityEvent(AccessibilityEvent.TYPE_VIEW_TEXT_CHANGED,
                    "hello world", "hello", -1, 5, -1, 0, 6);
            assertFalse(mAutocomplete.isCursorVisible());
            assertVerifierCallCounts(0, 1);
        } else {
            verifyOnPopulateAccessibilityEvent(AccessibilityEvent.TYPE_VIEW_TEXT_CHANGED,
                    "hello world", "hello", -1, 5, -1, 0, 6);
            mInOrder.verify(mVerifier).onUpdateSelection(11, 11);
            verifyOnPopulateAccessibilityEvent(AccessibilityEvent.TYPE_VIEW_TEXT_SELECTION_CHANGED,
                    "hello world", "", 11, 11, 11, -1, -1);
            mInOrder.verify(mVerifier).onUpdateSelection(5, 11);
            verifyOnPopulateAccessibilityEvent(AccessibilityEvent.TYPE_VIEW_TEXT_SELECTION_CHANGED,
                    "hello world", "", 11, 5, 11, -1, -1);
            assertVerifierCallCounts(2, 3);
        }
        mInOrder.verifyNoMoreInteractions();
        // User touches autocomplete text.
        mAutocomplete.setSelection(7);
        if (isUsingSpannableModel()) {
            mInOrder.verify(mVerifier).onUpdateSelection(7, 7);
            verifyOnPopulateAccessibilityEvent(AccessibilityEvent.TYPE_VIEW_TEXT_SELECTION_CHANGED,
                    "hello world", "", 11, 7, 7, -1, -1);
            mInOrder.verify(mVerifier).onAutocompleteTextStateChanged(false);
        } else {
            mInOrder.verify(mVerifier).onAutocompleteTextStateChanged(false);
            mInOrder.verify(mVerifier).onUpdateSelection(7, 7);
            verifyOnPopulateAccessibilityEvent(AccessibilityEvent.TYPE_VIEW_TEXT_SELECTION_CHANGED,
                    "hello world", "", 11, 7, 7, -1, -1);
        }
        assertVerifierCallCounts(2, 1);
        mInOrder.verifyNoMoreInteractions();
        assertFalse(mAutocomplete.shouldAutocomplete());
        assertTexts("hello world", "");
    }

    @Test
    @EnableFeatures(ChromeFeatureList.SPANNABLE_INLINE_AUTOCOMPLETE)
    public void testSelect_SelectUserTextWithSpannableModel() {
        internalTestSelect_SelectUserText();
    }

    @Test
    @DisableFeatures(ChromeFeatureList.SPANNABLE_INLINE_AUTOCOMPLETE)
    public void testSelect_SelectUserTextWithoutSpannableModel() {
        internalTestSelect_SelectUserText();
    }

    private void internalTestSelect_SelectUserText() {
        // User types "hello".
        assertTrue(mInputConnection.commitText("hello", 1));
        if (isUsingSpannableModel()) {
            mInOrder.verify(mVerifier).onUpdateSelection(5, 5);
            verifyOnPopulateAccessibilityEvent(
                    AccessibilityEvent.TYPE_VIEW_TEXT_CHANGED, "hello", "", -1, 0, -1, 0, 5);
            verifyOnPopulateAccessibilityEvent(AccessibilityEvent.TYPE_VIEW_TEXT_SELECTION_CHANGED,
                    "hello", "", 5, 5, 5, -1, -1);
            mInOrder.verify(mVerifier).onAutocompleteTextStateChanged(false);
            assertVerifierCallCounts(2, 2);
        } else {
            mInOrder.verify(mVerifier).onAutocompleteTextStateChanged(true);
            verifyOnPopulateAccessibilityEvent(
                    AccessibilityEvent.TYPE_VIEW_TEXT_CHANGED, "hello", "", -1, 0, -1, 0, 5);
            mInOrder.verify(mVerifier).onUpdateSelection(5, 5);
            verifyOnPopulateAccessibilityEvent(AccessibilityEvent.TYPE_VIEW_TEXT_SELECTION_CHANGED,
                    "hello", "", 5, 5, 5, -1, -1);
            assertVerifierCallCounts(2, 2);
        }
        mInOrder.verifyNoMoreInteractions();
        assertTrue(mAutocomplete.shouldAutocomplete());
        // The controller kicks in.
        mAutocomplete.setAutocompleteText("hello", " world");
        assertTexts("hello", " world");
        if (isUsingSpannableModel()) {
            verifyOnPopulateAccessibilityEvent(AccessibilityEvent.TYPE_VIEW_TEXT_CHANGED,
                    "hello world", "hello", -1, 5, -1, 0, 6);
            assertFalse(mAutocomplete.isCursorVisible());
            assertVerifierCallCounts(0, 1);
        } else {
            verifyOnPopulateAccessibilityEvent(AccessibilityEvent.TYPE_VIEW_TEXT_CHANGED,
                    "hello world", "hello", -1, 5, -1, 0, 6);
            mInOrder.verify(mVerifier).onUpdateSelection(11, 11);
            verifyOnPopulateAccessibilityEvent(AccessibilityEvent.TYPE_VIEW_TEXT_SELECTION_CHANGED,
                    "hello world", "", 11, 11, 11, -1, -1);
            mInOrder.verify(mVerifier).onUpdateSelection(5, 11);
            verifyOnPopulateAccessibilityEvent(AccessibilityEvent.TYPE_VIEW_TEXT_SELECTION_CHANGED,
                    "hello world", "", 11, 5, 11, -1, -1);
            assertVerifierCallCounts(2, 3);
        }
        mInOrder.verifyNoMoreInteractions();
        // User touches the user text.
        mAutocomplete.setSelection(3);
        if (isUsingSpannableModel()) {
            assertTrue(mAutocomplete.isCursorVisible());
            mInOrder.verify(mVerifier).onUpdateSelection(3, 3);
            verifyOnPopulateAccessibilityEvent(AccessibilityEvent.TYPE_VIEW_TEXT_CHANGED, "hello",
                    "hello world", -1, 5, -1, 6, 0);
            verifyOnPopulateAccessibilityEvent(AccessibilityEvent.TYPE_VIEW_TEXT_SELECTION_CHANGED,
                    "hello", "", 5, 3, 3, -1, -1);
            mInOrder.verify(mVerifier).onAutocompleteTextStateChanged(false);
            assertVerifierCallCounts(2, 2);
        } else {
            mInOrder.verify(mVerifier).onAutocompleteTextStateChanged(true);
            verifyOnPopulateAccessibilityEvent(AccessibilityEvent.TYPE_VIEW_TEXT_CHANGED, "hello",
                    "hello world", -1, 5, -1, 6, 0);
            mInOrder.verify(mVerifier).onAutocompleteTextStateChanged(false);
            mInOrder.verify(mVerifier).onUpdateSelection(3, 3);
            verifyOnPopulateAccessibilityEvent(AccessibilityEvent.TYPE_VIEW_TEXT_SELECTION_CHANGED,
                    "hello", "", 5, 3, 3, -1, -1);
            assertVerifierCallCounts(3, 2);
        }
        mInOrder.verifyNoMoreInteractions();
        assertFalse(mAutocomplete.shouldAutocomplete());
        // Autocomplete text is removed.
        assertTexts("hello", "");
    }

    @Test
    @EnableFeatures(ChromeFeatureList.SPANNABLE_INLINE_AUTOCOMPLETE)
    public void testAppend_AfterSelectAllWithSpannableModel() {
        internalTestAppend_AfterSelectAll();
    }

    @Test
    @DisableFeatures(ChromeFeatureList.SPANNABLE_INLINE_AUTOCOMPLETE)
    public void testAppend_AfterSelectAllWithoutSpannableModel() {
        internalTestAppend_AfterSelectAll();
    }

    private void internalTestAppend_AfterSelectAll() {
        final String url = "https://www.google.com/";
        mAutocomplete.setText(url);
        mAutocomplete.setSelection(0, url.length());
        assertTrue(mAutocomplete.isCursorVisible());
        // User types "h" - note that this is also starting character of the URL. The selection gets
        // replaced by what user types.
        assertTrue(mInputConnection.commitText("h", 1));
        // We want to allow inline autocomplete when the user overrides an existing URL.
        assertTrue(mAutocomplete.shouldAutocomplete());
        assertTrue(mAutocomplete.isCursorVisible());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.SPANNABLE_INLINE_AUTOCOMPLETE)
    public void testIgnoreAndGetWithSpannableModel() {
        internalTestIgnoreAndGet();
    }

    @Test
    @DisableFeatures(ChromeFeatureList.SPANNABLE_INLINE_AUTOCOMPLETE)
    public void testIgnoreAndGetWithoutSpannableModel() {
        internalTestIgnoreAndGet();
    }

    private void internalTestIgnoreAndGet() {
        final String url = "https://www.google.com/";
        mAutocomplete.setIgnoreTextChangesForAutocomplete(true);
        mAutocomplete.setText(url);
        mAutocomplete.setIgnoreTextChangesForAutocomplete(false);
        mInputConnection.getTextBeforeCursor(1, 1);
        if (isUsingSpannableModel()) {
            assertTrue(mAutocomplete.isCursorVisible());
        }
        mInOrder.verifyNoMoreInteractions();
    }

    // crbug.com/760013
    @Test
    @EnableFeatures(ChromeFeatureList.SPANNABLE_INLINE_AUTOCOMPLETE)
    public void testOnSaveInstanceStateDoesNotCrash() {
        mInputConnection.setComposingText("h", 1);
        mAutocomplete.setAutocompleteText("h", "ello world");
        // On Android JB, TextView#onSaveInstanceState() calls new SpannableString(mText). This
        // should not crash.
        new SpannableString(mAutocomplete.getText());
    }

    // crbug.com/759876
    @Test
    @EnableFeatures(ChromeFeatureList.SPANNABLE_INLINE_AUTOCOMPLETE)
    public void testFocusInAndSelectAllWithSpannableModel() {
        internalTestFocusInAndSelectAll();
    }

    // crbug.com/759876
    @Test
    @DisableFeatures(ChromeFeatureList.SPANNABLE_INLINE_AUTOCOMPLETE)
    public void testFocusInAndSelectAllWithoutSpannableModel() {
        internalTestFocusInAndSelectAll();
    }

    private void internalTestFocusInAndSelectAll() {
        final String url = "https://google.com";
        final int len = url.length();
        mAutocomplete.setIgnoreTextChangesForAutocomplete(true);
        mAutocomplete.setText(url);
        mAutocomplete.setIgnoreTextChangesForAutocomplete(false);

        mInOrder.verifyNoMoreInteractions();
        assertVerifierCallCounts(0, 0);

        assertTrue(mFocusPlaceHolder.requestFocus());

        mInOrder.verifyNoMoreInteractions();
        assertVerifierCallCounts(0, 0);

        // LocationBarLayout does this.
        mAutocomplete.setSelectAllOnFocus(true);

        assertTrue(mAutocomplete.requestFocus());

        mInOrder.verify(mVerifier).onUpdateSelection(len, len);
        verifyOnPopulateAccessibilityEvent(
                AccessibilityEvent.TYPE_VIEW_TEXT_SELECTION_CHANGED, url, "", 18, 18, 18, -1, -1);
        mInOrder.verify(mVerifier).onUpdateSelection(0, len);
        verifyOnPopulateAccessibilityEvent(
                AccessibilityEvent.TYPE_VIEW_TEXT_SELECTION_CHANGED, url, "", 18, 0, 18, -1, -1);
        verifyOnPopulateAccessibilityEvent(
                AccessibilityEvent.TYPE_VIEW_FOCUSED, url, "", 2, -1, -1, -1, -1);

        assertVerifierCallCounts(2, 3);
        mInOrder.verifyNoMoreInteractions();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.SPANNABLE_INLINE_AUTOCOMPLETE)
    public void testNonMatchingBatchEditWithSpannableModel() {
        internalNonMatchingBatchEdit();
    }

    @Test
    @DisableFeatures(ChromeFeatureList.SPANNABLE_INLINE_AUTOCOMPLETE)
    public void testNonMatchingBatchEditWithoutSpannableModel() {
        internalNonMatchingBatchEdit();
    }

    // crbug.com/764749
    private void internalNonMatchingBatchEdit() {
        // beginBatchEdit() was not matched by endBatchEdit(), for some reason.
        mInputConnection.beginBatchEdit();

        // Restart input should reset batch edit count.
        assertNotNull(mAutocomplete.onCreateInputConnection(new EditorInfo()));
        mInputConnection = mAutocomplete.getInputConnection();

        assertTrue(mInputConnection.commitText("a", 1));
        // Works again.
        assertTrue(mAutocomplete.shouldAutocomplete());
    }

    // crbug.com/768323
    @Test
    @EnableFeatures(ChromeFeatureList.SPANNABLE_INLINE_AUTOCOMPLETE)
    public void testFocusLossHidesCursorWithSpannableModel() {
        assertTrue(mAutocomplete.isFocused());
        assertTrue(mAutocomplete.isCursorVisible());

        // AutocompleteEditText loses focus, and this hides cursor.
        assertTrue(mFocusPlaceHolder.requestFocus());

        assertFalse(mAutocomplete.isFocused());
        assertFalse(mAutocomplete.isCursorVisible());

        // Some IME operations may arrive after focus loss, but this should never show cursor.
        mInputConnection.getTextBeforeCursor(1, 0);
        assertFalse(mAutocomplete.isCursorVisible());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.SPANNABLE_INLINE_AUTOCOMPLETE)
    public void testBlacklistWithSpannableModel() {
        mAutocomplete.setKeyboardPackageName("jp.co.sharp.android.iwnn");
        // User types "h".
        assertTrue(mInputConnection.commitText("h", 1));
        assertFalse(mAutocomplete.shouldAutocomplete());
    }

    // crbug.com/783165
    @Test
    @EnableFeatures(ChromeFeatureList.SPANNABLE_INLINE_AUTOCOMPLETE)
    public void testSetTextAndSelect() {
        // User types "h".
        assertTrue(mInputConnection.commitText("h", 1));
        assertTrue(mAutocomplete.shouldAutocomplete());
        mAutocomplete.setAutocompleteText("h", "ello world");
        mAutocomplete.setIgnoreTextChangesForAutocomplete(true);
        mAutocomplete.setText("abcde");
        mAutocomplete.setIgnoreTextChangesForAutocomplete(false);
        assertEquals("abcde", mAutocomplete.getText().toString());

        mAutocomplete.setSelection(0);

        // Check the internal states are correct.
        assertEquals("abcde", mAutocomplete.getText().toString());
        assertEquals("abcde", mAutocomplete.getTextWithAutocomplete());
        assertEquals("abcde", mAutocomplete.getTextWithoutAutocomplete());
    }

    // crbug.com/810704
    @Test
    @EnableFeatures(ChromeFeatureList.SPANNABLE_INLINE_AUTOCOMPLETE)
    public void testPerformEditorAction() {
        // User types "goo".
        assertTrue(mInputConnection.setComposingText("goo", 1));
        assertTrue(mAutocomplete.shouldAutocomplete());
        mAutocomplete.setAutocompleteText("goo", "gle.com");
        assertEquals("google.com", mAutocomplete.getText().toString());

        // User presses 'GO' key on the keyboard.
        assertTrue(mInputConnection.commitText("goo", 1));
        assertEquals("google.com", mAutocomplete.getText().toString());

        assertTrue(mInputConnection.performEditorAction(EditorInfo.IME_ACTION_GO));
        assertEquals("google.com", mAutocomplete.getText().toString());
    }

    // crbug.com/810704
    @Test
    @EnableFeatures(ChromeFeatureList.SPANNABLE_INLINE_AUTOCOMPLETE)
    public void testPerformEditorActionInBatchEdit() {
        // User types "goo".
        assertTrue(mInputConnection.setComposingText("goo", 1));
        assertTrue(mAutocomplete.shouldAutocomplete());
        mAutocomplete.setAutocompleteText("goo", "gle.com");
        assertEquals("google.com", mAutocomplete.getText().toString());

        // User presses 'GO' key on the keyboard.
        mInputConnection.beginBatchEdit();

        assertTrue(mInputConnection.commitText("goo", 1));
        assertEquals("google.com", mAutocomplete.getText().toString());

        assertTrue(mInputConnection.performEditorAction(EditorInfo.IME_ACTION_GO));
        assertEquals("google.com", mAutocomplete.getText().toString());

        mInputConnection.endBatchEdit();

        assertEquals("google.com", mAutocomplete.getText().toString());
    }

    // crbug.com/759876
    @Test
    @EnableFeatures(ChromeFeatureList.SPANNABLE_INLINE_AUTOCOMPLETE)
    public void testTextSelectionGetsAnnouncedAgainOnFocusWithSpannableModel() {
        internalTestTextSelectionGetsAnnouncedAgainOnFocus();
    }

    @Test
    @DisableFeatures(ChromeFeatureList.SPANNABLE_INLINE_AUTOCOMPLETE)
    public void testTextSelectionGetsAnnouncedAgainOnFocusWithoutSpannableModel() {
        internalTestTextSelectionGetsAnnouncedAgainOnFocus();
    }

    private void internalTestTextSelectionGetsAnnouncedAgainOnFocus() {
        final String text = "hello";
        final int len = text.length();

        assertTrue(mInputConnection.commitText(text, len));
        mAutocomplete.setSelection(0, len);

        verifyOnPopulateAccessibilityEvent(
                AccessibilityEvent.TYPE_VIEW_TEXT_CHANGED, text, "", -1, 0, -1, 0, len);
        verifyOnPopulateAccessibilityEvent(AccessibilityEvent.TYPE_VIEW_TEXT_SELECTION_CHANGED,
                text, "", len, len, len, -1, -1);
        verifyOnPopulateAccessibilityEvent(
                AccessibilityEvent.TYPE_VIEW_TEXT_SELECTION_CHANGED, text, "", len, 0, len, -1, -1);
        mInOrder.verifyNoMoreInteractions();
        assertVerifierCallCounts(3, 3);

        assertTrue(mFocusPlaceHolder.requestFocus());
        mInOrder.verifyNoMoreInteractions();
        assertVerifierCallCounts(0, 0);

        // We left EditText with selected content. We should get the same event sent again now.
        mAutocomplete.setSelectAllOnFocus(true);
        assertTrue(mAutocomplete.requestFocus());

        mInOrder.verify(mVerifier).onUpdateSelection(0, len);
        verifyOnPopulateAccessibilityEvent(
                AccessibilityEvent.TYPE_VIEW_TEXT_SELECTION_CHANGED, text, "", len, 0, len, -1, -1);
        verifyOnPopulateAccessibilityEvent(
                AccessibilityEvent.TYPE_VIEW_FOCUSED, text, "", 2, -1, -1, -1, -1);

        assertVerifierCallCounts(2, 3);
        mInOrder.verifyNoMoreInteractions();
    }
}

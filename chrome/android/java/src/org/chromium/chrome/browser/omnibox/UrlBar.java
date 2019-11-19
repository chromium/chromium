// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.Rect;
import android.os.Build;
import android.os.StrictMode;
import android.provider.Settings;
import android.support.v13.view.inputmethod.EditorInfoCompat;
import android.support.v4.text.BidiFormatter;
import android.support.v4.util.ObjectsCompat;
import android.text.Editable;
import android.text.InputType;
import android.text.Layout;
import android.text.Selection;
import android.text.SpannableStringBuilder;
import android.text.TextUtils;
import android.text.TextWatcher;
import android.text.style.ReplacementSpan;
import android.util.AttributeSet;
import android.view.GestureDetector;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.View;
import android.view.accessibility.AccessibilityNodeInfo;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputConnection;
import android.widget.TextView;

import androidx.annotation.IntDef;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.Log;
import org.chromium.base.SysUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.metrics.CachedMetrics;
import org.chromium.chrome.browser.WindowDelegate;
import org.chromium.ui.KeyboardVisibilityDelegate;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * The URL text entry view for the Omnibox.
 */
public abstract class UrlBar extends AutocompleteEditText {
    private static final String TAG = "UrlBar";

    private static final boolean DEBUG = false;

    private static final CachedMetrics.ActionEvent ACTION_LONG_PRESS_COPY =
            new CachedMetrics.ActionEvent("Omnibox.LongPress.Copy");
    private static final CachedMetrics.ActionEvent ACTION_LONG_PRESS_CUT =
            new CachedMetrics.ActionEvent("Omnibox.LongPress.Cut");
    private static final CachedMetrics.ActionEvent ACTION_LONG_PRESS_SHARE =
            new CachedMetrics.ActionEvent("Omnibox.LongPress.Share");

    private static final CachedMetrics.TimesHistogramSample TIME_UNTIL_COPY =
            new CachedMetrics.TimesHistogramSample("Omnibox.TimeUntilFirst.Copy");
    private static final CachedMetrics.TimesHistogramSample TIME_UNTIL_CUT =
            new CachedMetrics.TimesHistogramSample("Omnibox.TimeUntilFirst.Cut");
    private static final CachedMetrics.TimesHistogramSample TIME_UNTIL_SHARE =
            new CachedMetrics.TimesHistogramSample("Omnibox.TimeUntilFirst.Share");

    @IntDef({OmniboxAction.CUT, OmniboxAction.COPY, OmniboxAction.SHARE})
    @Retention(RetentionPolicy.SOURCE)
    /** Actions that can be taken from the omnibox. */
    public @interface OmniboxAction {
        int CUT = 0;
        int COPY = 1;
        int SHARE = 2;
    }

    // TextView becomes very slow on long strings, so we limit maximum length
    // of what is displayed to the user, see limitDisplayableLength().
    private static final int MAX_DISPLAYABLE_LENGTH = 4000;
    private static final int MAX_DISPLAYABLE_LENGTH_LOW_END = 1000;

    /** The last time that the omnibox was focused. */
    private long mLastOmniboxFocusTime;

    /** Whether a timing event should be recorded. This will be true once per omnibox focus. */
    private boolean mShouldRecordTimingEvent;

    private boolean mFirstDrawComplete;

    /**
     * The text direction of the URL or query: LAYOUT_DIRECTION_LOCALE, LAYOUT_DIRECTION_LTR, or
     * LAYOUT_DIRECTION_RTL.
     * */
    private int mUrlDirection;

    private UrlBarDelegate mUrlBarDelegate;
    private UrlTextChangeListener mUrlTextChangeListener;
    private TextWatcher mTextChangedListener;
    private UrlBarTextContextMenuDelegate mTextContextMenuDelegate;
    private UrlDirectionListener mUrlDirectionListener;

    /**
     * The gesture detector is used to detect long presses. Long presses require special treatment
     * because the URL bar has custom touch event handling. See: {@link #onTouchEvent}.
     */
    private final GestureDetector mGestureDetector;

    private final KeyboardHideHelper mKeyboardHideHelper;

    private boolean mFocused;
    private boolean mSuppressingTouchMoveEventsForThisTouch;
    private MotionEvent mSuppressedTouchDownEvent;
    private boolean mAllowFocus = true;

    private boolean mPendingScroll;
    private int mPreviousWidth;

    @ScrollType
    private int mPreviousScrollType;
    private String mPreviousScrollText;
    private int mPreviousScrollViewWidth;
    private int mPreviousScrollResultXPosition;
    private float mPreviousScrollFontSize;
    private boolean mPreviousScrollWasRtl;

    // Used as a hint to indicate the text may contain an ellipsize span.  This will be true if an
    // ellispize span was applied the last time the text changed.  A true value here does not
    // guarantee that the text does contain the span currently as newly set text may have cleared
    // this (and it the value will only be recalculated after the text has been changed).
    private boolean mDidEllipsizeTextHint;

    /** A cached point for getting this view's location in the window. */
    private final int[] mCachedLocation = new int[2];

    /** The location of this view on the last ACTION_DOWN event. */
    private float mDownEventViewTop;

    /**
     * The character index in the displayed text where the origin ends. This is required to
     * ensure that the end of the origin is not scrolled out of view for long hostnames.
     */
    private int mOriginEndIndex;

    @ScrollType
    private int mScrollType;

    /** What scrolling action should be taken after the URL bar text changes. **/
    @IntDef({ScrollType.NO_SCROLL, ScrollType.SCROLL_TO_TLD, ScrollType.SCROLL_TO_BEGINNING})
    @Retention(RetentionPolicy.SOURCE)
    public @interface ScrollType {
        int NO_SCROLL = 0;
        int SCROLL_TO_TLD = 1;
        int SCROLL_TO_BEGINNING = 2;
    }

    /**
     * An optional string to use with AccessibilityNodeInfo to report text content.
     * This is particularly important for auto-fill applications, such as password managers, that
     * rely on AccessibilityNodeInfo data to apply related form-fill data.
     */
    private CharSequence mTextForAutofillServices;
    protected boolean mRequestingAutofillStructure;

    /**
     * Implement this to get updates when the direction of the text in the URL bar changes.
     * E.g. If the user is typing a URL, then erases it and starts typing a query in Arabic,
     * the direction will change from left-to-right to right-to-left.
     */
    interface UrlDirectionListener {
        /**
         * Called whenever the layout direction of the UrlBar changes.
         * @param layoutDirection the new direction: android.view.View.LAYOUT_DIRECTION_LTR or
         *                        android.view.View.LAYOUT_DIRECTION_RTL
         */
        public void onUrlDirectionChanged(int layoutDirection);
    }

    /**
     * Delegate used to communicate with the content side and the parent layout.
     */
    public interface UrlBarDelegate {
        /**
         * @return The view to be focused on a backward focus traversal.
         */
        @Nullable
        View getViewForUrlBackFocus();

        /**
         * @return Whether the keyboard should be allowed to learn from the user input.
         */
        boolean allowKeyboardLearning();

        /**
         * Called to notify that back key has been pressed while the URL bar has focus.
         */
        void backKeyPressed();

        /**
         * @return Whether or not we should force LTR text on the URL bar when unfocused.
         */
        boolean shouldForceLTR();

        /**
         * @return Whether or not the copy/cut action should grab the underlying URL or just copy
         *         whatever's in the URL bar verbatim.
         */
        boolean shouldCutCopyVerbatim();

        /**
         * Called to notify that a tap or long press gesture has been detected.
         * @param isLongPress Whether or not is a long press gesture.
         */
        void gestureDetected(boolean isLongPress);
    }

    /** Provides updates about the URL text changes. */
    public interface UrlTextChangeListener {
        /**
         * Called when the text state has changed.
         * @param textWithoutAutocomplete The url bar text without autocompletion.
         * @param textWithAutocomplete The url bar text with autocompletion.
         */
        // TODO(crbug.com/1003080): Consider splitting these into two different callbacks.
        void onTextChanged(String textWithoutAutocomplete, String textWithAutocomplete);
    }

    /** Delegate that provides the additional functionality to the textual context menus. */
    interface UrlBarTextContextMenuDelegate {
        /** @return The text to be pasted into the UrlBar. */
        @NonNull
        String getTextToPaste();

        /**
         * Gets potential replacement text to be used instead of the current selected text for
         * cut/copy actions.  If null is returned, the existing text will be cut or copied.
         *
         * @param currentText The current displayed text.
         * @param selectionStart The selection start in the display text.
         * @param selectionEnd The selection end in the display text.
         * @return The text to be cut/copied instead of the currently selected text.
         */
        @Nullable
        String getReplacementCutCopyText(String currentText, int selectionStart, int selectionEnd);
    }

    public UrlBar(Context context, AttributeSet attrs) {
        super(context, attrs);
        mUrlDirection = LAYOUT_DIRECTION_LOCALE;

        // The URL Bar is derived from an text edit class, and as such is focusable by
        // default. This means that if it is created before the first draw of the UI it
        // will (as the only focusable element of the UI) get focus on the first draw.
        // We react to this by greying out the tab area and bringing up the keyboard,
        // which we don't want to do at startup. Prevent this by disabling focus until
        // the first draw.
        setFocusable(false);
        setFocusableInTouchMode(false);

        // The HTC Sense IME will attempt to autocomplete words in the Omnibox when Prediction is
        // enabled.  We want to disable this feature and rely on the Omnibox's implementation.
        // Their IME does not respect ~TYPE_TEXT_FLAG_AUTO_COMPLETE nor any of the other InputType
        // options I tried, but setting the filter variation prevents it.  Sadly, it also removes
        // the .com button, but the prediction was buggy as it would autocomplete words even when
        // typing at the beginning of the omnibox text when other content was present (messing up
        // what was previously there).  See bug: http://b/issue?id=6200071
        String defaultIme = Settings.Secure.getString(
                getContext().getContentResolver(), Settings.Secure.DEFAULT_INPUT_METHOD);
        if (defaultIme != null && defaultIme.contains("com.htc.android.htcime")) {
            setInputType(getInputType() | InputType.TYPE_TEXT_VARIATION_FILTER);
        }

        mGestureDetector =
                new GestureDetector(getContext(), new GestureDetector.SimpleOnGestureListener() {
                    @Override
                    public void onLongPress(MotionEvent e) {
                        mUrlBarDelegate.gestureDetected(true);
                        performLongClick();
                    }

                    @Override
                    public boolean onSingleTapUp(MotionEvent e) {
                        requestFocus();
                        mUrlBarDelegate.gestureDetected(false);
                        return true;
                    }
                }, ThreadUtils.getUiThreadHandler());
        mGestureDetector.setOnDoubleTapListener(null);
        mKeyboardHideHelper = new KeyboardHideHelper(this, new Runnable() {
            @Override
            public void run() {
                if (mUrlBarDelegate != null) mUrlBarDelegate.backKeyPressed();
            }
        });

        ApiCompatibilityUtils.disableSmartSelectionTextClassifier(this);
    }

    /**
     * Record than an action occurred in the omnibox.
     * @param actionTaken The action taken that triggered the recording.
     * @param lastOmniboxFocusTime The time that the last omnibox focus event occurred.
     */
    public static void recordTimedActionForMetrics(
            @OmniboxAction int actionTaken, long lastOmniboxFocusTime) {
        final long finalTime = System.currentTimeMillis() - lastOmniboxFocusTime;
        assert finalTime >= 0;
        switch (actionTaken) {
            case OmniboxAction.COPY:
                TIME_UNTIL_COPY.record(finalTime);
                break;
            case OmniboxAction.CUT:
                TIME_UNTIL_CUT.record(finalTime);
                break;
            case OmniboxAction.SHARE:
                TIME_UNTIL_SHARE.record(finalTime);
                break;
            default:
                break;
        }
    }

    /**
     * Initialize the delegate that allows interaction with the Window.
     */
    public void setWindowDelegate(WindowDelegate windowDelegate) {
        mKeyboardHideHelper.setWindowDelegate(windowDelegate);
    }

    /**
     * Set the delegate to be used for text context menu actions.
     */
    public void setTextContextMenuDelegate(UrlBarTextContextMenuDelegate delegate) {
        mTextContextMenuDelegate = delegate;
    }

    @Override
    public boolean onKeyPreIme(int keyCode, KeyEvent event) {
        if (KeyEvent.KEYCODE_BACK == keyCode && event.getAction() == KeyEvent.ACTION_UP) {
            mKeyboardHideHelper.monitorForKeyboardHidden();
        }
        return super.onKeyPreIme(keyCode, event);
    }

    /**
     * See {@link AutocompleteEditText#setIgnoreTextChangesForAutocomplete(boolean)}.
     * <p>
     * {@link #setDelegate(UrlBarDelegate)} must be called with a non-null instance prior to
     * enabling autocomplete.
     */
    @Override
    public void setIgnoreTextChangesForAutocomplete(boolean ignoreAutocomplete) {
        assert mUrlBarDelegate != null;
        super.setIgnoreTextChangesForAutocomplete(ignoreAutocomplete);
    }

    @Override
    protected void onFocusChanged(boolean focused, int direction, Rect previouslyFocusedRect) {
        mFocused = focused;
        super.onFocusChanged(focused, direction, previouslyFocusedRect);

        if (focused) {
            mPendingScroll = false;
            mLastOmniboxFocusTime = System.currentTimeMillis();
        }
        mShouldRecordTimingEvent = focused;

        fixupTextDirection();
    }

    /**
     * Sets whether this {@link UrlBar} should be focusable.
     */
    public void setAllowFocus(boolean allowFocus) {
        mAllowFocus = allowFocus;
        setFocusable(allowFocus);
        setFocusableInTouchMode(allowFocus);
    }

    /**
     * Sets the {@link UrlBar}'s text direction based on focus and contents.
     *
     * Should be called whenever focus or text contents change.
     */
    private void fixupTextDirection() {
        // When unfocused, force left-to-right rendering at the paragraph level (which is desired
        // for URLs). Right-to-left runs are still rendered RTL, but will not flip the whole URL
        // around. This is consistent with OmniboxViewViews on desktop. When focused, render text
        // normally (to allow users to make non-URL searches and to avoid showing Android's split
        // insertion point when an RTL user enters RTL text). Also render text normally when the
        // text field is empty (because then it displays an instruction that is not a URL).
        if (mFocused || length() == 0 || !mUrlBarDelegate.shouldForceLTR()) {
            setTextDirection(TEXT_DIRECTION_INHERIT);
        } else {
            setTextDirection(TEXT_DIRECTION_LTR);
        }
        // Always align to the same as the paragraph direction (LTR = left, RTL = right).
        setTextAlignment(TEXT_ALIGNMENT_TEXT_START);
    }

    @Override
    public void onWindowFocusChanged(boolean hasWindowFocus) {
        super.onWindowFocusChanged(hasWindowFocus);
        if (DEBUG) Log.i(TAG, "onWindowFocusChanged: " + hasWindowFocus);
        if (hasWindowFocus) {
            if (isFocused()) {
                // Without the call to post(..), the keyboard was not getting shown when the
                // window regained focus despite this being the final call in the view system
                // flow.
                post(new Runnable() {
                    @Override
                    public void run() {
                        KeyboardVisibilityDelegate.getInstance().showKeyboard(UrlBar.this);
                    }
                });
            }
        }
    }

    @Override
    public View focusSearch(int direction) {
        if (direction == View.FOCUS_BACKWARD && mUrlBarDelegate.getViewForUrlBackFocus() != null) {
            return mUrlBarDelegate.getViewForUrlBackFocus();
        } else {
            return super.focusSearch(direction);
        }
    }

    @Override
    public boolean onTouchEvent(MotionEvent event) {
        // This method contains special logic to enable long presses to be handled correctly.

        // One piece of the logic is to suppress all ACTION_DOWN events received while the UrlBar is
        // not focused, and only pass them to super.onTouchEvent() if it turns out we're about to
        // perform a long press. Long pressing will not behave properly without sending this event,
        // but if we always send it immediately, it will cause the keyboard to show immediately,
        // whereas we want to wait to show it until after the URL focus animation finishes, to avoid
        // performance issues on slow devices.

        // The other piece of the logic is to suppress ACTION_MOVE events received after an
        // ACTION_DOWN received while the UrlBar is not focused. This is because the UrlBar moves to
        // the side as it's focusing, and a finger held still on the screen would therefore be
        // interpreted as a drag selection.

        if (event.getActionMasked() == MotionEvent.ACTION_DOWN) {
            getLocationInWindow(mCachedLocation);
            mDownEventViewTop = mCachedLocation[1];
            mSuppressingTouchMoveEventsForThisTouch = !mFocused;
        }

        if (!mFocused) {
            if (event.getActionMasked() == MotionEvent.ACTION_DOWN) {
                mSuppressedTouchDownEvent = MotionEvent.obtain(event);
            }
            mGestureDetector.onTouchEvent(event);
            return true;
        }

        if (event.getActionMasked() == MotionEvent.ACTION_UP
                || event.getActionMasked() == MotionEvent.ACTION_CANCEL) {
            // Minor optimization to avoid unnecessarily holding onto a MotionEvent after the touch
            // finishes.
            mSuppressedTouchDownEvent = null;
        }

        if (mSuppressingTouchMoveEventsForThisTouch
                && event.getActionMasked() == MotionEvent.ACTION_MOVE) {
            return true;
        }

        try {
            return super.onTouchEvent(event);
        } catch (NullPointerException e) {
            // Working around a platform bug (b/25562038) that was fixed in N that can throw an
            // exception during text selection. We just swallow the exception. The outcome is that
            // the text selection handle doesn't show.

            // If this happens on N or later, there's a different issue here that we might want to
            // know about.
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N) throw e;

            Log.w(TAG, "Ignoring NPE in UrlBar#onTouchEvent.", e);
            return true;
        } catch (IndexOutOfBoundsException e) {
            // Work around crash of unknown origin (https://crbug.com/837419).
            Log.w(TAG, "Ignoring IndexOutOfBoundsException in UrlBar#onTouchEvent.", e);
            return true;
        }
    }

    @Override
    public boolean performLongClick() {
        if (!shouldPerformLongClick()) return false;

        releaseSuppressedTouchDownEvent();
        return super.performLongClick();
    }

    /**
     * @return Whether or not a long click should be performed.
     */
    private boolean shouldPerformLongClick() {
        getLocationInWindow(mCachedLocation);

        // If the view moved between the last down event, block the long-press.
        return mDownEventViewTop == mCachedLocation[1];
    }

    private void releaseSuppressedTouchDownEvent() {
        if (mSuppressedTouchDownEvent != null) {
            super.onTouchEvent(mSuppressedTouchDownEvent);
            mSuppressedTouchDownEvent = null;
        }
    }

    @Override
    public void onDraw(Canvas canvas) {
        super.onDraw(canvas);

        if (!mFirstDrawComplete) {
            mFirstDrawComplete = true;

            // We have now avoided the first draw problem (see the comment in
            // the constructor) so we want to make the URL bar focusable so that
            // touches etc. activate it.
            setFocusable(mAllowFocus);
            setFocusableInTouchMode(mAllowFocus);
        }

        // Notify listeners if the URL's direction has changed.
        updateUrlDirection();
    }

    /**
     * If the direction of the URL has changed, update mUrlDirection and notify the
     * UrlDirectionListeners.
     */
    private void updateUrlDirection() {
        Layout layout = getLayout();
        if (layout == null) return;

        int urlDirection;
        if (length() == 0) {
            urlDirection = LAYOUT_DIRECTION_LOCALE;
        } else if (layout.getParagraphDirection(0) == Layout.DIR_LEFT_TO_RIGHT) {
            urlDirection = LAYOUT_DIRECTION_LTR;
        } else {
            urlDirection = LAYOUT_DIRECTION_RTL;
        }

        if (urlDirection != mUrlDirection) {
            mUrlDirection = urlDirection;
            if (mUrlDirectionListener != null) {
                mUrlDirectionListener.onUrlDirectionChanged(urlDirection);
            }

            // Ensure the display text is visible after updating the URL direction.
            scrollDisplayText();
        }
    }

    /**
     * @return The text direction of the URL, e.g. LAYOUT_DIRECTION_LTR.
     */
    public int getUrlDirection() {
        return mUrlDirection;
    }

    /**
     * Sets the listener for changes in the url bar's layout direction. Also calls
     * onUrlDirectionChanged() immediately on the listener.
     *
     * @param listener The UrlDirectionListener to receive callbacks when the url direction changes,
     *     or null to unregister any previously registered listener.
     */
    public void setUrlDirectionListener(UrlDirectionListener listener) {
        mUrlDirectionListener = listener;
        if (mUrlDirectionListener != null) {
            mUrlDirectionListener.onUrlDirectionChanged(mUrlDirection);
        }
    }

    /**
     * Set the url delegate to handle communication from the {@link UrlBar} to the rest of the UI.
     * @param delegate The {@link UrlBarDelegate} to be used.
     */
    public void setDelegate(UrlBarDelegate delegate) {
        mUrlBarDelegate = delegate;
    }

    /**
     * Set the listener to be notified when the URL text has changed.
     * @param listener The listener to be notified.
     */
    public void setUrlTextChangeListener(UrlTextChangeListener listener) {
        mUrlTextChangeListener = listener;
    }

    /**
     * Set the listener to be notified when the view's text has changed.
     * @param textChangedListener The listener to be notified.
     */
    public void setTextChangedListener(TextWatcher textChangedListener) {
        if (ObjectsCompat.equals(mTextChangedListener, textChangedListener)) {
            return;
        } else if (mTextChangedListener != null) {
            removeTextChangedListener(mTextChangedListener);
        }

        mTextChangedListener = textChangedListener;
        addTextChangedListener(mTextChangedListener);
    }

    /**
     * Set the text to report to Autofill services upon call to onProvideAutofillStructure.
     */
    public void setTextForAutofillServices(CharSequence text) {
        mTextForAutofillServices = text;
    }

    @Override
    public boolean onTextContextMenuItem(int id) {
        if (mTextContextMenuDelegate == null) return super.onTextContextMenuItem(id);

        if (id == android.R.id.paste) {
            String pasteString = mTextContextMenuDelegate.getTextToPaste();
            if (pasteString != null) {
                int min = 0;
                int max = getText().length();

                if (isFocused()) {
                    final int selStart = getSelectionStart();
                    final int selEnd = getSelectionEnd();

                    min = Math.max(0, Math.min(selStart, selEnd));
                    max = Math.max(0, Math.max(selStart, selEnd));
                }

                Selection.setSelection(getText(), max);
                getText().replace(min, max, pasteString);
                onPaste();
            }
            return true;
        }

        if ((id == android.R.id.cut || id == android.R.id.copy)
                && !mUrlBarDelegate.shouldCutCopyVerbatim()) {
            if (id == android.R.id.cut) {
                ACTION_LONG_PRESS_CUT.record();
            } else {
                ACTION_LONG_PRESS_COPY.record();
            }
            if (mShouldRecordTimingEvent) {
                recordTimedActionForMetrics(
                        id == android.R.id.copy ? OmniboxAction.COPY : OmniboxAction.CUT,
                        mLastOmniboxFocusTime);
                mShouldRecordTimingEvent = false;
            }
            String currentText = getText().toString();
            String replacementCutCopyText = mTextContextMenuDelegate.getReplacementCutCopyText(
                    currentText, getSelectionStart(), getSelectionEnd());
            if (replacementCutCopyText == null) return super.onTextContextMenuItem(id);

            setIgnoreTextChangesForAutocomplete(true);
            setText(replacementCutCopyText);
            setSelection(0, replacementCutCopyText.length());
            setIgnoreTextChangesForAutocomplete(false);

            boolean retVal = super.onTextContextMenuItem(id);

            if (TextUtils.equals(getText(), replacementCutCopyText)) {
                // Restore the old text if the operation did modify the text.
                setIgnoreTextChangesForAutocomplete(true);
                setText(currentText);

                // Move the cursor to the end.
                setSelection(getText().length());
                setIgnoreTextChangesForAutocomplete(false);
            }

            return retVal;
        }

        if (id == android.R.id.shareText) {
            ACTION_LONG_PRESS_SHARE.record();
            if (mShouldRecordTimingEvent) {
                recordTimedActionForMetrics(OmniboxAction.SHARE, mLastOmniboxFocusTime);
                mShouldRecordTimingEvent = false;
            }
        }

        return super.onTextContextMenuItem(id);
    }

    /**
     * Specified how text should be scrolled within the UrlBar.
     *
     * @param scrollType What type of scroll should be applied to the text.
     * @param scrollToIndex The index that should be scrolled to, which only applies to
     *                      {@link ScrollType#SCROLL_TO_TLD}.
     */
    public void setScrollState(@ScrollType int scrollType, int scrollToIndex) {
        if (scrollType == ScrollType.SCROLL_TO_TLD) {
            mOriginEndIndex = scrollToIndex;
        } else {
            mOriginEndIndex = 0;
        }
        mScrollType = scrollType;
        scrollDisplayText();
    }

    /**
     * Scrolls the omnibox text to a position determined by the current scroll type.
     *
     * @see #setScrollState(int, int)
     */
    private void scrollDisplayText() {
        if (isLayoutRequested()) {
            mPendingScroll = mScrollType != ScrollType.NO_SCROLL;
            return;
        }
        scrollDisplayTextInternal(mScrollType);
    }

    /**
     * Scrolls the omnibox text to the position specified, based on the {@link ScrollType}.
     *
     * @param scrollType What type of scroll to perform.
     *                   SCROLL_TO_TLD: Scrolls the omnibox text to bring the TLD into view.
     *                   SCROLL_TO_BEGINNING: Scrolls text that's too long to fit in the omnibox
     *                                        to the beginning so we can see the first character.
     */
    private void scrollDisplayTextInternal(@ScrollType int scrollType) {
        mPendingScroll = false;

        if (mFocused) return;

        Editable text = getText();
        if (TextUtils.isEmpty(text)) scrollType = ScrollType.SCROLL_TO_BEGINNING;

        // Ensure any selection from the focus state is cleared.
        setSelection(0);

        float currentTextSize = getTextSize();
        boolean currentIsRtl = getLayoutDirection() == LAYOUT_DIRECTION_RTL;

        int measuredWidth = getMeasuredWidth() - (getPaddingLeft() + getPaddingRight());
        if (scrollType == mPreviousScrollType && TextUtils.equals(text, mPreviousScrollText)
                && measuredWidth == mPreviousScrollViewWidth
                // Font size is float but it changes in discrete range (eg small font, big font),
                // therefore false negative using regular equality is unlikely.
                && currentTextSize == mPreviousScrollFontSize
                && currentIsRtl == mPreviousScrollWasRtl) {
            scrollTo(mPreviousScrollResultXPosition, getScrollY());
            return;
        }

        switch (scrollType) {
            case ScrollType.SCROLL_TO_TLD:
                scrollToTLD();
                break;
            case ScrollType.SCROLL_TO_BEGINNING:
                scrollToBeginning();
                break;
            default:
                // Intentional return to avoid clearing scroll state when no scroll was applied.
                return;
        }

        mPreviousScrollType = scrollType;
        mPreviousScrollText = text.toString();
        mPreviousScrollViewWidth = measuredWidth;
        mPreviousScrollFontSize = currentTextSize;
        mPreviousScrollResultXPosition = getScrollX();
        mPreviousScrollWasRtl = currentIsRtl;
    }

    /**
     * Scrolls the omnibox text to show the very beginning of the text entered.
     */
    private void scrollToBeginning() {
        Editable text = getText();
        float scrollPos = 0f;
        if (TextUtils.isEmpty(text)) {
            if (getLayoutDirection() == LAYOUT_DIRECTION_RTL
                    && BidiFormatter.getInstance().isRtl(getHint())) {
                // Compared to below that uses getPrimaryHorizontal(1) due to 0 returning an
                // invalid value, if the text is empty, getPrimaryHorizontal(0) returns the actual
                // max scroll amount.
                scrollPos = (int) getLayout().getPrimaryHorizontal(0) - getMeasuredWidth();
            }
        } else if (BidiFormatter.getInstance().isRtl(text)) {
            // RTL.
            float endPointX = getLayout().getPrimaryHorizontal(text.length());
            int measuredWidth = getMeasuredWidth();
            float width = getLayout().getPaint().measureText(text.toString());
            scrollPos = Math.max(0, endPointX - measuredWidth + width);
        }
        scrollTo((int) scrollPos, getScrollY());
    }

    /**
     * Scrolls the omnibox text to bring the TLD into view.
     */
    private void scrollToTLD() {
        Editable url = getText();
        int measuredWidth = getMeasuredWidth() - (getPaddingLeft() + getPaddingRight());

        Layout textLayout = getLayout();
        assert getLayout().getLineCount() == 1;
        final int originEndIndex = Math.min(mOriginEndIndex, url.length());
        if (mOriginEndIndex > url.length()) {
            // If discovered locally, please update crbug.com/859219 with the steps to reproduce.
            assert false : "Attempting to scroll past the end of the URL: " + url + ", end index: "
                           + mOriginEndIndex;
        }
        float endPointX = textLayout.getPrimaryHorizontal(originEndIndex);
        // Compare the position offset of the last character and the character prior to determine
        // the LTR-ness of the final component of the URL.
        float priorToEndPointX = url.length() == 1
                ? 0
                : textLayout.getPrimaryHorizontal(Math.max(0, originEndIndex - 1));

        float scrollPos;
        if (priorToEndPointX < endPointX) {
            // LTR
            scrollPos = Math.max(0, endPointX - measuredWidth);
        } else {
            // RTL

            // To handle BiDirectional text, search backward from the two existing offsets to find
            // the first LTR character.  Ensure the final RTL component of the domain is visible
            // above any of the prior LTR pieces.
            int rtlStartIndexForEndingRun = originEndIndex - 1;
            for (int i = originEndIndex - 2; i >= 0; i--) {
                float indexOffsetDrawPosition = textLayout.getPrimaryHorizontal(i);
                if (indexOffsetDrawPosition > endPointX) {
                    rtlStartIndexForEndingRun = i;
                } else {
                    // getPrimaryHorizontal determines the index position for the next character
                    // based on the previous characters.  In bi-directional text, the first RTL
                    // character following LTR text will have an LTR-appearing horizontal offset
                    // as it is based on the preceding LTR text.  Thus, the start of the RTL
                    // character run will be after and including the first LTR horizontal index.
                    rtlStartIndexForEndingRun = Math.max(0, rtlStartIndexForEndingRun - 1);
                    break;
                }
            }
            float width = textLayout.getPaint().measureText(
                    url.subSequence(rtlStartIndexForEndingRun, originEndIndex).toString());
            if (width < measuredWidth) {
                scrollPos = Math.max(0, endPointX + width - measuredWidth);
            } else {
                scrollPos = endPointX + measuredWidth;
            }
        }
        scrollTo((int) scrollPos, getScrollY());
    }

    @Override
    protected void onLayout(boolean changed, int left, int top, int right, int bottom) {
        super.onLayout(changed, left, top, right, bottom);

        if (mPendingScroll) {
            scrollDisplayTextInternal(mScrollType);
        } else if (mPreviousWidth != (right - left)) {
            scrollDisplayTextInternal(mScrollType);
            mPreviousWidth = right - left;
        }
    }

    @Override
    public boolean bringPointIntoView(int offset) {
        // TextView internally attempts to keep the selection visible, but in the unfocused state
        // this class ensures that the TLD is visible.
        if (!mFocused) return false;
        assert !mPendingScroll;

        return super.bringPointIntoView(offset);
    }

    @Override
    public InputConnection onCreateInputConnection(EditorInfo outAttrs) {
        InputConnection connection = super.onCreateInputConnection(outAttrs);
        if (mUrlBarDelegate == null || !mUrlBarDelegate.allowKeyboardLearning()) {
            outAttrs.imeOptions |= EditorInfoCompat.IME_FLAG_NO_PERSONALIZED_LEARNING;
        }
        return connection;
    }

    @Override
    public void setText(CharSequence text, BufferType type) {
        if (DEBUG) Log.i(TAG, "setText -- text: %s", text);
        super.setText(text, type);
        fixupTextDirection();
    }

    private void limitDisplayableLength() {
        // To limit displayable length we replace middle portion of the string with ellipsis.
        // That affects only presentation of the text, and doesn't affect other aspects like
        // copying to the clipboard, getting text with getText(), etc.
        final int maxLength =
                SysUtils.isLowEndDevice() ? MAX_DISPLAYABLE_LENGTH_LOW_END : MAX_DISPLAYABLE_LENGTH;

        Editable text = getText();
        int textLength = text.length();
        if (textLength <= maxLength) {
            if (mDidEllipsizeTextHint) {
                EllipsisSpan[] spans = text.getSpans(0, textLength, EllipsisSpan.class);
                if (spans != null && spans.length > 0) {
                    assert spans.length == 1 : "Should never apply more than a single EllipsisSpan";
                    for (int i = 0; i < spans.length; i++) {
                        text.removeSpan(spans[i]);
                    }
                }
            }
            mDidEllipsizeTextHint = false;
            return;
        }

        mDidEllipsizeTextHint = true;

        int spanLeft = text.nextSpanTransition(0, textLength, EllipsisSpan.class);
        if (spanLeft != textLength) return;

        spanLeft = maxLength / 2;
        text.setSpan(EllipsisSpan.INSTANCE, spanLeft, textLength - spanLeft,
                Editable.SPAN_INCLUSIVE_EXCLUSIVE);
    }

    @Override
    public void onInitializeAccessibilityNodeInfo(AccessibilityNodeInfo info) {
        // Certain OEM implementations of onInitializeAccessibilityNodeInfo trigger disk reads
        // to access the clipboard.  crbug.com/640993
        StrictMode.ThreadPolicy oldPolicy = StrictMode.allowThreadDiskReads();
        try {
            super.onInitializeAccessibilityNodeInfo(info);
        } finally {
            StrictMode.setThreadPolicy(oldPolicy);
        }
    }

    @Override
    public Editable getText() {
        return mRequestingAutofillStructure ? new SpannableStringBuilder(mTextForAutofillServices)
                                            : super.getText();
    }

    @Override
    public CharSequence getAccessibilityClassName() {
        // When UrlBar is used as a read-only TextView, force Talkback to pronounce it like
        // TextView. Otherwise Talkback will say "Edit box, http://...". crbug.com/636988
        if (isEnabled()) {
            return super.getAccessibilityClassName();
        } else {
            return TextView.class.getName();
        }
    }

    @Override
    public void replaceAllTextFromAutocomplete(String text) {
        if (DEBUG) Log.i(TAG, "replaceAllTextFromAutocomplete: " + text);
        setText(text);
    }

    @Override
    public void onAutocompleteTextStateChanged(boolean updateDisplay) {
        if (DEBUG) {
            Log.i(TAG, "onAutocompleteTextStateChanged: DIS[%b]", updateDisplay);
        }
        if (mUrlTextChangeListener == null) return;
        if (updateDisplay) limitDisplayableLength();
        // crbug.com/764749
        Log.w(TAG, "Text change observed, triggering autocomplete.");

        mUrlTextChangeListener.onTextChanged(
                getTextWithoutAutocomplete(), getTextWithAutocomplete());
    }

    /**
     * Span that displays ellipsis instead of the text. Used to hide portion of
     * very large string to get decent performance from TextView.
     */
    private static class EllipsisSpan extends ReplacementSpan {
        private static final String ELLIPSIS = "...";

        public static final EllipsisSpan INSTANCE = new EllipsisSpan();

        @Override
        public int getSize(
                Paint paint, CharSequence text, int start, int end, Paint.FontMetricsInt fm) {
            return (int) paint.measureText(ELLIPSIS);
        }

        @Override
        public void draw(Canvas canvas, CharSequence text, int start, int end, float x, int top,
                int y, int bottom, Paint paint) {
            canvas.drawText(ELLIPSIS, x, y, paint);
        }
    }
}

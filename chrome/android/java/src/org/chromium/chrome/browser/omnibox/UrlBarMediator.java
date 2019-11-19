// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import android.content.ClipData;
import android.content.ClipboardManager;
import android.content.Context;
import android.net.Uri;
import android.text.Editable;
import android.text.Spanned;
import android.text.TextUtils;
import android.text.TextWatcher;
import android.text.format.DateUtils;
import android.view.ActionMode;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.library_loader.LibraryProcessType;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.WindowDelegate;
import org.chromium.chrome.browser.omnibox.OmniboxUrlEmphasizer.UrlEmphasisSpan;
import org.chromium.chrome.browser.omnibox.UrlBar.ScrollType;
import org.chromium.chrome.browser.omnibox.UrlBar.UrlBarDelegate;
import org.chromium.chrome.browser.omnibox.UrlBar.UrlDirectionListener;
import org.chromium.chrome.browser.omnibox.UrlBar.UrlTextChangeListener;
import org.chromium.chrome.browser.omnibox.UrlBarCoordinator.SelectionState;
import org.chromium.chrome.browser.omnibox.UrlBarProperties.AutocompleteText;
import org.chromium.chrome.browser.omnibox.UrlBarProperties.UrlBarTextState;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteCoordinatorFactory;
import org.chromium.content_public.browser.BrowserStartupController;
import org.chromium.ui.base.Clipboard;
import org.chromium.ui.modelutil.PropertyModel;

import java.net.MalformedURLException;
import java.net.URL;
import java.util.ArrayList;
import java.util.List;

/**
 * Handles collecting and pushing state information to the UrlBar model.
 */
class UrlBarMediator
        implements UrlBar.UrlBarTextContextMenuDelegate, UrlBar.UrlTextChangeListener, TextWatcher {
    private final PropertyModel mModel;

    private Callback<Boolean> mOnFocusChangeCallback;
    private boolean mHasFocus;

    private UrlBarData mUrlBarData;
    private @ScrollType int mScrollType = UrlBar.ScrollType.NO_SCROLL;
    private @SelectionState int mSelectionState = UrlBarCoordinator.SelectionState.SELECT_ALL;

    // The numbers for "MobileOmnibox.LongPressPasteAge", the expected time range of time is from
    // 1ms to 1 hour, and 100 buckets.
    private static final long MIN_TIME_MILLIS = 1;
    private static final long MAX_TIME_MILLIS = DateUtils.HOUR_IN_MILLIS;
    private static final int NUM_OF_BUCKETS = 100;

    private final List<UrlTextChangeListener> mUrlTextChangeListeners = new ArrayList<>();
    private final List<TextWatcher> mTextChangedListeners = new ArrayList<>();

    public UrlBarMediator(PropertyModel model) {
        mModel = model;

        mModel.set(UrlBarProperties.FOCUS_CHANGE_CALLBACK, this::onUrlFocusChange);
        mModel.set(UrlBarProperties.SHOW_CURSOR, false);
        mModel.set(UrlBarProperties.TEXT_CONTEXT_MENU_DELEGATE, this);
        mModel.set(UrlBarProperties.URL_TEXT_CHANGE_LISTENER, this);
        mModel.set(UrlBarProperties.TEXT_CHANGED_LISTENER, this);
        setUseDarkTextColors(true);
    }

    /**
     * Set the primary delegate for the UrlBar view.
     */
    public void setDelegate(UrlBarDelegate delegate) {
        mModel.set(UrlBarProperties.DELEGATE, delegate);
    }

    /** @see UrlBarMediator#setDelegate(UrlBarDelegate) */
    public void addUrlTextChangeListener(UrlTextChangeListener listener) {
        mUrlTextChangeListeners.add(listener);
    }

    /** @see android.widget.TextView#addTextChangedListener */
    public void addTextChangedListener(TextWatcher textWatcher) {
        mTextChangedListeners.add(textWatcher);
    }

    /**
     * Updates the text content of the UrlBar.
     *
     * @param data The new data to be displayed.
     * @param scrollType The scroll type that should be applied to the data.
     * @param selectionState Specifies how the text should be selected when focused.
     * @return Whether this data differs from the previously passed in values.
     */
    public boolean setUrlBarData(
            UrlBarData data, @ScrollType int scrollType, @SelectionState int selectionState) {
        if (data.originEndIndex == data.originStartIndex) {
            scrollType = UrlBar.ScrollType.SCROLL_TO_BEGINNING;
        }

        // Do not scroll to the end of the host for URLs such as data:, javascript:, etc...
        if (data.url != null && data.originEndIndex == data.displayText.length()) {
            Uri uri = Uri.parse(data.url);
            String scheme = uri.getScheme();
            if (!TextUtils.isEmpty(scheme)
                    && UrlBarData.UNSUPPORTED_SCHEMES_TO_SPLIT.contains(scheme)) {
                scrollType = UrlBar.ScrollType.SCROLL_TO_BEGINNING;
            }
        }

        if (!mHasFocus && isNewTextEquivalentToExistingText(mUrlBarData, data)
                && mScrollType == scrollType) {
            return false;
        }
        mUrlBarData = data;
        mScrollType = scrollType;
        mSelectionState = selectionState;

        pushTextToModel();
        return true;
    }

    private void pushTextToModel() {
        CharSequence text =
                !mHasFocus ? mUrlBarData.displayText : mUrlBarData.getEditingOrDisplayText();
        CharSequence textForAutofillServices = text;

        if (!(mHasFocus || TextUtils.isEmpty(text) || mUrlBarData.url == null)) {
            textForAutofillServices = mUrlBarData.url;
        }

        @ScrollType
        int scrollType = mHasFocus ? UrlBar.ScrollType.NO_SCROLL : mScrollType;
        if (text == null) text = "";

        UrlBarTextState state = new UrlBarTextState(text, textForAutofillServices, scrollType,
                mUrlBarData.originEndIndex, mSelectionState);
        mModel.set(UrlBarProperties.TEXT_STATE, state);
    }

    @VisibleForTesting
    protected static boolean isNewTextEquivalentToExistingText(
            UrlBarData existingUrlData, UrlBarData newUrlData) {
        if (existingUrlData == null) return newUrlData == null;
        if (newUrlData == null) return false;

        if (!TextUtils.equals(existingUrlData.editingText, newUrlData.editingText)) return false;

        CharSequence existingCharSequence = existingUrlData.displayText;
        CharSequence newCharSequence = newUrlData.displayText;
        if (existingCharSequence == null) return newCharSequence == null;

        // Regardless of focus state, ensure the text content is the same.
        if (!TextUtils.equals(existingCharSequence, newCharSequence)) return false;

        // If both existing and new text is empty, then treat them equal regardless of their
        // spanned state.
        if (TextUtils.isEmpty(newCharSequence)) return true;

        // When not focused, compare the emphasis spans applied to the text to determine
        // equality.  Internally, TextView applies many additional spans that need to be
        // ignored for this comparison to be useful, so this is scoped to only the span types
        // applied by our UI.
        if (!(newCharSequence instanceof Spanned) || !(existingCharSequence instanceof Spanned)) {
            return false;
        }

        Spanned currentText = (Spanned) existingCharSequence;
        Spanned newText = (Spanned) newCharSequence;
        UrlEmphasisSpan[] currentSpans =
                currentText.getSpans(0, currentText.length(), UrlEmphasisSpan.class);
        UrlEmphasisSpan[] newSpans = newText.getSpans(0, newText.length(), UrlEmphasisSpan.class);
        if (currentSpans.length != newSpans.length) return false;
        for (int i = 0; i < currentSpans.length; i++) {
            UrlEmphasisSpan currentSpan = currentSpans[i];
            UrlEmphasisSpan newSpan = newSpans[i];
            if (!currentSpan.equals(newSpan)
                    || currentText.getSpanStart(currentSpan) != newText.getSpanStart(newSpan)
                    || currentText.getSpanEnd(currentSpan) != newText.getSpanEnd(newSpan)
                    || currentText.getSpanFlags(currentSpan) != newText.getSpanFlags(newSpan)) {
                return false;
            }
        }
        return true;
    }

    /**
     * Sets the autocomplete text to be shown.
     *
     * @param userText The existing user text.
     * @param autocompleteText The text to be appended to the user text.
     */
    public void setAutocompleteText(String userText, String autocompleteText) {
        if (!mHasFocus) {
            assert false : "Should not update autocomplete text when not focused";
            return;
        }
        mModel.set(UrlBarProperties.AUTOCOMPLETE_TEXT,
                new AutocompleteText(userText, autocompleteText));
    }

    /**
     * Updates the callback that will be notified when the focus changes on the UrlBar.
     *
     * @param callback The callback to be notified on focus changes.
     */
    public void setOnFocusChangedCallback(Callback<Boolean> callback) {
        mOnFocusChangeCallback = callback;
    }

    private void onUrlFocusChange(boolean focus) {
        mHasFocus = focus;

        if (mModel.get(UrlBarProperties.ALLOW_FOCUS)) {
            mModel.set(UrlBarProperties.SHOW_CURSOR, mHasFocus);
        }

        UrlBarTextState preCallbackState = mModel.get(UrlBarProperties.TEXT_STATE);
        if (mOnFocusChangeCallback != null) mOnFocusChangeCallback.onResult(focus);
        boolean textChangedInFocusCallback =
                mModel.get(UrlBarProperties.TEXT_STATE) != preCallbackState;
        if (mUrlBarData != null && !textChangedInFocusCallback) {
            pushTextToModel();
        }
    }

    /**
     * Sets whether to use dark text colors.
     *
     * @return Whether this resulted in a change from the previous value.
     */
    public boolean setUseDarkTextColors(boolean useDarkColors) {
        // TODO(bauerb): Make clients observe the property instead of checking the return value.
        boolean previousValue = mModel.get(UrlBarProperties.USE_DARK_TEXT_COLORS);
        mModel.set(UrlBarProperties.USE_DARK_TEXT_COLORS, useDarkColors);
        return previousValue != useDarkColors;
    }

    /**
     * Sets whether the view allows user focus.
     */
    public void setAllowFocus(boolean allowFocus) {
        mModel.set(UrlBarProperties.ALLOW_FOCUS, allowFocus);
        if (allowFocus) {
            mModel.set(UrlBarProperties.SHOW_CURSOR, mHasFocus);
        }
    }

    /**
     * Set the listener to be notified for URL direction changes.
     */
    public void setUrlDirectionListener(UrlDirectionListener listener) {
        mModel.set(UrlBarProperties.URL_DIRECTION_LISTENER, listener);
    }

    /**
     * Set the delegate that provides Window capabilities.
     */
    public void setWindowDelegate(WindowDelegate windowDelegate) {
        mModel.set(UrlBarProperties.WINDOW_DELEGATE, windowDelegate);
    }

    /**
     * Set the callback to handle contextual Action Modes.
     */
    public void setActionModeCallback(ActionMode.Callback callback) {
        mModel.set(UrlBarProperties.ACTION_MODE_CALLBACK, callback);
    }

    @Override
    public String getReplacementCutCopyText(
            String currentText, int selectionStart, int selectionEnd) {
        if (mUrlBarData == null || mUrlBarData.url == null) return null;

        // Replace the cut/copy text only applies if the user selected from the beginning of the
        // display text.
        if (selectionStart != 0) return null;

        // Trim to just the currently selected text as that is the only text we are replacing.
        currentText = currentText.substring(selectionStart, selectionEnd);

        String formattedUrlLocation;
        String originalUrlLocation;
        try {
            // TODO(bauerb): Use |urlBarData.originEndIndex| for this instead?
            URL javaUrl = new URL(mUrlBarData.url);
            formattedUrlLocation = getUrlContentsPrePath(
                    mUrlBarData.getEditingOrDisplayText().toString(), javaUrl.getHost());
            originalUrlLocation = getUrlContentsPrePath(mUrlBarData.url, javaUrl.getHost());
        } catch (MalformedURLException mue) {
            // Just keep the existing selected text for cut/copy if unable to parse the URL.
            return null;
        }

        // If we are copying/cutting the full previously formatted URL, reset the URL
        // text before initiating the TextViews handling of the context menu.
        //
        // Example:
        //    Original display text: www.example.com
        //    Original URL:          http://www.example.com
        //
        // Editing State:
        //    www.example.com/blah/foo
        //    |<--- Selection --->|
        //
        // Resulting clipboard text should be:
        //    http://www.example.com/blah/
        //
        // As long as the full original text was selected, it will replace that with the original
        // URL and keep any further modifications by the user.
        if (!currentText.startsWith(formattedUrlLocation)
                || selectionEnd < formattedUrlLocation.length()) {
            return null;
        }

        return originalUrlLocation + currentText.substring(formattedUrlLocation.length());
    }

    @Override
    public String getTextToPaste() {
        Context context = ContextUtils.getApplicationContext();

        ClipboardManager clipboard =
                (ClipboardManager) context.getSystemService(Context.CLIPBOARD_SERVICE);
        ClipData clipData = clipboard.getPrimaryClip();
        if (clipData == null) return null;

        StringBuilder builder = new StringBuilder();
        for (int i = 0; i < clipData.getItemCount(); i++) {
            builder.append(clipData.getItemAt(i).coerceToText(context));
        }

        String stringToPaste = sanitizeTextForPaste(builder.toString());
        recordPasteMetrics(stringToPaste);
        return stringToPaste;
    }

    @VisibleForTesting
    protected String sanitizeTextForPaste(String text) {
        return OmniboxViewUtil.sanitizeTextForPaste(text);
    }

    /**
     * Returns the portion of the URL that precedes the path/query section of the URL.
     *
     * @param url The url to be used to find the preceding portion.
     * @param host The host to be located in the URL to determine the location of the path.
     * @return The URL contents that precede the path (or the passed in URL if the host is
     *         not found).
     */
    private static String getUrlContentsPrePath(String url, String host) {
        int hostIndex = url.indexOf(host);
        if (hostIndex == -1) return url;

        int pathIndex = url.indexOf('/', hostIndex);
        if (pathIndex <= 0) return url;

        return url.substring(0, pathIndex);
    }

    /** @see UrlTextChangeListener */
    @Override
    public void onTextChanged(String textWithoutAutocomplete, String textWithAutocomplete) {
        for (int i = 0; i < mUrlTextChangeListeners.size(); i++) {
            mUrlTextChangeListeners.get(i).onTextChanged(
                    textWithoutAutocomplete, textWithAutocomplete);
        }
    }

    /** @see TextWatcher */
    @Override
    public void beforeTextChanged(CharSequence s, int start, int count, int after) {
        for (int i = 0; i < mTextChangedListeners.size(); i++) {
            mTextChangedListeners.get(i).beforeTextChanged(s, start, count, after);
        }
    }

    /** @see TextWatcher */
    @Override
    public void onTextChanged(CharSequence s, int start, int before, int count) {
        for (int i = 0; i < mTextChangedListeners.size(); i++) {
            mTextChangedListeners.get(i).onTextChanged(s, start, before, count);
        }
    }

    /** @see TextWatcher */
    @Override
    public void afterTextChanged(Editable editable) {
        for (int i = 0; i < mTextChangedListeners.size(); i++) {
            mTextChangedListeners.get(i).afterTextChanged(editable);
        }
    }

    private void recordPasteMetrics(String text) {
        boolean isUrl = BrowserStartupController.get(LibraryProcessType.PROCESS_BROWSER)
                                .isFullBrowserStarted()
                && AutocompleteCoordinatorFactory.qualifyPartialURLQuery(text) != null;

        long age = System.currentTimeMillis() - Clipboard.getInstance().getLastModifiedTimeMs();
        RecordHistogram.recordCustomTimesHistogram("MobileOmnibox.LongPressPasteAge", age,
                MIN_TIME_MILLIS, MAX_TIME_MILLIS, NUM_OF_BUCKETS);
        if (isUrl) {
            RecordHistogram.recordCustomTimesHistogram("MobileOmnibox.LongPressPasteAge.URL", age,
                    MIN_TIME_MILLIS, MAX_TIME_MILLIS, NUM_OF_BUCKETS);
        } else {
            RecordHistogram.recordCustomTimesHistogram("MobileOmnibox.LongPressPasteAge.TEXT", age,
                    MIN_TIME_MILLIS, MAX_TIME_MILLIS, NUM_OF_BUCKETS);
        }
    }
}

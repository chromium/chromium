// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import android.text.TextWatcher;
import android.view.ActionMode;

import androidx.annotation.IntDef;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.WindowDelegate;
import org.chromium.chrome.browser.omnibox.UrlBar.ScrollType;
import org.chromium.chrome.browser.omnibox.UrlBar.UrlBarDelegate;
import org.chromium.chrome.browser.omnibox.UrlBar.UrlTextChangeListener;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Coordinates the interactions with the UrlBar text component.
 */
public class UrlBarCoordinator implements UrlBarEditingTextStateProvider {
    /** Specified how the text should be selected when focused. */
    @IntDef({SelectionState.SELECT_ALL, SelectionState.SELECT_END})
    @Retention(RetentionPolicy.SOURCE)
    public @interface SelectionState {
        /** Select all of the text. */
        int SELECT_ALL = 0;

        /** Selection (along with the input cursor) will be placed at the end of the text. */
        int SELECT_END = 1;
    }

    private UrlBar mUrlBar;
    private UrlBarMediator mMediator;

    /**
     * Constructs a coordinator for the given UrlBar view.
     *
     * @param urlBar The {@link UrlBar} view this coordinator encapsulates.
     * @param windowDelegate Delegate for accessing and mutating window properties, e.g. soft input
     *         mode.
     * @param actionModeCallback Callback to handle changes in contextual action Modes.
     * @param focusChangeCallback The callback that will be notified when focus changes on the
     *         UrlBar.
     * @param delegate The primary delegate for the UrlBar view.
     */
    public UrlBarCoordinator(@NonNull UrlBar urlBar, @Nullable WindowDelegate windowDelegate,
            @NonNull ActionMode.Callback actionModeCallback,
            @NonNull Callback<Boolean> focusChangeCallback, @NonNull UrlBarDelegate delegate) {
        mUrlBar = urlBar;

        PropertyModel model =
                new PropertyModel.Builder(UrlBarProperties.ALL_KEYS)
                        .with(UrlBarProperties.ACTION_MODE_CALLBACK, actionModeCallback)
                        .with(UrlBarProperties.WINDOW_DELEGATE, windowDelegate)
                        .with(UrlBarProperties.DELEGATE, delegate)
                        .build();
        PropertyModelChangeProcessor.create(model, urlBar, UrlBarViewBinder::bind);

        mMediator = new UrlBarMediator(model, focusChangeCallback);
    }

    public void destroy() {
        mMediator.destroy();
        mMediator = null;
        mUrlBar.destroy();
        mUrlBar = null;
    }

    /** @see UrlBarMediator#addUrlTextChangeListener(UrlTextChangeListener) */
    public void addUrlTextChangeListener(UrlTextChangeListener listener) {
        mMediator.addUrlTextChangeListener(listener);
    }

    /** @see TextWatcher */
    public void addTextChangedListener(TextWatcher textWatcher) {
        mMediator.addTextChangedListener(textWatcher);
    }

    /** @see UrlBarMediator#setUrlBarData(UrlBarData, int, int) */
    public boolean setUrlBarData(
            UrlBarData data, @ScrollType int scrollType, @SelectionState int state) {
        return mMediator.setUrlBarData(data, scrollType, state);
    }

    /** @see UrlBarMediator#setAutocompleteText(String, String) */
    public void setAutocompleteText(String userText, String autocompleteText) {
        mMediator.setAutocompleteText(userText, autocompleteText);
    }

    /** @see UrlBarMediator#setUseDarkTextColors(boolean) */
    public boolean setUseDarkTextColors(boolean useDarkColors) {
        return mMediator.setUseDarkTextColors(useDarkColors);
    }

    /** @see UrlBarMediator#setAllowFocus(boolean) */
    public void setAllowFocus(boolean allowFocus) {
        mMediator.setAllowFocus(allowFocus);
    }

    /** @see UrlBarMediator#setUrlDirectionListener(Callback<Integer>) */
    public void setUrlDirectionListener(Callback<Integer> listener) {
        mMediator.setUrlDirectionListener(listener);
    }

    /** Selects all of the text of the UrlBar. */
    public void selectAll() {
        mUrlBar.selectAll();
    }

    @Override
    public int getSelectionStart() {
        return mUrlBar.getSelectionStart();
    }

    @Override
    public int getSelectionEnd() {
        return mUrlBar.getSelectionEnd();
    }

    @Override
    public boolean shouldAutocomplete() {
        return mUrlBar.shouldAutocomplete();
    }

    @Override
    public boolean wasLastEditPaste() {
        return mUrlBar.wasLastEditPaste();
    }

    @Override
    public String getTextWithAutocomplete() {
        return mUrlBar.getTextWithAutocomplete();
    }

    @Override
    public String getTextWithoutAutocomplete() {
        return mUrlBar.getTextWithoutAutocomplete();
    }
}

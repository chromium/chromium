// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import android.text.TextWatcher;
import android.view.ActionMode;

import androidx.annotation.IntDef;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.WindowDelegate;
import org.chromium.chrome.browser.omnibox.UrlBar.ScrollType;
import org.chromium.chrome.browser.omnibox.UrlBar.UrlBarDelegate;
import org.chromium.chrome.browser.omnibox.UrlBar.UrlDirectionListener;
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

    private final UrlBar mUrlBar;
    private final UrlBarMediator mMediator;

    /**
     * Constructs a coordinator for the given UrlBar view.
     */
    public UrlBarCoordinator(UrlBar urlBar) {
        mUrlBar = urlBar;

        PropertyModel model = new PropertyModel(UrlBarProperties.ALL_KEYS);
        PropertyModelChangeProcessor.create(model, urlBar, UrlBarViewBinder::bind);

        mMediator = new UrlBarMediator(model);
    }

    /** @see UrlBarMediator#setDelegate(UrlBarDelegate) */
    public void setDelegate(UrlBarDelegate delegate) {
        mMediator.setDelegate(delegate);
    }

    /** @see UrlBarMediator#setDelegate(UrlBarDelegate) */
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

    /** @see UrlBarMediator#setUrlDirectionListener(UrlDirectionListener) */
    public void setUrlDirectionListener(UrlDirectionListener listener) {
        mMediator.setUrlDirectionListener(listener);
    }

    /** @see UrlBarMediator#setOnFocusChangedCallback(Callback) */
    public void setOnFocusChangedCallback(Callback<Boolean> callback) {
        mMediator.setOnFocusChangedCallback(callback);
    }

    /** @see UrlBarMediator#setWindowDelegate(WindowDelegate) */
    public void setWindowDelegate(WindowDelegate windowDelegate) {
        mMediator.setWindowDelegate(windowDelegate);
    }

    /** @see UrlBarMediator#setActionModeCallback(android.view.ActionMode.Callback) */
    public void setActionModeCallback(ActionMode.Callback callback) {
        mMediator.setActionModeCallback(callback);
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

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.search_engines.choice_screen;

import android.text.TextUtils;

import androidx.annotation.CallSuper;

import com.google.common.collect.ImmutableList;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.search_engines.DefaultSearchEngineDialogHelper;
import org.chromium.chrome.browser.search_engines.SearchEnginePromoType;
import org.chromium.components.search_engines.TemplateUrl;

import java.util.stream.Collectors;

/**
 * Represents the backend API exposed to the content screen.
 *
 * Adapts the API defined by {@link DefaultSearchEngineDialogHelper.Delegate} for this component.
 */
class ChoiceScreenDelegate {
    private final Callback<Boolean> mOnClosedCallback;
    private final DefaultSearchEngineDialogHelper.Delegate mWrappedDelegate;

    private final ImmutableList<TemplateUrl> mTemplateUrls;

    ChoiceScreenDelegate(DefaultSearchEngineDialogHelper.Delegate wrappedDelegate,
            Callback<Boolean> onClosedCallback) {
        mOnClosedCallback = onClosedCallback;
        mWrappedDelegate = wrappedDelegate;
        mTemplateUrls = ImmutableList.copyOf(
                wrappedDelegate.getSearchEnginesForPromoDialog(SearchEnginePromoType.SHOW_WAFFLE));
        assert !mTemplateUrls.isEmpty();
    }

    /**
     * Returns the list of search engines that a user may choose between. The list is cached and
     * will be unchanged during the lifetime of the delegate. It is intended to be shown as-is
     * (for example without altering the order of the items).
     */
    ImmutableList<TemplateUrl> getSearchEngines() {
        return mTemplateUrls;
    }

    /**
     * Will be called when the user completed their selection.
     *
     * Can be overridden if something the view's embedder needs to do something after the choice is
     * completed, like closing the dialog or advancing in the flow for example.
     * @param keyword The keyword for the chosen search engine, per {@link
     *         TemplateUrl#getKeyword()}.
     */
    @CallSuper
    void onChoiceMade(String keyword) {
        assert !TextUtils.isEmpty(keyword);

        mWrappedDelegate.onUserSearchEngineChoice(SearchEnginePromoType.SHOW_WAFFLE,
                mTemplateUrls.stream().map(TemplateUrl::getKeyword).collect(Collectors.toList()),
                keyword);
        mOnClosedCallback.onResult(true);
    }

    /** Will be called when the user exits the choice screen without making a choice. */
    void onExitWithoutChoice() {
        mOnClosedCallback.onResult(false);
    }
}

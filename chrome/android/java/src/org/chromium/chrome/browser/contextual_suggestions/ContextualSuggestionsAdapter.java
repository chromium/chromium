// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextual_suggestions;

import android.view.ViewGroup;

import org.chromium.chrome.browser.modelutil.RecyclerViewAdapter;
import org.chromium.chrome.browser.native_page.ContextMenuManager;
import org.chromium.chrome.browser.ntp.cards.ItemViewType;
import org.chromium.chrome.browser.ntp.cards.NewTabPageViewHolder;
import org.chromium.chrome.browser.ntp.cards.NewTabPageViewHolder.PartialBindCallback;
import org.chromium.chrome.browser.ntp.snippets.SectionHeaderViewHolder;
import org.chromium.chrome.browser.offlinepages.OfflinePageBridge;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.suggestions.SuggestionsRecyclerView;
import org.chromium.chrome.browser.suggestions.SuggestionsUiDelegate;
import org.chromium.chrome.browser.widget.displaystyle.UiConfig;

/**
 * An adapter that contains the view binder for the content component.
 */
class ContextualSuggestionsAdapter
        extends RecyclerViewAdapter<NewTabPageViewHolder, PartialBindCallback> {
    private static class ContextualSuggestionsViewHolderFactory
            implements ViewHolderFactory<NewTabPageViewHolder> {
        private final Profile mProfile;
        private final UiConfig mUiConfig;
        private final SuggestionsUiDelegate mUiDelegate;
        private final ContextMenuManager mContextMenuManager;

        public ContextualSuggestionsViewHolderFactory(Profile profile, UiConfig uiConfig,
                SuggestionsUiDelegate uiDelegate, ContextMenuManager contextMenuManager) {
            mProfile = profile;
            mUiConfig = uiConfig;
            mUiDelegate = uiDelegate;
            mContextMenuManager = contextMenuManager;
        }

        @Override
        public NewTabPageViewHolder createViewHolder(ViewGroup parent, int viewType) {
            switch (viewType) {
                case ItemViewType.HEADER:
                    return new SectionHeaderViewHolder((SuggestionsRecyclerView) parent, mUiConfig);

                case ItemViewType.SNIPPET:
                    return new ContextualSuggestionCardViewHolder((SuggestionsRecyclerView) parent,
                            mContextMenuManager, mUiDelegate, mUiConfig,
                            OfflinePageBridge.getForProfile(mProfile));

                default:
                    assert false;
                    return null;
            }
        }
    }

    /**
     * Construct a new {@link ContextualSuggestionsAdapter}.
     * @param profile The regular {@link Profile}.
     * @param uiConfig The {@link UiConfig} used to adjust view display.
     * @param uiDelegate The {@link SuggestionsUiDelegate} used to help construct items in the
     *                   content view.
     * @param contextMenuManager The {@link ContextMenuManager} used to display a context menu.
     * @param delegate The {@link Delegate} implementing the core logic.
     */
    ContextualSuggestionsAdapter(Profile profile, UiConfig uiConfig,
            SuggestionsUiDelegate uiDelegate, ContextMenuManager contextMenuManager,
            RecyclerViewAdapter.Delegate<NewTabPageViewHolder, PartialBindCallback> delegate) {
        super(delegate,
                new ContextualSuggestionsViewHolderFactory(
                        profile, uiConfig, uiDelegate, contextMenuManager));
    }

    @Override
    public void onViewRecycled(NewTabPageViewHolder holder) {
        holder.recycle();
    }
}

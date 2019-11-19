// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp.cards;

import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;

import org.chromium.chrome.browser.ntp.cards.NewTabPageViewHolder.PartialBindCallback;
import org.chromium.chrome.browser.ntp.snippets.CategoryInt;
import org.chromium.chrome.browser.ntp.snippets.SectionHeaderViewHolder;
import org.chromium.chrome.browser.ntp.snippets.SnippetArticleViewHolder;
import org.chromium.chrome.browser.ui.widget.displaystyle.HorizontalDisplayStyle;
import org.chromium.chrome.browser.ui.widget.displaystyle.UiConfig;
import org.chromium.chrome.browser.ui.widget.displaystyle.VerticalDisplayStyle;

/**
 * JUnit specific utility classes for testing suggestions code. Other utility code is in
 * {@link org.chromium.chrome.test.util.browser.suggestions.ContentSuggestionsTestUtils}.
 */
public final class ContentSuggestionsUnitTestUtils {
    private ContentSuggestionsUnitTestUtils() {}

    public static UiConfig makeUiConfig(
            @HorizontalDisplayStyle int horizontal, @VerticalDisplayStyle int vertical) {
        UiConfig uiConfig = mock(UiConfig.class);
        when(uiConfig.getCurrentDisplayStyle())
                .thenReturn(new UiConfig.DisplayStyle(horizontal, vertical));
        return uiConfig;
    }

    public static UiConfig makeUiConfig() {
        return makeUiConfig(HorizontalDisplayStyle.REGULAR, VerticalDisplayStyle.REGULAR);
    }

    public static void bindViewHolders(InnerNode<NewTabPageViewHolder, PartialBindCallback> node) {
        bindViewHolders(node, 0, node.getItemCount());
    }

    public static void bindViewHolders(InnerNode<NewTabPageViewHolder, PartialBindCallback> node,
            int startIndex, int endIndex) {
        for (int i = startIndex; i < endIndex; ++i) {
            node.onBindViewHolder(makeViewHolder(node.getItemViewType(i)), i, null);
        }
    }

    private static NewTabPageViewHolder makeViewHolder(@CategoryInt int viewType) {
        switch (viewType) {
            case ItemViewType.SNIPPET:
                return mock(SnippetArticleViewHolder.class);
            case ItemViewType.HEADER:
                return mock(SectionHeaderViewHolder.class);
            case ItemViewType.STATUS:
                return mock(StatusCardViewHolder.class);
            case ItemViewType.ACTION:
                return mock(ActionItem.ViewHolder.class);
            case ItemViewType.PROGRESS:
                return mock(ProgressViewHolder.class);
            default:
                return mock(NewTabPageViewHolder.class);
        }
    }
}

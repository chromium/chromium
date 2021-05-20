// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextmenu;

import android.text.TextUtils;
import android.webkit.URLUtil;

import org.chromium.components.embedder_support.contextmenu.ContextMenuParams;

/**
 * Provides utility methods for generating context menus.
 */
public final class ContextMenuUtils {
    private ContextMenuUtils() {}

    /**
     * Returns the title for the given {@link ContextMenuParams}.
     */
    static String getTitle(ContextMenuParams params) {
        if (!TextUtils.isEmpty(params.getTitleText())) {
            return params.getTitleText();
        }
        if (!TextUtils.isEmpty(params.getLinkText())) {
            return params.getLinkText();
        }
        if (params.isImage() || params.isVideo() || params.isFile()) {
            return URLUtil.guessFileName(params.getSrcUrl().getSpec(), null, null);
        }
        return "";
    }

    /**
     * Get the suffix for the context menu type determined by the params. Histogram values should
     * match with the values defined in histogram_suffixes_list.xml under ContextMenuTypeAndroid
     * @param params The list of params for the opened context menu.
     * @return A string value for the histogram suffix.
     */
    static String getContextMenuTypeForHistogram(ContextMenuParams params) {
        if (params.isVideo()) {
            return "Video";
        } else if (params.isImage()) {
            return params.isAnchor() ? "ImageLink" : "Image";
        } else if (params.getOpenedFromHighlight()) {
            return "SharedHighlightingInteraction";
        }
        assert params.isAnchor();
        return "Link";
    }
}

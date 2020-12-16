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
}

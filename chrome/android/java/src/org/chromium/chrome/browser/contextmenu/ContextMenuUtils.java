// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextmenu;

import android.text.TextUtils;
import android.webkit.URLUtil;

import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.components.embedder_support.contextmenu.ContextMenuParams;
import org.chromium.content_public.browser.ContentFeatureList;
import org.chromium.content_public.common.ContentFeatures;

/**
 * Provides utility methods for generating context menus.
 */
public final class ContextMenuUtils {
    /** Experiment params to hide the context menu header image. */
    @VisibleForTesting
    public static final String HIDE_HEADER_IMAGE_PARAM = "hide_header_image";

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

    /** Whether to force using popup style for context menu. */
    static boolean forcePopupStyleEnabled() {
        return ChromeFeatureList.isEnabled(ChromeFeatureList.CONTEXT_MENU_POPUP_STYLE)
                || ContentFeatureList.isEnabled(ContentFeatures.TOUCH_DRAG_AND_CONTEXT_MENU);
    }

    /**
     * Whether hide the context menu header image by field trial params. The value is read from 
     * either {@link ContentFeatures.TOUCH_DRAG_AND_CONTEXT_MENU} or
     * {@link ChromeFeatureList.CONTEXT_MENU_POPUP_STYLE}.
     */
    static boolean hideContextMenuHeaderImage() {
        int valueHideHeaderFromDragAndDrop = ContentFeatureList.getFieldTrialParamByFeatureAsInt(
                ContentFeatures.TOUCH_DRAG_AND_CONTEXT_MENU, HIDE_HEADER_IMAGE_PARAM, -1);
        if (valueHideHeaderFromDragAndDrop != -1) {
            return valueHideHeaderFromDragAndDrop != 0;
        }

        return ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                ChromeFeatureList.CONTEXT_MENU_POPUP_STYLE, HIDE_HEADER_IMAGE_PARAM, false);
    }
}

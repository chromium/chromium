// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import android.net.Uri;
import android.text.Spanned;
import android.text.TextUtils;

import androidx.annotation.Nullable;

import org.chromium.base.CollectionUtil;
import org.chromium.chrome.browser.util.UrlConstants;
import org.chromium.content_public.common.ContentUrlConstants;

import java.util.HashSet;

/**
 * Encapsulates all data that is necessary for the URL bar to display its contents.
 */
public class UrlBarData {
    /**
     * The URL schemes that should be displayed complete with path.
     */
    public static final HashSet<String> UNSUPPORTED_SCHEMES_TO_SPLIT =
            CollectionUtil.newHashSet(ContentUrlConstants.ABOUT_SCHEME, UrlConstants.DATA_SCHEME,
                    UrlConstants.FILE_SCHEME, UrlConstants.FTP_SCHEME, UrlConstants.INLINE_SCHEME,
                    UrlConstants.JAVASCRIPT_SCHEME, UrlConstants.CHROME_SCHEME);
    /**
     * URI schemes that ContentView can handle.
     *
     * Copied from UrlUtilities.java.  UrlUtilities uses a URI to check for schemes, which
     * is more strict than Uri and causes the path stripping to fail.
     *
     * The following additions have been made: "chrome", "ftp".
     */
    private static final HashSet<String> ACCEPTED_SCHEMES = CollectionUtil.newHashSet(
            ContentUrlConstants.ABOUT_SCHEME, UrlConstants.DATA_SCHEME, UrlConstants.FILE_SCHEME,
            UrlConstants.FTP_SCHEME, UrlConstants.HTTP_SCHEME, UrlConstants.HTTPS_SCHEME,
            UrlConstants.INLINE_SCHEME, UrlConstants.JAVASCRIPT_SCHEME, UrlConstants.CHROME_SCHEME);
    // Unicode "Left-To-Right Mark" (LRM) character.
    private static final char LRM = '\u200E';

    /**
     * Represents an empty URL bar.
     */
    public static final UrlBarData EMPTY = forNonUrlText("");

    public static UrlBarData forUrl(String url) {
        return forUrlAndText(url, url, null);
    }

    public static UrlBarData forNonUrlText(String displayText) {
        return create(null, displayText, 0, 0, null);
    }

    public static UrlBarData forUrlAndText(String url, String displayText) {
        return forUrlAndText(url, displayText, null);
    }

    public static UrlBarData forUrlAndText(
            String url, CharSequence displayText, @Nullable String editingText) {
        int pathSearchOffset = 0;
        String displayTextStr = displayText.toString();
        String scheme = Uri.parse(displayTextStr).getScheme();

        if (!TextUtils.isEmpty(scheme)) {
            if (UNSUPPORTED_SCHEMES_TO_SPLIT.contains(scheme)) {
                return create(url, displayText, 0, displayText.length(), editingText);
            }
            if (ACCEPTED_SCHEMES.contains(scheme)) {
                for (pathSearchOffset = scheme.length(); pathSearchOffset < displayText.length();
                        pathSearchOffset++) {
                    char c = displayText.charAt(pathSearchOffset);
                    if (c != ':' && c != '/') break;
                }
            }
        }
        int pathOffset = -1;
        if (pathSearchOffset < displayText.length()) {
            pathOffset = displayTextStr.indexOf('/', pathSearchOffset);
        }
        if (pathOffset == -1) return create(url, displayText, 0, displayText.length(), editingText);

        // If the '/' is the last character and the beginning of the path, then just drop
        // the path entirely.
        if (pathOffset == displayText.length() - 1) {
            String prePathText = displayTextStr.substring(0, pathOffset);
            return create(url, prePathText, 0, prePathText.length(), editingText);
        }

        return create(url, displayText, 0, pathOffset, editingText);
    }

    public static UrlBarData create(@Nullable String url, CharSequence displayText,
            int originStartIndex, int originEndIndex, @Nullable String editingText) {
        return new UrlBarData(url, displayText, originStartIndex, originEndIndex, editingText);
    }

    /**
     * The canonical URL that is shown in the URL bar, or null if it currently does not correspond
     * to a URL (for example, showing suggestion text).
     */
    public final @Nullable String url;

    /**
     * The text that should be shown in the URL bar. This can be a {@link Spanned} that contains
     * formatting to highlight parts of the display text.
     */
    public final CharSequence displayText;

    /**
     * The text that should replace the display text when editing the contents of the URL bar,
     * or null to use the {@link #displayText} when editing.
     */
    public final @Nullable String editingText;

    /**
     * The character index in {@link #displayText} where the origin starts. This is required to
     * ensure that the end of the origin is not scrolled out of view for long hostnames.
     */
    public final int originStartIndex;

    /**
     * The character index in {@link #displayText} where the origin ends. This is required to
     * ensure that the end of the origin is not scrolled out of view for long hostnames.
     */
    public final int originEndIndex;

    /**
     * @return The text for editing, falling back to the display text if the former is null.
     */
    public CharSequence getEditingOrDisplayText() {
        return editingText != null ? editingText : displayText;
    }

    private UrlBarData(@Nullable String url, CharSequence displayText, int originStartIndex,
            int originEndIndex, @Nullable String editingText) {
        this.url = url;
        this.displayText = displayText;
        this.originStartIndex = originStartIndex;
        this.originEndIndex = originEndIndex;
        this.editingText = editingText;
    }
}

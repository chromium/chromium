// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextual_tasks;

import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.content_public.browser.WebContents;
import org.chromium.url.GURL;

/** Utility methods for Contextual Tasks. */
@NullMarked
public final class ContextualTasksUtils {
    /** The host for the Contextual Tasks WebUI. */
    public static final String CONTEXTUAL_TASKS_HOST = "contextual-tasks";

    /**
     * Returns whether the given URL is a contextual tasks WebUI URL.
     *
     * @param gurl The URL to check.
     * @return True if it is a contextual tasks URL.
     */
    public static boolean isContextualTasksUrl(@Nullable GURL gurl) {
        if (GURL.isEmptyOrInvalid(gurl)) return false;
        return gurl.getScheme().equals(UrlConstants.CHROME_SCHEME)
                && gurl.getHost().equals(CONTEXTUAL_TASKS_HOST);
    }

    /**
     * Returns the "pretty" version of the AI Mode URL for display and editing.
     *
     * @param webContents The web contents of the AI Mode page.
     * @return The pretty URL (e.g. chrome://google.com/search).
     */
    public static GURL getContextualTasksDisplayUrl(WebContents webContents) {
        return ContextualTasksUtilsJni.get().getContextualTasksDisplayUrl(webContents);
    }

    /**
     * Returns the functional version of the AI Mode URL for copying and sharing.
     *
     * @param webContents The web contents of the AI Mode page.
     * @return The functional URL (e.g. https://www.google.com/search).
     */
    public static GURL getContextualTasksFunctionalURL(WebContents webContents) {
        return ContextualTasksUtilsJni.get().getContextualTasksFunctionalURL(webContents);
    }

    /**
     * Returns the replacement text for cut/copy if the selected text matches the AI Mode URL.
     *
     * @param currentText The current text in the URL bar.
     * @param selectionStart The start of the selection.
     * @param selectionEnd The end of the selection.
     * @param functionalGurl The functional URL to replace with.
     * @return The replacement URL string, or null if no replacement should occur.
     */
    public static @Nullable String getReplacementUrl(
            String currentText, int selectionStart, int selectionEnd, GURL functionalGurl) {
        return ContextualTasksUtilsJni.get()
                .getReplacementUrl(currentText, selectionStart, selectionEnd, functionalGurl);
    }

    @NativeMethods
    public interface Natives {
        @JniType("GURL")
        GURL getContextualTasksDisplayUrl(
                @JniType("content::WebContents*") WebContents webContents);

        @JniType("GURL")
        GURL getContextualTasksFunctionalURL(
                @JniType("content::WebContents*") WebContents webContents);

        String getReplacementUrl(
                @JniType("std::u16string") String currentText,
                int selectionStart,
                int selectionEnd,
                @JniType("GURL") GURL functionalGurl);
    }
}

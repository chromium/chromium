// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextual_tasks;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.embedder_support.util.UrlConstants;
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
}

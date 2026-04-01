// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextual_tasks;

import org.chromium.build.annotations.NullMarked;
import org.chromium.components.embedder_support.util.UrlConstants;

/** Utility class for contextual tasks. */
@NullMarked
public class ContextualTasksUtils {
    /** The query parameter name for the task ID. */
    public static final String URL_QUERY_PARAM_TASK_ID = "chrome_task_id";

    private static final String WEB_UI_URL_PREFIX =
            UrlConstants.CHROME_URL_PREFIX
                    + UrlConstants.CONTEXTUAL_TASKS_WEBUI_HOST
                    + "/?"
                    + URL_QUERY_PARAM_TASK_ID
                    + "=";

    /**
     * Returns the WebUI URL for a given task ID.
     *
     * @param taskId The ID of the task.
     * @return The WebUI URL.
     */
    public static String createWebUiUrl(String taskId) {
        return WEB_UI_URL_PREFIX + taskId;
    }
}

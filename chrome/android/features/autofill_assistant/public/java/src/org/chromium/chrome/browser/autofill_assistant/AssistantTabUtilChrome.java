// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import android.app.Activity;

import org.chromium.base.task.PostTask;
import org.chromium.chrome.browser.customtabs.CustomTabActivity;
import org.chromium.components.autofill_assistant.AssistantTabUtil;
import org.chromium.content_public.browser.UiThreadTaskTraits;

/**
 * Implementation of {@link AssistantTabUtil} for Chrome.
 */
public class AssistantTabUtilChrome implements AssistantTabUtil {
    @Override
    public void scheduleCloseCustomTab(Activity activity) {
        if (!(activity instanceof CustomTabActivity)) {
            return;
        }

        PostTask.postTask(UiThreadTaskTraits.DEFAULT, activity::finish);
    }
}

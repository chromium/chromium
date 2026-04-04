// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextual_tasks;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;

/** Unit tests for {@link ContextualTasksUtils}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ContextualTasksUtilsUnitTest {
    @Test
    public void testCreateWebUiUrl() {
        String taskId = "test-task-id";
        String expectedUrl = "chrome://contextual-tasks/?chrome_task_id=test-task-id";
        Assert.assertEquals(expectedUrl, ContextualTasksUtils.createWebUiUrl(taskId));
    }
}

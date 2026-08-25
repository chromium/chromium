// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextual_tasks;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.FeatureList;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;

/** Unit tests for {@link ContextualTasksUtils}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ContextualTasksUtilsUnitTest {

    @Test
    public void testIsContextualTasksUiEnabled_preNative_defaultDisabled() {
        assertFalse(FeatureList.isNativeInitialized());
        assertFalse(ContextualTasksUtils.isContextualTasksUiEnabled());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.CONTEXTUAL_TASKS_SIDE_PANEL)
    public void testIsContextualTasksUiEnabled_preNative_sidePanelEnabled() {
        assertFalse(FeatureList.isNativeInitialized());
        assertTrue(ContextualTasksUtils.isContextualTasksUiEnabled());
    }

    @Test
    @DisableFeatures(ChromeFeatureList.CONTEXTUAL_TASKS_SIDE_PANEL)
    public void testIsContextualTasksUiEnabled_preNative_sidePanelDisabled() {
        assertFalse(FeatureList.isNativeInitialized());
        assertFalse(ContextualTasksUtils.isContextualTasksUiEnabled());
    }
}

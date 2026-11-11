// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tasks.tab_management.TabListMediator.TabListLayoutType;

/** Unit tests for {@link TabUiFeatureUtilities}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TabUiFeatureUtilitiesUnitTest {
    @Test
    @DisableFeatures(ChromeFeatureList.TAB_UI_DATA_VIRTUALIZATION)
    public void testDisabled_returnsFalseForAllLayouts() {
        assertFalse(TabUiFeatureUtilities.isTabUiDataVirtualizationEnabled(TabListLayoutType.FLAT));
        assertFalse(
                TabUiFeatureUtilities.isTabUiDataVirtualizationEnabled(TabListLayoutType.GROUPED));
        assertFalse(
                TabUiFeatureUtilities.isTabUiDataVirtualizationEnabled(TabListLayoutType.NESTED));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.TAB_UI_DATA_VIRTUALIZATION)
    @DisableFeatures(ChromeFeatureList.ANDROID_TAB_UI_REFACTOR)
    public void testFlat_enabledWithoutRefactor() {
        assertTrue(TabUiFeatureUtilities.isTabUiDataVirtualizationEnabled(TabListLayoutType.FLAT));
        assertTrue(
                TabUiFeatureUtilities.isTabUiDataVirtualizationEnabled(TabListLayoutType.NESTED));
        // Grouped surfaces depend on the Android Tab UI refactor.
        assertFalse(
                TabUiFeatureUtilities.isTabUiDataVirtualizationEnabled(TabListLayoutType.GROUPED));
    }

    @Test
    @EnableFeatures({
        ChromeFeatureList.TAB_UI_DATA_VIRTUALIZATION,
        ChromeFeatureList.ANDROID_TAB_UI_REFACTOR
    })
    public void testGrouped_requiresRefactor() {
        assertTrue(
                TabUiFeatureUtilities.isTabUiDataVirtualizationEnabled(TabListLayoutType.GROUPED));
    }

    @Test
    @EnableFeatures({
        ChromeFeatureList.TAB_UI_DATA_VIRTUALIZATION,
        ChromeFeatureList.TAB_UI_DATA_VIRTUALIZATION + ":enable_flat/false"
    })
    public void testFlatParamOff_returnsFalse() {
        assertFalse(TabUiFeatureUtilities.isTabUiDataVirtualizationEnabled(TabListLayoutType.FLAT));
        assertTrue(
                TabUiFeatureUtilities.isTabUiDataVirtualizationEnabled(TabListLayoutType.NESTED));
    }
}

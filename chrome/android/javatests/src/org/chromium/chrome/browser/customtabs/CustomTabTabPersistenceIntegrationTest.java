// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import android.support.test.InstrumentationRegistry;
import android.support.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.customtabs.content.CustomTabActivityNavigationController.FinishReason;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabState;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.common.ContentUrlConstants;

import java.io.File;
import java.util.Locale;

/**
 * Integration testing for the CustomTab Tab persistence logic.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class CustomTabTabPersistenceIntegrationTest {
    @Rule
    public CustomTabActivityTestRule mCustomTabActivityTestRule = new CustomTabActivityTestRule();

    @Before
    public void setUp() {
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(
                CustomTabsTestUtils.createMinimalCustomTabIntent(
                        InstrumentationRegistry.getTargetContext(),
                        ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL));
    }

    @Test
    @MediumTest
    public void testTabFilesDeletedOnClose() {
        Tab tab = mCustomTabActivityTestRule.getActivity().getActivityTab();
        String expectedTabFileName = TabState.getTabStateFilename(tab.getId(), false);

        CustomTabTabPersistencePolicy tabPersistencePolicy = mCustomTabActivityTestRule
                .getActivity().getComponent().resolveTabPersistencePolicy();

        String expectedMetadataFileName = tabPersistencePolicy.getStateFileName();
        File stateDir = tabPersistencePolicy.getOrCreateStateDirectory();

        waitForFileExistState(true, expectedTabFileName, stateDir);
        waitForFileExistState(true, expectedMetadataFileName, stateDir);

        mCustomTabActivityTestRule.getActivity().getComponent()
                .resolveNavigationController().finish(FinishReason.OTHER);

        waitForFileExistState(false, expectedTabFileName, stateDir);
        waitForFileExistState(false, expectedMetadataFileName, stateDir);
    }

    private void waitForFileExistState(
            final boolean exists, final String fileName, final File filePath) {
        CriteriaHelper.pollInstrumentationThread(new Criteria(
                String.format(Locale.US, "File, %s, expected to exist: %b", fileName, exists)) {
            @Override
            public boolean isSatisfied() {
                File file = new File(filePath, fileName);
                return file.exists() == exists;
            }
        });
    }

}

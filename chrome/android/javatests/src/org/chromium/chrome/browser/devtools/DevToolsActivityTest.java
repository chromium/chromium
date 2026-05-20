// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.devtools;

import static org.junit.Assert.assertTrue;

import static org.chromium.base.ThreadUtils.runOnUiThreadBlocking;

import androidx.test.filters.MediumTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabClosureParams;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.chrome.test.util.ActivityTestUtils;
import org.chromium.content_public.browser.ContentFeatureList;
import org.chromium.content_public.browser.WebContents;

/** This class tests the functionality of the {@link DevToolsActivity}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@EnableFeatures(ContentFeatureList.ANDROID_DEV_TOOLS_FRONTEND)
@Batch(Batch.PER_CLASS)
public class DevToolsActivityTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public FreshCtaTransitTestRule mTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();

    @Before
    public void setUp() {
        mTestRule.startOnBlankPage();
    }

    @Test
    @MediumTest
    public void testOpenDevTools() {
        TabModel tabModel = mTestRule.getActivity().getCurrentTabModel();
        Tab inspectedTab = tabModel.getCurrentTabSupplier().get();
        Profile profile = inspectedTab.getProfile();
        WebContents inspectedContents = inspectedTab.getWebContents();
        assertTrue(
                runOnUiThreadBlocking(
                        () ->
                                DevToolsWindowAndroid.isDevToolsAllowedFor(
                                        profile, inspectedContents)));

        // Calling openDevTools() shows a DevToolsActivity.
        DevToolsActivity activity =
                ActivityTestUtils.waitForActivity(
                        InstrumentationRegistry.getInstrumentation(),
                        DevToolsActivity.class,
                        () ->
                                runOnUiThreadBlocking(
                                        () ->
                                                DevToolsWindowAndroid.openDevTools(
                                                        inspectedContents)));

        // DevToolsActivity gets closed when the inspected tab closes.
        runOnUiThreadBlocking(
                () -> {
                    tabModel.getTabRemover()
                            .closeTabs(TabClosureParams.closeTab(inspectedTab).build(), false);
                    tabModel.commitAllTabClosures();
                });
        CriteriaHelper.pollUiThread(
                () -> {
                    return activity.isDestroyed();
                },
                "DevToolsActivity wasn't closed.");
    }
}

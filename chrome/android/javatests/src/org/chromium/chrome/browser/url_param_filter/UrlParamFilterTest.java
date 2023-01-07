// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.url_param_filter;

import android.app.Activity;
import android.support.test.InstrumentationRegistry;

import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.app.tabmodel.AsyncTabParamsManagerSingleton;
import org.chromium.chrome.browser.app.tabmodel.ChromeTabModelFilterFactory;
import org.chromium.chrome.browser.app.tabmodel.TabWindowManagerSingleton;
import org.chromium.chrome.browser.firstrun.FirstRunStatus;
import org.chromium.chrome.browser.flags.ActivityType;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.NextTabPolicy;
import org.chromium.chrome.browser.tabmodel.NextTabPolicy.NextTabPolicySupplier;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabModelFilterFactory;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorFactory;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorImpl;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.batch.BlankCTATabInitialStateRule;
import org.chromium.chrome.test.util.browser.contextmenu.ContextMenuUtils;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.test.util.Coordinates;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.util.concurrent.TimeoutException;

/**
 * Verifies URL parameters filtering on "Open in new incognito tab" when enabled.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
public class UrlParamFilterTest {
    @ClassRule
    public static ChromeTabbedActivityTestRule sActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Rule
    public BlankCTATabInitialStateRule mBlankCTATabInitialStateRule =
            new BlankCTATabInitialStateRule(sActivityTestRule, true);

    // An encoded string with parameter classifications. For more details, see
    // CreateBase64EncodedFilterParamClassificationForTesting in url_param_filter_test_helper.cc.
    // This classifies "plzblock" on destination URLs as filterable.
    private static final String LOCAL_IP_DESTINATION_PLZBLOCK =
            "H4sIAAAAAAAAAOOS5OI0NDLXMwBCQwEmKS4ujoKcqqSc%2fORsACDxPHgbAAAA";
    // Same as LOCAL_IP_DESTINATION_PLZBLOCK, but blocks plzblock on navigations emanating from
    // 127.0.0.1 instead of only on 127.0.0.1 as a destination.
    private static final String LOCAL_IP_SOURCE_PLZBLOCK =
            "H4sIAAAAAAAAAOOS5OI0NDLXMwBCQwFGKS4ujoKcqqSc%2fORsAO6d9sUbAAAA";

    private static final String HTML_PATH =
            "/chrome/test/data/android/contextmenu/context_menu_test.html";

    // The last tab opened; used to retrieve the new incognito tab.
    private static Tab sLastOpenedTab;

    // Records tabs opened and otherwise behaves like TabModelSelectorImpl.
    private static class RecordingTabModelSelector extends TabModelSelectorImpl {
        @Override
        public Tab openNewTab(LoadUrlParams loadUrlParams, @TabLaunchType int type, Tab parent,
                boolean incognito) {
            Tab result = super.openNewTab(loadUrlParams, type, parent, incognito);
            sLastOpenedTab = result;
            return result;
        }

        public RecordingTabModelSelector(Activity activity, TabCreatorManager tabCreatorManager,
                TabModelFilterFactory tabModelFilterFactory, int selectorIndex) {
            super(null, tabCreatorManager, tabModelFilterFactory,
                    ()
                            -> NextTabPolicy.HIERARCHICAL,
                    AsyncTabParamsManagerSingleton.getInstance(), false, ActivityType.TABBED,
                    false);
        }
    }

    @BeforeClass
    public static void beforeClass() throws Exception {
        // Plant RecordingTabModelSelector as the TabModelSelector used in Main. The factory has to
        // be set before super.setUp(), as super.setUp() creates Main and consequently the
        // TabModelSelector.
        TabWindowManagerSingleton.setTabModelSelectorFactoryForTesting(
                new TabModelSelectorFactory() {
                    @Override
                    public TabModelSelector buildSelector(Activity activity,
                            TabCreatorManager tabCreatorManager,
                            NextTabPolicySupplier nextTabPolicySupplier, int selectorIndex) {
                        return new RecordingTabModelSelector(activity, tabCreatorManager,
                                new ChromeTabModelFilterFactory(activity), selectorIndex);
                    }
                });
    }

    @Before
    public void setUp() throws Exception {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { FirstRunStatus.setFirstRunFlowComplete(true); });
    }

    @After
    public void tearDown() {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { FirstRunStatus.setFirstRunFlowComplete(false); });
    }

    /**
     * Verifies parameters specified by feature flags are filtered for "Open in new incognito tab",
     * and that filtering occurs when the should_filter parameter is true.
     */
    @Test
    @MediumTest
    @Feature({"Browser"})
    @CommandLineFlags.Add({"enable-features=IncognitoParamFilterEnabled:classifications/"
            + LOCAL_IP_DESTINATION_PLZBLOCK + "/should_filter/true"})
    public void
    testOpenInIncognitoDestinationFiltering() throws TimeoutException {
        triggerContextMenuLoad(sActivityTestRule.getTestServer().getURL(HTML_PATH),
                "testLinkFiltered", R.id.contextmenu_open_in_incognito_tab);

        Tab tab = sActivityTestRule.getActivity().getActivityTab();
        waitForLoad(sLastOpenedTab.getWebContents());
        Assert.assertFalse(
                "".equals(sLastOpenedTab.getWebContents().getLastCommittedUrl().getSpec()));
        Assert.assertFalse(sLastOpenedTab.getWebContents().getLastCommittedUrl().getSpec().contains(
                "plzblock"));
    }

    /**
     * Verifies parameters specified by feature flags are filtered for "Open in new incognito tab",
     * and that filtering occurs when the should_filter parameter is true.
     */
    @Test
    @MediumTest
    @Feature({"Browser"})
    @CommandLineFlags.Add({"enable-features=IncognitoParamFilterEnabled:classifications/"
            + LOCAL_IP_SOURCE_PLZBLOCK + "/should_filter/true"})
    public void
    testOpenInIncognitoSourceFiltering() throws TimeoutException {
        triggerContextMenuLoad(sActivityTestRule.getTestServer().getURL(HTML_PATH),
                "testLinkFiltered", R.id.contextmenu_open_in_incognito_tab);

        Tab tab = sActivityTestRule.getActivity().getActivityTab();
        waitForLoad(sLastOpenedTab.getWebContents());
        Assert.assertFalse(
                "".equals(sLastOpenedTab.getWebContents().getLastCommittedUrl().getSpec()));
        Assert.assertFalse(sLastOpenedTab.getWebContents().getLastCommittedUrl().getSpec().contains(
                "plzblock"));
    }

    /**
     * Verifies that parameters are not filtered for "Open in new incognito tab" unless the
     * feature is explicitly enabled.
     */
    @Test
    @MediumTest
    @Feature({"Browser"})
    public void testOpenInIncognitoNonFilteringDueToFeature() throws TimeoutException {
        triggerContextMenuLoad(sActivityTestRule.getTestServer().getURL(HTML_PATH),
                "testLinkFiltered", R.id.contextmenu_open_in_incognito_tab);

        Tab tab = sActivityTestRule.getActivity().getActivityTab();
        waitForLoad(sLastOpenedTab.getWebContents());
        Assert.assertFalse(
                "".equals(sLastOpenedTab.getWebContents().getLastCommittedUrl().getSpec()));
        Assert.assertTrue(sLastOpenedTab.getWebContents().getLastCommittedUrl().getSpec().contains(
                "plzblock"));
    }

    /**
     * Verifies destination parameters aren't filtered for "Open in new incognito tab",
     * when the should_filter parameter is false.
     */
    @Test
    @MediumTest
    @Feature({"Browser"})
    @CommandLineFlags.Add({"enable-features=IncognitoParamFilterEnabled:classifications/"
            + LOCAL_IP_DESTINATION_PLZBLOCK + "/should_filter/false"})
    public void
    testOpenInIncognitoNonFilteringDestinationDueToParam() throws TimeoutException {
        triggerContextMenuLoad(sActivityTestRule.getTestServer().getURL(HTML_PATH),
                "testLinkFiltered", R.id.contextmenu_open_in_incognito_tab);

        Tab tab = sActivityTestRule.getActivity().getActivityTab();
        waitForLoad(sLastOpenedTab.getWebContents());
        Assert.assertFalse(
                "".equals(sLastOpenedTab.getWebContents().getLastCommittedUrl().getSpec()));
        Assert.assertTrue(sLastOpenedTab.getWebContents().getLastCommittedUrl().getSpec().contains(
                "plzblock"));
    }

    /**
     * Verifies source parameters aren't filtered for "Open in new incognito tab",
     * when the should_filter parameter is false.
     */
    @Test
    @MediumTest
    @Feature({"Browser"})
    @CommandLineFlags.Add({"enable-features=IncognitoParamFilterEnabled:classifications/"
            + LOCAL_IP_SOURCE_PLZBLOCK + "/should_filter/false"})
    public void
    testOpenInIncognitoNonFilteringSourceDueToParam() throws TimeoutException {
        triggerContextMenuLoad(sActivityTestRule.getTestServer().getURL(HTML_PATH),
                "testLinkFiltered", R.id.contextmenu_open_in_incognito_tab);

        Tab tab = sActivityTestRule.getActivity().getActivityTab();
        waitForLoad(sLastOpenedTab.getWebContents());
        Assert.assertFalse(
                "".equals(sLastOpenedTab.getWebContents().getLastCommittedUrl().getSpec()));
        Assert.assertTrue(sLastOpenedTab.getWebContents().getLastCommittedUrl().getSpec().contains(
                "plzblock"));
    }

    private void waitForLoad(final WebContents webContents) {
        final Coordinates coord = Coordinates.createFor(webContents);
        CriteriaHelper.pollUiThread(
                coord::frameInfoUpdated, "FrameInfo has not been updated in time.");
    }

    private void triggerContextMenuLoad(String url, String openerDomId, int menuItemId)
            throws TimeoutException {
        sActivityTestRule.loadUrl(url);
        sActivityTestRule.assertWaitForPageScaleFactorMatch(0.5f);
        Tab tab = sActivityTestRule.getActivity().getActivityTab();
        ContextMenuUtils.selectContextMenuItem(InstrumentationRegistry.getInstrumentation(),
                sActivityTestRule.getActivity(), tab, openerDomId, menuItemId);
    }
}

// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import android.content.Context;

import androidx.test.filters.MediumTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.After;
import org.junit.AfterClass;
import org.junit.Assert;
import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.app.tabmodel.AsyncTabParamsManagerSingleton;
import org.chromium.chrome.browser.app.tabmodel.TabWindowManagerSingleton;
import org.chromium.chrome.browser.firstrun.FirstRunStatus;
import org.chromium.chrome.browser.flags.ActivityType;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.profiles.ProfileProvider;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.NextTabPolicy.NextTabPolicySupplier;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.batch.BlankCTATabInitialStateRule;
import org.chromium.chrome.test.util.browser.contextmenu.ContextMenuUtils;
import org.chromium.content_public.browser.LoadUrlParams;

import java.util.concurrent.TimeoutException;
import java.util.regex.Pattern;

/** Verifies URL load parameters set when triggering navigations from the context menu. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
public class ContextMenuLoadUrlParamsTest {
    @ClassRule
    public static ChromeTabbedActivityTestRule sActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Rule
    public BlankCTATabInitialStateRule mBlankCTATabInitialStateRule =
            new BlankCTATabInitialStateRule(sActivityTestRule, true);

    private static final String HTML_PATH =
            "/chrome/test/data/android/contextmenu/context_menu_test.html";
    private static final Pattern SCHEME_SEPARATOR_RE = Pattern.compile("://");

    // Load parameters of the last call to openNewTab().
    private static LoadUrlParams sOpenNewTabLoadUrlParams;

    // Records parameters of calls to TabModelSelector methods and otherwise behaves like
    // TabModelSelectorImpl.
    private static class RecordingTabModelSelector extends TabModelSelectorImpl {
        @Override
        public Tab openNewTab(
                LoadUrlParams loadUrlParams,
                @TabLaunchType int type,
                Tab parent,
                boolean incognito) {
            sOpenNewTabLoadUrlParams = loadUrlParams;
            return super.openNewTab(loadUrlParams, type, parent, incognito);
        }

        public RecordingTabModelSelector(
                OneshotSupplier<ProfileProvider> profileProviderSupplier,
                TabCreatorManager tabCreatorManager) {
            super(
                    profileProviderSupplier,
                    tabCreatorManager,
                    () -> NextTabPolicy.HIERARCHICAL,
                    AsyncTabParamsManagerSingleton.getInstance(),
                    false,
                    ActivityType.TABBED,
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
                    public TabModelSelector buildSelector(
                            Context context,
                            OneshotSupplier<ProfileProvider> profileProviderSupplier,
                            TabCreatorManager tabCreatorManager,
                            NextTabPolicySupplier nextTabPolicySupplier) {
                        return new RecordingTabModelSelector(
                                profileProviderSupplier, tabCreatorManager);
                    }
                });
    }

    @AfterClass
    public static void afterClass() {
        TabWindowManagerSingleton.resetTabModelSelectorFactoryForTesting();
    }

    @Before
    public void setUp() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    FirstRunStatus.setFirstRunFlowComplete(true);
                });
    }

    @After
    public void tearDown() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    FirstRunStatus.setFirstRunFlowComplete(false);
                });
    }

    /**
     * Verifies that the referrer and additional navigation params are correctly set for "Open in
     * new tab".
     */
    @Test
    @MediumTest
    @Feature({"Browser"})
    public void testOpenInNewTabReferrer() throws TimeoutException {
        triggerContextMenuLoad(
                sActivityTestRule.getTestServer().getURL(HTML_PATH),
                "testLink",
                R.id.contextmenu_open_in_new_tab);

        Assert.assertNotNull(sOpenNewTabLoadUrlParams);
        Assert.assertEquals(
                sActivityTestRule.getTestServer().getURL(HTML_PATH),
                sOpenNewTabLoadUrlParams.getReferrer().getUrl());

        Assert.assertNotNull(sOpenNewTabLoadUrlParams.getAdditionalNavigationParams());
        Assert.assertNotEquals(
                sOpenNewTabLoadUrlParams.getAdditionalNavigationParams().getInitiatorProcessId(),
                -1);
    }

    /**
     * Verifies that the referrer and additional navigation params are not set for "Open in new
     * incognito tab".
     */
    @Test
    @MediumTest
    @Feature({"Browser"})
    public void testOpenInIncognitoTabNoReferrer() throws TimeoutException {
        triggerContextMenuLoad(
                sActivityTestRule.getTestServer().getURL(HTML_PATH),
                "testLink",
                R.id.contextmenu_open_in_incognito_tab);

        Assert.assertNotNull(sOpenNewTabLoadUrlParams);
        Assert.assertNull(sOpenNewTabLoadUrlParams.getReferrer());
        Assert.assertNull(sOpenNewTabLoadUrlParams.getAdditionalNavigationParams());
    }

    /** Verifies that the referrer is stripped from username and password fields. */
    @Test
    @MediumTest
    @Feature({"Browser"})
    public void testOpenInNewTabSanitizeReferrer() throws TimeoutException {
        String testUrl = sActivityTestRule.getTestServer().getURL(HTML_PATH);
        String[] schemeAndUrl = SCHEME_SEPARATOR_RE.split(testUrl, 2);
        Assert.assertEquals(2, schemeAndUrl.length);
        String testUrlUserPass = schemeAndUrl[0] + "://user:pass@" + schemeAndUrl[1];
        triggerContextMenuLoad(testUrlUserPass, "testLink", R.id.contextmenu_open_in_new_tab);
        Assert.assertNotNull(sOpenNewTabLoadUrlParams);
        Assert.assertEquals(testUrl, sOpenNewTabLoadUrlParams.getReferrer().getUrl());
    }

    private void triggerContextMenuLoad(String url, String openerDomId, int menuItemId)
            throws TimeoutException {
        sActivityTestRule.loadUrl(url);
        sActivityTestRule.assertWaitForPageScaleFactorMatch(0.5f);
        Tab tab = sActivityTestRule.getActivity().getActivityTab();
        ContextMenuUtils.selectContextMenuItem(
                InstrumentationRegistry.getInstrumentation(),
                sActivityTestRule.getActivity(),
                tab,
                openerDomId,
                menuItemId);
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
    }
}

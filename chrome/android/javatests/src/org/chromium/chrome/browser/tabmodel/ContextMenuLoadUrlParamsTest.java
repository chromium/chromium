// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import android.app.Activity;
import android.support.test.InstrumentationRegistry;

import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.app.tabmodel.AsyncTabParamsManagerSingleton;
import org.chromium.chrome.browser.app.tabmodel.ChromeTabModelFilterFactory;
import org.chromium.chrome.browser.firstrun.FirstRunStatus;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.NextTabPolicy.NextTabPolicySupplier;
import org.chromium.chrome.browser.tabmodel.TabWindowManager.TabModelSelectorFactory;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.browser.contextmenu.RevampedContextMenuUtils;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.net.test.EmbeddedTestServer;

import java.util.concurrent.TimeoutException;
import java.util.regex.Pattern;

/**
 * Verifies URL load parameters set when triggering navigations from the context menu.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class ContextMenuLoadUrlParamsTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    private static final String HTML_PATH =
            "/chrome/test/data/android/contextmenu/context_menu_test.html";
    private static final Pattern SCHEME_SEPARATOR_RE = Pattern.compile("://");

    // Load parameters of the last call to openNewTab().
    LoadUrlParams mOpenNewTabLoadUrlParams;

    private EmbeddedTestServer mTestServer;

    // Records parameters of calls to TabModelSelector methods and otherwise behaves like
    // TabModelSelectorImpl.
    class RecordingTabModelSelector extends TabModelSelectorImpl {
        @Override
        public Tab openNewTab(LoadUrlParams loadUrlParams, @TabLaunchType int type, Tab parent,
                boolean incognito) {
            mOpenNewTabLoadUrlParams = loadUrlParams;
            return super.openNewTab(loadUrlParams, type, parent, incognito);
        }

        public RecordingTabModelSelector(Activity activity, TabCreatorManager tabCreatorManager,
                TabModelFilterFactory tabModelFilterFactory, int selectorIndex) {
            super(activity, null, tabCreatorManager,
                    new TabbedModeTabPersistencePolicy(selectorIndex, false), tabModelFilterFactory,
                    ()
                            -> NextTabPolicy.HIERARCHICAL,
                    AsyncTabParamsManagerSingleton.getInstance(), false, false, false);
        }
    }

    @Before
    public void setUp() throws Exception {
        // Plant RecordingTabModelSelector as the TabModelSelector used in Main. The factory has to
        // be set before super.setUp(), as super.setUp() creates Main and consequently the
        // TabModelSelector.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            TabWindowManager.getInstance().setTabModelSelectorFactory(
                    new TabModelSelectorFactory() {
                        @Override
                        public TabModelSelector buildSelector(Activity activity,
                                TabCreatorManager tabCreatorManager,
                                NextTabPolicySupplier nextTabPolicySupplier, int selectorIndex) {
                            return new RecordingTabModelSelector(activity, tabCreatorManager,
                                    new ChromeTabModelFilterFactory(), selectorIndex);
                        }
                    });
        });
        mActivityTestRule.startMainActivityOnBlankPage();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { FirstRunStatus.setFirstRunFlowComplete(true); });

        mTestServer = EmbeddedTestServer.createAndStartServer(InstrumentationRegistry.getContext());
    }

    @After
    public void tearDown() {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { FirstRunStatus.setFirstRunFlowComplete(false); });
        mTestServer.stopAndDestroyServer();
    }

    /**
     * Verifies that the referrer is correctly set for "Open in new tab".
     */
    @Test
    @MediumTest
    @Feature({"Browser"})
    public void testOpenInNewTabReferrer() throws TimeoutException {
        triggerContextMenuLoad(mTestServer.getURL(HTML_PATH), "testLink",
                R.id.contextmenu_open_in_new_tab);

        Assert.assertNotNull(mOpenNewTabLoadUrlParams);
        Assert.assertEquals(
                mTestServer.getURL(HTML_PATH), mOpenNewTabLoadUrlParams.getReferrer().getUrl());
    }

    /**
     * Verifies that the referrer is not set for "Open in new incognito tab".
     */
    @Test
    @MediumTest
    @Feature({"Browser"})
    public void testOpenInIncognitoTabNoReferrer() throws TimeoutException {
        triggerContextMenuLoad(mTestServer.getURL(HTML_PATH), "testLink",
                R.id.contextmenu_open_in_incognito_tab);

        Assert.assertNotNull(mOpenNewTabLoadUrlParams);
        Assert.assertNull(mOpenNewTabLoadUrlParams.getReferrer());
    }

    /**
     * Verifies that the referrer is stripped from username and password fields.
     */
    @Test
    @MediumTest
    @Feature({"Browser"})
    public void testOpenInNewTabSanitizeReferrer() throws TimeoutException {
        String testUrl = mTestServer.getURL(HTML_PATH);
        String[] schemeAndUrl = SCHEME_SEPARATOR_RE.split(testUrl, 2);
        Assert.assertEquals(2, schemeAndUrl.length);
        String testUrlUserPass = schemeAndUrl[0] + "://user:pass@" + schemeAndUrl[1];
        triggerContextMenuLoad(testUrlUserPass, "testLink", R.id.contextmenu_open_in_new_tab);
        Assert.assertNotNull(mOpenNewTabLoadUrlParams);
        Assert.assertEquals(testUrl, mOpenNewTabLoadUrlParams.getReferrer().getUrl());
    }

    private void triggerContextMenuLoad(String url, String openerDomId, int menuItemId)
            throws TimeoutException {
        mActivityTestRule.loadUrl(url);
        mActivityTestRule.assertWaitForPageScaleFactorMatch(0.5f);
        Tab tab = mActivityTestRule.getActivity().getActivityTab();
        RevampedContextMenuUtils.selectContextMenuItem(InstrumentationRegistry.getInstrumentation(),
                mActivityTestRule.getActivity(), tab, openerDomId, menuItemId);
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
    }
}

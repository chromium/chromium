// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import static androidx.test.espresso.intent.Intents.intended;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.util.Pair;

import androidx.test.espresso.intent.Intents;
import androidx.test.espresso.intent.matcher.IntentMatchers;
import androidx.test.filters.MediumTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.After;
import org.junit.AfterClass;
import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.app.tabmodel.AsyncTabParamsManagerSingleton;
import org.chromium.chrome.browser.app.tabwindow.TabWindowManagerSingleton;
import org.chromium.chrome.browser.firstrun.FirstRunStatus;
import org.chromium.chrome.browser.flags.ActivityType;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.incognito.IncognitoUtils;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileProvider;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.NextTabPolicy.NextTabPolicySupplier;
import org.chromium.chrome.browser.tabwindow.TabModelSelectorFactory;
import org.chromium.chrome.browser.tabwindow.WindowId;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.transit.AutoResetCtaTransitTestRule;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.chrome.test.util.browser.contextmenu.ContextMenuUtils;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.modaldialog.ModalDialogManager;

import java.util.concurrent.TimeoutException;
import java.util.regex.Pattern;

/** Verifies URL load parameters set when triggering navigations from the context menu. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
public class ContextMenuLoadUrlParamsTest {
    @Rule
    public AutoResetCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.autoResetCtaActivityRule();

    private static final String HTML_PATH =
            "/chrome/test/data/android/contextmenu/context_menu_test.html";
    // LINT.IfChange(PageScaleFactor)
    // The initial-scale defined in the test html file meta. The setUp function
    // will check that the page scale factor has been updated to this value.
    // This ensures the long press/ right click is simulated at the correct
    // coordinates of the specified element. See crbug.com/432281754.
    private static final float PAGE_SCALE_FACTOR = 1.0f;
    // LINT.ThenChange(//chrome/test/data/android/contextmenu/context_menu_test.html:PageScaleFactor)
    private static final Pattern SCHEME_SEPARATOR_RE = Pattern.compile("://");

    // Load parameters of the last call to openNewTab().
    private static LoadUrlParams sOpenNewTabLoadUrlParams;
    private WebPageStation mInitialPage;

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
                Context context,
                ModalDialogManager modalDialogManager,
                OneshotSupplier<ProfileProvider> profileProviderSupplier,
                TabCreatorManager tabCreatorManager) {
            super(
                    context,
                    modalDialogManager,
                    profileProviderSupplier,
                    tabCreatorManager,
                    () -> NextTabPolicy.HIERARCHICAL,
                    /* multiInstanceManager= */ null,
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
                    public TabModelSelector buildTabbedSelector(
                            Context context,
                            ModalDialogManager modalDialogManager,
                            OneshotSupplier<ProfileProvider> profileProviderSupplier,
                            TabCreatorManager tabCreatorManager,
                            NextTabPolicySupplier nextTabPolicySupplier,
                            MultiInstanceManager multiInstanceManager) {
                        return new RecordingTabModelSelector(
                                context,
                                modalDialogManager,
                                profileProviderSupplier,
                                tabCreatorManager);
                    }

                    @Override
                    public Pair<TabModelSelector, Destroyable> buildHeadlessSelector(
                            @WindowId int windowId, Profile profile) {
                        return Pair.create(null, null);
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
        mInitialPage = mActivityTestRule.startOnBlankPage();
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
                mActivityTestRule.getTestServer().getURL(HTML_PATH),
                "testLink",
                R.id.contextmenu_open_in_new_tab);

        assertNotNull(sOpenNewTabLoadUrlParams);
        assertEquals(
                mActivityTestRule.getTestServer().getURL(HTML_PATH),
                sOpenNewTabLoadUrlParams.getReferrer().getUrl());

        assertNotNull(sOpenNewTabLoadUrlParams.getAdditionalNavigationParams());
        assertNotEquals(
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
        Intents.init();

        int menuItemId =
                IncognitoUtils.shouldOpenIncognitoAsWindow()
                        ? R.id.contextmenu_open_in_incognito_window
                        : R.id.contextmenu_open_in_incognito_tab;
        triggerContextMenuLoad(
                mActivityTestRule.getTestServer().getURL(HTML_PATH), "testLink", menuItemId);

        if (IncognitoUtils.shouldOpenIncognitoAsWindow()) {
            intended(IntentMatchers.hasAction(Intent.ACTION_VIEW));
        } else {
            assertNotNull(sOpenNewTabLoadUrlParams);
            assertNull(sOpenNewTabLoadUrlParams.getReferrer());
            assertNull(sOpenNewTabLoadUrlParams.getAdditionalNavigationParams());
        }
        Intents.release();
    }

    /** Verifies that the referrer is stripped from username and password fields. */
    @Test
    @MediumTest
    @Feature({"Browser"})
    public void testOpenInNewTabSanitizeReferrer() throws TimeoutException {
        String testUrl = mActivityTestRule.getTestServer().getURL(HTML_PATH);
        String[] schemeAndUrl = SCHEME_SEPARATOR_RE.split(testUrl, 2);
        assertEquals(2, schemeAndUrl.length);
        String testUrlUserPass = schemeAndUrl[0] + "://user:pass@" + schemeAndUrl[1];
        triggerContextMenuLoad(testUrlUserPass, "testLink", R.id.contextmenu_open_in_new_tab);
        assertNotNull(sOpenNewTabLoadUrlParams);
        assertEquals(testUrl, sOpenNewTabLoadUrlParams.getReferrer().getUrl());
    }

    private void triggerContextMenuLoad(String url, String openerDomId, int menuItemId)
            throws TimeoutException {
        mActivityTestRule.loadUrl(url);
        mActivityTestRule.assertWaitForPageScaleFactorMatch(PAGE_SCALE_FACTOR);
        Tab tab = mActivityTestRule.getActivityTab();

        Activity activityToWaitFor = mActivityTestRule.getActivity();
        if (menuItemId == R.id.contextmenu_open_in_incognito_window) {
            activityToWaitFor = null;
        }

        ContextMenuUtils.selectContextMenuItem(
                InstrumentationRegistry.getInstrumentation(),
                activityToWaitFor,
                tab,
                openerDomId,
                menuItemId);
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
    }
}

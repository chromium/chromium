// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.dynamicmodule;

import static org.hamcrest.Matchers.equalTo;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;

import static org.chromium.base.ThreadUtils.runOnUiThreadBlocking;

import android.content.Intent;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.SmallTest;
import android.support.test.uiautomator.UiDevice;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewGroup.MarginLayoutParams;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.RuleChain;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.library_loader.LibraryProcessType;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.AppHooksModule;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.customtabs.CustomTabActivity;
import org.chromium.chrome.browser.customtabs.CustomTabActivityTestRule;
import org.chromium.chrome.browser.customtabs.CustomTabsTestUtils;
import org.chromium.chrome.browser.customtabs.dynamicmodule.CustomTabsDynamicModuleTestUtils.AppHooksModuleForTest;
import org.chromium.chrome.browser.customtabs.dynamicmodule.CustomTabsDynamicModuleTestUtils.IntentBuilder;
import org.chromium.chrome.browser.dependency_injection.ModuleOverridesRule;
import org.chromium.chrome.browser.toolbar.top.CustomTabToolbar;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.content_public.browser.test.util.ClickUtils;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.common.ContentSwitches;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.net.test.ServerCertificate;
import org.chromium.ui.base.PageTransition;

import java.util.concurrent.TimeoutException;

/**
 * Instrumentation tests for UI elements of {@link CustomTabActivity}
 * controlled by a dynamic module.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
        // The module managed host is google.com. In test, it is served by the
        // EmbeddedTestServer, running on 127.0.0.1, without a valid certificate.
        ContentSwitches.HOST_RESOLVER_RULES + "=MAP * 127.0.0.1", "ignore-certificate-errors"})
public class CustomTabsDynamicModuleUITest {

    private TestRule mModuleOverridesRule = new ModuleOverridesRule()
            .setOverride(AppHooksModule.Factory.class, AppHooksModuleForTest::new);

    private CustomTabActivityTestRule mActivityRule = new CustomTabActivityTestRule();

    @Rule
    public TestRule mOverrideModulesThenLaunchRule =
            RuleChain.outerRule(mModuleOverridesRule).around(mActivityRule);

    private static final String TEST_PAGE = "/chrome/test/data/android/google.html";
    private static final String TEST_PAGE_2 = "/chrome/test/data/android/test.html";
    private static final String MODULE_MANAGED_PAGE = "/chrome/test/data/android/about.html";
    private static final String MODULE_MANAGED_PAGE_2 = "/chrome/test/data/android/simple.html";

    private EmbeddedTestServer mTestServer;

    private String mTestPage;
    private String mTestPage2;
    private String mModuleManagedPage;
    private String mModuleManagedPage2;

    @Before
    public void setUp() {
        LibraryLoader.getInstance().ensureInitialized(LibraryProcessType.PROCESS_BROWSER);

        // Module managed hosts only work with HTTPS.
        mTestServer = EmbeddedTestServer.createAndStartHTTPSServer(
                InstrumentationRegistry.getInstrumentation().getContext(),
                ServerCertificate.CERT_OK);

        mTestPage = mTestServer.getURL(TEST_PAGE);
        mTestPage2 = mTestServer.getURL(TEST_PAGE_2);
        mModuleManagedPage = mTestServer.getURLWithHostName("google.com", MODULE_MANAGED_PAGE);
        mModuleManagedPage2 = mTestServer.getURLWithHostName("google.com", MODULE_MANAGED_PAGE_2);

        // The embedded test server can't use the 443 port number.
        DynamicModuleCoordinator.setAllowNonStandardPortNumber(true);
    }

    @After
    public void tearDown() {
        DynamicModuleCoordinator.setAllowNonStandardPortNumber(false);
    }

    /**
     * When module is not provided, i.e. module component name is not specified in the intent,
     * {@link DynamicModuleCoordinator} is not instantiated, therefore it should not be possible
     * to update UI elements of CCT.
     */
    @Test
    @SmallTest
    @Features.EnableFeatures(ChromeFeatureList.CCT_MODULE)
    public void testModuleNotProvided() {
        Intent intent = new IntentBuilder(mModuleManagedPage)
                .setModulePackageName(null).setModuleClassName(null)
                .setModuleManagedUrlRegex(getModuleManagedRegex())
                .build();

        mActivityRule.startCustomTabActivityWithIntent(intent);
        waitForModuleLoading();

        assertFalse(getActivity().getIntentDataProvider().isDynamicModuleEnabled());
        assertNoTopBar();
        assertCCTHeaderIsVisible();
    }

    /**
     * When feature {@link ChromeFeatureList.CCT_MODULE} is disabled,
     * {@link DynamicModuleCoordinator} is not instantiated, therefore it should not be possible
     * to update UI elements of CCT.
     */
    @Test
    @SmallTest
    @Features.DisableFeatures(ChromeFeatureList.CCT_MODULE)
    public void testFeatureIsDisabled() {
        Intent intent = new IntentBuilder(mModuleManagedPage)
                .setModuleManagedUrlRegex(getModuleManagedRegex())
                .build();

        mActivityRule.startCustomTabActivityWithIntent(intent);
        waitForModuleLoading();

        assertFalse(getActivity().getIntentDataProvider().isDynamicModuleEnabled());
        assertNoTopBar();
        assertCCTHeaderIsVisible();
    }

    /**
     This test executes the following workflow assuming dynamic module has been loaded succesfully:
     - moduleManagedUrl1 -> nav1.1 -> nav1.2 -> modulemanagedUrl2 -> nav2.1 -> nav2.2
     - User hits the "close button", therefore goes back to modulemanagedUrl2
     - User hits the Android back button, going returning to nav1.2
     - User hits the "close button" again, going return to moduleManagedUrl1
     - User hits the Android back button thereby closes CCT.
     */
    @Test
    @SmallTest
    @DisabledTest
    @Features.EnableFeatures(ChromeFeatureList.CCT_MODULE)
    public void testCloseButtonBehaviourWithDynamicModule() throws TimeoutException {
        Intent intent = new IntentBuilder(mModuleManagedPage)
                .setModuleManagedUrlRegex(getModuleManagedRegex())
                .build();

        // Open CCT with moduleManagedUrl1 and navigate
        // moduleManagedUrl1 -> nav1.1 - nav1.2 -> modulemanagedUrl2 -> nav2.1 -> nav2.2
        mActivityRule.startCustomTabActivityWithIntent(intent);
        CustomTabActivity cctActivity = mActivityRule.getActivity();

        mActivityRule.loadUrlInTab(mTestPage, PageTransition.LINK,
                cctActivity.getActivityTab());
        mActivityRule.loadUrlInTab(mTestPage2, PageTransition.LINK,
                cctActivity.getActivityTab());
        mActivityRule.loadUrlInTab(mModuleManagedPage2, PageTransition.TYPED,
                cctActivity.getActivityTab());
        mActivityRule.loadUrlInTab(mTestPage, PageTransition.LINK,
                cctActivity.getActivityTab());
        mActivityRule.loadUrlInTab(mTestPage2, PageTransition.LINK,
                cctActivity.getActivityTab());

        // click the close button and wait while tab page loaded
        ClickUtils.clickButton(cctActivity.findViewById(R.id.close_button));
        ChromeTabUtils.waitForTabPageLoaded(cctActivity.getActivityTab(), null);

        // close button returns back to moduleManagedUrl2
        assertEquals(mModuleManagedPage2, cctActivity.getActivityTab().getUrl());

        // press the back button and wait while tab page loaded
        UiDevice mDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation());
        mDevice.pressBack();
        ChromeTabUtils.waitForTabPageLoaded(cctActivity.getActivityTab(), null);

        // the back button returns to nav1.2
        assertEquals(mTestPage2, cctActivity.getActivityTab().getUrl());

        // click the close button and wait while tab page loaded
        ClickUtils.clickButton(cctActivity.findViewById(R.id.close_button));
        ChromeTabUtils.waitForTabPageLoaded(cctActivity.getActivityTab(), null);

        // close button returns back to moduleManagedUrl1
        assertEquals(mModuleManagedPage, cctActivity.getActivityTab().getUrl());

        // press back button and while cct is hidden
        runAndWaitForActivityStopped(mDevice::pressBack);
    }

    /**
     This test executes the following workflow assuming dynamic module has not been loaded:
     - moduleManagedUrl1 -> nav1.1 - nav1.2 -> modulemanagedUrl2 -> nav2.1 -> nav2.2
     - User hits the close button, thereby closes CCT
     */
    @Test
    @SmallTest
    public void testCloseButtonBehaviourWithoutDynamicModule() throws TimeoutException {
        // Open CCT with moduleManagedUrl1 and navigate
        // moduleManagedUrl1 -> nav1.1 - nav1.2 -> modulemanagedUrl2 -> nav2.1 -> nav2.2

        Intent intent = CustomTabsTestUtils.createMinimalCustomTabIntent(
                InstrumentationRegistry.getTargetContext(), mModuleManagedPage);
        mActivityRule.startCustomTabActivityWithIntent(intent);
        CustomTabActivity cctActivity = mActivityRule.getActivity();

        mActivityRule.loadUrlInTab(mTestPage, PageTransition.LINK,
                cctActivity.getActivityTab());
        mActivityRule.loadUrlInTab(mTestPage2, PageTransition.LINK,
                cctActivity.getActivityTab());
        mActivityRule.loadUrlInTab(mModuleManagedPage2, PageTransition.LINK,
                cctActivity.getActivityTab());
        mActivityRule.loadUrlInTab(mTestPage, PageTransition.LINK,
                cctActivity.getActivityTab());
        mActivityRule.loadUrlInTab(mTestPage2, PageTransition.LINK,
                cctActivity.getActivityTab());

        // click close button and wait while cct is hidden
        runAndWaitForActivityStopped(() ->
                ClickUtils.clickButton(cctActivity.findViewById(R.id.close_button)));
    }

    /**
     This test executes the following workflow assuming dynamic module loading fails:
     - moduleManagedUrl1 -> nav1.1 - nav1.2
     - User hits the close button, thereby closes CCT
     */
    @Test
    @SmallTest
    public void testCloseButtonBehaviourDynamicModuleLoadFails() throws TimeoutException {
        // Open CCT with moduleManagedUrl1 and navigate
        // moduleManagedUrl1 -> nav1.1 - nav1.2
        Intent intent = new IntentBuilder(mModuleManagedPage)
                .setModuleFailToLoadComponentName()
                .setModuleManagedUrlRegex(getModuleManagedRegex()).build();

        mActivityRule.startCustomTabActivityWithIntent(intent);
        CustomTabActivity cctActivity = mActivityRule.getActivity();

        waitForModuleLoading();

        mActivityRule.loadUrlInTab(mTestPage, PageTransition.LINK,
                cctActivity.getActivityTab());
        mActivityRule.loadUrlInTab(mTestPage2, PageTransition.LINK,
                cctActivity.getActivityTab());

        // click close button and wait while cct is hidden
        runAndWaitForActivityStopped(() ->
                ClickUtils.clickButton(cctActivity.findViewById(R.id.close_button)));
    }

    @Test
    @SmallTest
    @Features.EnableFeatures(ChromeFeatureList.CCT_MODULE)
    public void testSetTopBarContentView() {
        Intent intent = new IntentBuilder(mModuleManagedPage)
                .build();

        mActivityRule.startCustomTabActivityWithIntent(intent);
        waitForModuleLoading();

        runOnUiThreadBlocking(() -> {
            CustomTabActivity cctActivity = getActivity();
            View anyView = new View(cctActivity);
            getModuleCoordinator().setTopBarContentView(anyView);
            ViewGroup topBar = cctActivity.findViewById(R.id.topbar);
            Assert.assertNotNull(topBar);
            Assert.assertThat(anyView.getParent(), equalTo(topBar));
            assertEquals(View.GONE, anyView.getVisibility());
        });
    }

    @Test
    @SmallTest
    @Features.EnableFeatures(ChromeFeatureList.CCT_MODULE)
    public void testSetTopBarContentView_secondCallIsNoOp() {
        Intent intent = new IntentBuilder(mModuleManagedPage)
                .setModuleManagedUrlRegex(getModuleManagedRegex())
                .build();

        mActivityRule.startCustomTabActivityWithIntent(intent);
        waitForModuleLoading();

        DynamicModuleCoordinator coordinator = getModuleCoordinator();
        runOnUiThreadBlocking(() -> {
            View anyView = new View(getActivity());
            coordinator.setTopBarContentView(anyView);
            // Second call will not crash.
            coordinator.setTopBarContentView(anyView);
        });
    }

    @Test
    @SmallTest
    @Features.EnableFeatures(ChromeFeatureList.CCT_MODULE)
    public void testSetTopBarContentView_moduleLoadingFailed_cctHeaderVisible() {
        Intent intent = new IntentBuilder(mTestPage).setModuleFailToLoadComponentName().build();

        mActivityRule.startCustomTabActivityWithIntent(intent);
        waitForModuleLoading();

        runOnUiThreadBlocking(() -> {
            View anyView = new View(getActivity());
            getModuleCoordinator().setTopBarContentView(anyView);
            ViewGroup topBar = getActivity().findViewById(R.id.topbar);
            Assert.assertNotNull(topBar);
            Assert.assertThat(anyView.getParent(), equalTo(topBar));
            assertEquals(View.GONE, anyView.getVisibility());
        });

        assertCCTHeaderIsVisible();
    }

    @Test
    @SmallTest
    @Features.EnableFeatures(ChromeFeatureList.CCT_MODULE)
    public void testSetTopBarContentView_withModuleAndManagedUrls_topBarVisible() {
        Intent intent = new IntentBuilder(mModuleManagedPage)
                .setModuleManagedUrlRegex(getModuleManagedRegex())
                .build();
        mActivityRule.startCustomTabActivityWithIntent(intent);
        waitForModuleLoading();

        runOnUiThreadBlocking(() -> {
            CustomTabActivity cctActivity = getActivity();
            View anyView = new View(cctActivity);
            getModuleCoordinator().setTopBarContentView(anyView);
            ViewGroup topBar = cctActivity.findViewById(R.id.topbar);
            Assert.assertNotNull(topBar);
            Assert.assertThat(anyView.getParent(), equalTo(topBar));
            assertEquals(View.VISIBLE, anyView.getVisibility());
        });
    }

    @Test
    @SmallTest
    @Features.
    EnableFeatures({ChromeFeatureList.CCT_MODULE, ChromeFeatureList.CCT_MODULE_CUSTOM_HEADER,
            ChromeFeatureList.CCT_MODULE_USE_INTENT_EXTRAS})
    public void
    testSetTopBarContentView_notModuleManagedHost_cctHeaderVisible() {
        String url = mTestServer.getURLWithHostName("non-managed-domain", MODULE_MANAGED_PAGE);
        Intent intent = new IntentBuilder(url)
                                .setModuleManagedUrlRegex(getModuleManagedRegex())
                                .setHideCCTHeader(true)
                                .build();

        mActivityRule.startCustomTabActivityWithIntent(intent);
        waitForModuleLoading();

        runOnUiThreadBlocking(
                () -> getModuleCoordinator().setTopBarContentView(new View(getActivity())));
        assertCCTHeaderIsVisible();
    }

    @Test
    @SmallTest
    @Features.
    EnableFeatures({ChromeFeatureList.CCT_MODULE, ChromeFeatureList.CCT_MODULE_CUSTOM_HEADER,
            ChromeFeatureList.CCT_MODULE_USE_INTENT_EXTRAS})
    public void
    testSetTopBarContentView_withModuleAndExtras_cctHeaderHidden() {
        Intent intent = new IntentBuilder(mModuleManagedPage)
                .setModuleManagedUrlRegex(getModuleManagedRegex())
                .setHideCCTHeader(true)
                .build();

        mActivityRule.startCustomTabActivityWithIntent(intent);
        waitForModuleLoading();

        CustomTabActivity cctActivity = getActivity();
        runOnUiThreadBlocking(() -> {
            getModuleCoordinator().setTopBarContentView(new View(cctActivity));
            ViewGroup toolbarContainerView = cctActivity.findViewById(R.id.toolbar_container);
            for (int index = 0; index < toolbarContainerView.getChildCount(); index++) {
                View childView = toolbarContainerView.getChildAt(index);
                if (childView.getId() != R.id.topbar) {
                    assertEquals(View.GONE, childView.getVisibility());
                }
            }
        });
    }

    @Test
    @SmallTest
    @Features
            .EnableFeatures(ChromeFeatureList.CCT_MODULE_USE_INTENT_EXTRAS)
            @Features.DisableFeatures(ChromeFeatureList.CCT_MODULE_CUSTOM_HEADER)
            public void testSetTopBarHeight_featureDisabled_heightNotChanged() {
        Intent intent = new IntentBuilder(mModuleManagedPage)
                .setModuleManagedUrlRegex(getModuleManagedRegex())
                .setHideCCTHeader(true)
                .build();
        mActivityRule.startCustomTabActivityWithIntent(intent);

        runOnUiThreadBlocking(() -> {
            CustomTabActivity cctActivity = getActivity();
            int defaultHeight = cctActivity.getFullscreenManager().getTopControlsHeight();
            int newHeight = defaultHeight + 10;
            getModuleCoordinator().setTopBarHeight(newHeight);
            assertEquals(
                    defaultHeight, cctActivity.getFullscreenManager().getTopControlsHeight());
        });
    }

    @Test
    @SmallTest
    @Features.
    EnableFeatures({ChromeFeatureList.CCT_MODULE, ChromeFeatureList.CCT_MODULE_CUSTOM_HEADER,
            ChromeFeatureList.CCT_MODULE_USE_INTENT_EXTRAS})
    public void
    testSetTopBarHeight_cctHeaderNotHidden_heightNotChanged() {
        Intent intent = new IntentBuilder(mModuleManagedPage)
                .setModuleManagedUrlRegex(getModuleManagedRegex())
                .setHideCCTHeader(false)
                .build();
        mActivityRule.startCustomTabActivityWithIntent(intent);

        runOnUiThreadBlocking(() -> {
            CustomTabActivity cctActivity = getActivity();
            int defaultHeight = cctActivity.getFullscreenManager().getTopControlsHeight();
            int newHeight = defaultHeight + 10;
            getModuleCoordinator().setTopBarHeight(newHeight);
            assertEquals(defaultHeight, cctActivity.getFullscreenManager().getTopControlsHeight());
        });
    }

    @Test
    @SmallTest
    @Features.
    EnableFeatures({ChromeFeatureList.CCT_MODULE, ChromeFeatureList.CCT_MODULE_CUSTOM_HEADER,
            ChromeFeatureList.CCT_MODULE_USE_INTENT_EXTRAS})
    public void
    testSetTopBarHeight_withModuleAndExtras_heightUpdated() {
        Intent intent = new IntentBuilder(mModuleManagedPage)
                .setModuleManagedUrlRegex(getModuleManagedRegex())
                .setHideCCTHeader(true)
                .build();
        mActivityRule.startCustomTabActivityWithIntent(intent);

        runOnUiThreadBlocking(() -> {
            CustomTabActivity cctActivity = getActivity();
            int defaultHeight = cctActivity.getFullscreenManager().getTopControlsHeight();
            int newHeight = defaultHeight + 10;
            getModuleCoordinator().setTopBarHeight(newHeight);
            assertEquals(newHeight, cctActivity.getFullscreenManager().getTopControlsHeight());
        });
    }

    @Test
    @SmallTest
    @Features.
    EnableFeatures({ChromeFeatureList.CCT_MODULE, ChromeFeatureList.CCT_MODULE_CUSTOM_HEADER,
            ChromeFeatureList.CCT_MODULE_USE_INTENT_EXTRAS})
    public void
    testSetTopBarHeight_zeroHeightHidesTopBar() {
        Intent intent = new IntentBuilder(mModuleManagedPage)
                .setModuleManagedUrlRegex(getModuleManagedRegex())
                .build();
        mActivityRule.startCustomTabActivityWithIntent(intent);
        waitForModuleLoading();

        runOnUiThreadBlocking(() -> {
            CustomTabActivity cctActivity = getActivity();
            View anyView = new View(cctActivity);
            getModuleCoordinator().setTopBarContentView(anyView);
            getModuleCoordinator().setTopBarHeight(0);
            assertEquals(View.GONE, anyView.getVisibility());
        });
    }

    @Test
    @SmallTest
    @Features
            .EnableFeatures(ChromeFeatureList.CCT_MODULE_USE_INTENT_EXTRAS)
            @Features.DisableFeatures(ChromeFeatureList.CCT_MODULE_CUSTOM_HEADER)
            public void testSetTopBarContentView_featureDisabled_progressBarNoChange() {
        Intent intent = new IntentBuilder(mModuleManagedPage)
                                .setModuleManagedUrlRegex(getModuleManagedRegex())
                                .setHideCCTHeader(true)
                                .build();
        mActivityRule.startCustomTabActivityWithIntent(intent);

        runOnUiThreadBlocking(() -> Assert.assertFalse(canChangeProgressBarTopMargin()));
    }

    @Test
    @SmallTest
    @Features.
    EnableFeatures({ChromeFeatureList.CCT_MODULE, ChromeFeatureList.CCT_MODULE_CUSTOM_HEADER,
            ChromeFeatureList.CCT_MODULE_USE_INTENT_EXTRAS})
    public void
    testSetTopBarContentView_cctHeaderNotHidden_progressBarNoChange() {
        Intent intent = new IntentBuilder(mModuleManagedPage)
                                .setModuleManagedUrlRegex(getModuleManagedRegex())
                                .setHideCCTHeader(false)
                                .build();
        mActivityRule.startCustomTabActivityWithIntent(intent);

        runOnUiThreadBlocking(() -> Assert.assertFalse(canChangeProgressBarTopMargin()));
    }

    @Test
    @SmallTest
    @Features.
    EnableFeatures({ChromeFeatureList.CCT_MODULE, ChromeFeatureList.CCT_MODULE_CUSTOM_HEADER,
            ChromeFeatureList.CCT_MODULE_USE_INTENT_EXTRAS})
    public void
    testSetTopBarContentView_withModuleAndExtras_progressBarChanged() {
        Intent intent = new IntentBuilder(mModuleManagedPage)
                                .setModuleManagedUrlRegex(getModuleManagedRegex())
                                .setHideCCTHeader(true)
                                .build();
        mActivityRule.startCustomTabActivityWithIntent(intent);

        runOnUiThreadBlocking(() -> Assert.assertTrue(canChangeProgressBarTopMargin()));
    }

    @Test
    @SmallTest
    public void testToolbarController_doesNotHideCctTopBar_doesNotAcquiredToken() throws Exception {
        Intent intent = CustomTabsTestUtils.createMinimalCustomTabIntent(
                InstrumentationRegistry.getTargetContext(), mTestPage);
        CustomTabsTestUtils.setHideCctTopBarOnModuleManagedUrls(intent, false);
        mActivityRule.startCustomTabActivityWithIntent(intent);

        runOnUiThreadBlocking(() -> {
            DynamicModuleToolbarController toolbarController =
                    getActivity().getComponent().resolveDynamicModuleToolbarController();
            Assert.assertFalse(toolbarController.hasAcquiredToken());
        });
    }

    @Test
    @SmallTest
    @Features.DisableFeatures(ChromeFeatureList.CCT_MODULE)
    public void testToolbarController_moduleDisabled_acquiredThenReleasedToken() throws Exception {
        Intent intent = CustomTabsTestUtils.createMinimalCustomTabIntent(
                InstrumentationRegistry.getTargetContext(), mTestPage);
        CustomTabsTestUtils.setHideCctTopBarOnModuleManagedUrls(intent, true);
        mActivityRule.startCustomTabActivityWithIntent(intent);

        runOnUiThreadBlocking(() -> {
            DynamicModuleToolbarController toolbarController =
                    getActivity().getComponent().resolveDynamicModuleToolbarController();
            Assert.assertTrue(toolbarController.hasAcquiredToken());
            Assert.assertTrue(toolbarController.hasReleasedToken());
        });
    }

    @Test
    @SmallTest
    @Features.EnableFeatures(ChromeFeatureList.CCT_MODULE)
    public void testToolbarController_hideCctTopBar_acquiredThenReleasedToken() throws Exception {
        Intent intent = new IntentBuilder(mModuleManagedPage).build();
        CustomTabsTestUtils.setHideCctTopBarOnModuleManagedUrls(intent, true);
        mActivityRule.startCustomTabActivityWithIntent(intent);

        runOnUiThreadBlocking(() -> {
            DynamicModuleToolbarController toolbarController =
                    getActivity().getComponent().resolveDynamicModuleToolbarController();
            Assert.assertTrue(toolbarController.hasAcquiredToken());
            Assert.assertTrue(toolbarController.hasReleasedToken());
        });
    }

    private void assertNoTopBar() {
        runOnUiThreadBlocking(() -> {
            ViewGroup topBar = getActivity().findViewById(R.id.topbar);
            Assert.assertNull(topBar);
        });
    }

    private void assertCCTHeaderIsVisible() {
        runOnUiThreadBlocking(() -> {
            View toolbarView = getActivity().findViewById(R.id.toolbar);
            Assert.assertTrue("A custom tab toolbar is never shown",
                    toolbarView instanceof CustomTabToolbar);
            CustomTabToolbar toolbar = (CustomTabToolbar) toolbarView;
            assertEquals(View.VISIBLE, toolbar.getVisibility());
        });
    }

    private String getModuleManagedRegex() {
        return "^(" + MODULE_MANAGED_PAGE + "|" + MODULE_MANAGED_PAGE_2 + ")$";
    }

    private void runAndWaitForActivityStopped(Runnable runnable) throws TimeoutException {
        CallbackHelper cctHiddenCallback = new CallbackHelper();
        ApplicationStatus.ActivityStateListener listener = (activity, newState) -> {
            if (activity == getActivity() &&
                    (newState == ActivityState.STOPPED || newState == ActivityState.DESTROYED)) {
                cctHiddenCallback.notifyCalled();
            }
        };
        ApplicationStatus.registerStateListenerForAllActivities(listener);

        runnable.run();
        cctHiddenCallback.waitForCallback("Hide cct", 0);
        ApplicationStatus.unregisterActivityStateListener(listener);
    }

    private void waitForModuleLoading() {
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                if (!getActivity().getIntentDataProvider().isDynamicModuleEnabled()) return true;

                DynamicModuleCoordinator module = getModuleCoordinator();
                return module != null && !module.isModuleLoading();
            }
        });
    }

    private CustomTabActivity getActivity() {
        return mActivityRule.getActivity();
    }

    private DynamicModuleCoordinator getModuleCoordinator() {
        return getActivity().getComponent().resolveDynamicModuleCoordinator();
    }

    private boolean canChangeProgressBarTopMargin() {
        CustomTabActivity cctActivity = getActivity();
        ViewGroup controlContainerView = cctActivity.findViewById(R.id.control_container);
        int progressBarHeight = 0;
        for (int index = 0; index < controlContainerView.getChildCount(); index++) {
            View childView = controlContainerView.getChildAt(index);
            if (childView.getId() != R.id.toolbar_container) {
                // Either ToolbarProgressBar or ToolbarProgressBarAnimatingView
                progressBarHeight = childView.getHeight();
                break;
            }
        }
        Assert.assertNotEquals(progressBarHeight, 0);

        View anyView = new View(cctActivity);
        getModuleCoordinator().setTopBarContentView(anyView);
        int newMargin = anyView.getBottom() - progressBarHeight;

        boolean canChange = false;
        for (int index = 0; index < controlContainerView.getChildCount(); index++) {
            View childView = controlContainerView.getChildAt(index);
            if (childView.getId() != R.id.toolbar_container) {
                if (((MarginLayoutParams) childView.getLayoutParams()).topMargin != newMargin) {
                    return false;
                } else {
                    canChange = true;
                }
            }
        }
        return canChange;
    }
}

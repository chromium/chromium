// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import static androidx.test.espresso.matcher.ViewMatchers.assertThat;

import static org.hamcrest.Matchers.equalTo;
import static org.hamcrest.Matchers.is;
import static org.junit.Assert.assertEquals;

import android.app.Activity;
import android.app.Instrumentation;
import android.app.PendingIntent;
import android.content.ComponentName;
import android.content.Context;
import android.content.ContextWrapper;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;

import androidx.browser.customtabs.CustomTabsCallback;
import androidx.browser.customtabs.CustomTabsIntent;
import androidx.browser.customtabs.CustomTabsSession;
import androidx.core.os.BuildCompat;
import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.SmallTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.hamcrest.Matchers;
import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.RuleChain;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.ContextUtils;
import org.chromium.base.IntentUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.JniMocker;
import org.chromium.base.test.util.PackageManagerWrapper;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.app.metrics.LaunchCauseMetrics;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider.CustomTabsUiType;
import org.chromium.chrome.browser.crypto.CipherFactory;
import org.chromium.chrome.browser.customtabs.content.CustomTabIntentHandler;
import org.chromium.chrome.browser.customtabs.dependency_injection.BaseCustomTabActivityModule;
import org.chromium.chrome.browser.dependency_injection.ModuleOverridesRule;
import org.chromium.chrome.browser.document.ChromeLauncherActivity;
import org.chromium.chrome.browser.firstrun.FirstRunStatus;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.incognito.reauth.IncognitoReauthSettingUtils;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.browser.theme.TopUiThemeColorProvider;
import org.chromium.chrome.browser.translate.TranslateBridge;
import org.chromium.chrome.browser.translate.TranslateBridgeJni;
import org.chromium.chrome.browser.ui.appmenu.AppMenuCoordinator;
import org.chromium.chrome.browser.ui.appmenu.AppMenuHandler;
import org.chromium.chrome.browser.ui.appmenu.AppMenuItemProperties;
import org.chromium.chrome.browser.ui.appmenu.AppMenuPropertiesDelegate;
import org.chromium.chrome.browser.ui.appmenu.AppMenuTestSupport;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.components.webapps.WebappsUtils;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.test.util.DeviceRestriction;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.TimeoutException;

/** Integration tests for the Custom Tab App Menu. */
@RunWith(ChromeJUnit4ClassRunner.class)
@EnableFeatures({ChromeFeatureList.APP_SPECIFIC_HISTORY})
public class CustomTabActivityAppMenuTest {
    private static final int MAX_MENU_CUSTOM_ITEMS = 7;
    private static final int NUM_CHROME_MENU_ITEMS = 6;
    private static final int NUM_CHROME_MENU_ITEMS_WITH_DIVIDER = 7;
    private static final String TEST_PAGE = "/chrome/test/data/android/google.html";
    private static final String TEST_MENU_TITLE = "testMenuTitle";

    @Rule public JniMocker jniMocker = new JniMocker();
    @Mock private TranslateBridge.Natives mTranslateBridgeJniMock;

    public CustomTabActivityTestRule mCustomTabActivityTestRule = new CustomTabActivityTestRule();

    private final TestRule mModuleOverridesRule =
            new ModuleOverridesRule()
                    .setOverride(
                            BaseCustomTabActivityModule.Factory.class,
                            (BrowserServicesIntentDataProvider intentDataProvider,
                                    CustomTabNightModeStateController nightModeController,
                                    CustomTabIntentHandler.IntentIgnoringCriterion
                                            intentIgnoringCriterion,
                                    TopUiThemeColorProvider topUiThemeColorProvider,
                                    DefaultBrowserProviderImpl customTabDefaultBrowserProvider,
                                    CipherFactory cipherFactory) ->
                                    new BaseCustomTabActivityModule(
                                            intentDataProvider,
                                            nightModeController,
                                            intentIgnoringCriterion,
                                            topUiThemeColorProvider,
                                            new FakeDefaultBrowserProviderImpl(),
                                            cipherFactory));

    @Rule
    public RuleChain mRuleChain =
            RuleChain.emptyRuleChain()
                    .around(mCustomTabActivityTestRule)
                    .around(mModuleOverridesRule);

    private String mTestPage;

    private static class TestContext extends ContextWrapper {
        public TestContext(Context baseContext) {
            super(baseContext);
        }

        @Override
        public PackageManager getPackageManager() {
            return new PackageManagerWrapper(super.getPackageManager()) {
                @Override
                public List<ResolveInfo> queryBroadcastReceivers(Intent intent, int filters) {
                    return new ArrayList<ResolveInfo>();
                }
            };
        }

        @Override
        public Object getSystemService(String name) {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N_MR1) {
                if (name.equals(Context.SHORTCUT_SERVICE)) {
                    return null;
                }
            }
            return super.getSystemService(name);
        }
    }

    @Before
    public void setUp() throws Exception {
        MockitoAnnotations.initMocks(this);

        // Mock translate bridge so "Translate..." menu item doesn't unexpectedly show up.
        jniMocker.mock(
                org.chromium.chrome.browser.translate.TranslateBridgeJni.TEST_HOOKS,
                mTranslateBridgeJniMock);
        jniMocker.mock(TranslateBridgeJni.TEST_HOOKS, mTranslateBridgeJniMock);

        ThreadUtils.runOnUiThreadBlocking(() -> FirstRunStatus.setFirstRunFlowComplete(true));
        mTestPage = mCustomTabActivityTestRule.getTestServer().getURL(TEST_PAGE);
        WebappsUtils.setAddToHomeIntentSupportedForTesting(true);
        LibraryLoader.getInstance().ensureInitialized();
    }

    @After
    public void tearDown() {
        ThreadUtils.runOnUiThreadBlocking(() -> FirstRunStatus.setFirstRunFlowComplete(false));

        // finish() is called on a non-UI thread by the testing harness. Must hide the menu
        // first, otherwise the UI is manipulated on a non-UI thread.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    if (mCustomTabActivityTestRule.getActivity() == null) return;
                    AppMenuCoordinator coordinator =
                            mCustomTabActivityTestRule.getAppMenuCoordinator();
                    // CCT doesn't always have a menu (ex. in the media viewer).
                    if (coordinator == null) return;
                    AppMenuHandler handler = coordinator.getAppMenuHandler();
                    if (handler != null) handler.hideAppMenu();
                });

        WebappsUtils.setAddToHomeIntentSupportedForTesting(null);
    }

    private Intent createMinimalCustomTabIntent() {
        return CustomTabsIntentTestUtils.createMinimalCustomTabIntent(
                ApplicationProvider.getApplicationContext(), mTestPage);
    }

    private CustomTabIntentDataProvider getCustomTabIntentDataProvider() {
        return (CustomTabIntentDataProvider)
                mCustomTabActivityTestRule.getActivity().getIntentDataProvider();
    }

    private void openAppMenuAndAssertMenuShown() {
        CustomTabsTestUtils.openAppMenuAndAssertMenuShown(mCustomTabActivityTestRule.getActivity());
    }

    private static int adjustMenuSize(int expectedMenuSize) {
        // history menu won't be shown on pre-U devices. Decrease the expected size by 1.
        return BuildCompat.isAtLeastU() ? expectedMenuSize : expectedMenuSize - 1;
    }

    private void assertHistoryMenuVisibility() {
        var historyMenu =
                AppMenuTestSupport.getMenuItemPropertyModel(
                        mCustomTabActivityTestRule.getAppMenuCoordinator(),
                        R.id.open_history_menu_id);
        if (BuildCompat.isAtLeastU()) {
            Assert.assertNotNull(historyMenu);
        } else {
            Assert.assertNull(historyMenu);
        }
    }

    private void assertHistoryMenuIsNotShown() {
        openAppMenuAndAssertMenuShown();

        Assert.assertNull(
                AppMenuTestSupport.getMenuItemPropertyModel(
                        mCustomTabActivityTestRule.getAppMenuCoordinator(),
                        R.id.open_history_menu_id));

        ModelList menuItemsModelList =
                AppMenuTestSupport.getMenuModelList(
                        mCustomTabActivityTestRule.getAppMenuCoordinator());
        final int expectedMenuSize = NUM_CHROME_MENU_ITEMS - 1;
        CustomTabsTestUtils.assertMenuSize(menuItemsModelList, expectedMenuSize);
    }

    /** Test the entries in the app menu. */
    @Test
    @SmallTest
    public void testAppMenu() throws Exception {
        Intent intent = createMinimalCustomTabIntent();
        int numMenuEntries = 1;
        CustomTabsIntentTestUtils.addMenuEntriesToIntent(intent, numMenuEntries, TEST_MENU_TITLE);
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);
        CustomTabAppMenuPropertiesDelegate propertiesDelegate =
                (CustomTabAppMenuPropertiesDelegate)
                        mCustomTabActivityTestRule
                                .getAppMenuCoordinator()
                                .getAppMenuPropertiesDelegate();
        propertiesDelegate.setHasClientPackageForTesting(true);

        openAppMenuAndAssertMenuShown();
        ModelList menuItemsModelList =
                AppMenuTestSupport.getMenuModelList(
                        mCustomTabActivityTestRule.getAppMenuCoordinator());
        final int expectedMenuSize = numMenuEntries + NUM_CHROME_MENU_ITEMS_WITH_DIVIDER;

        Assert.assertNotNull("App menu is not initialized: ", menuItemsModelList);
        Assert.assertNotNull(
                AppMenuTestSupport.getMenuItemPropertyModel(
                        mCustomTabActivityTestRule.getAppMenuCoordinator(), R.id.forward_menu_id));
        Assert.assertNotNull(
                AppMenuTestSupport.getMenuItemPropertyModel(
                        mCustomTabActivityTestRule.getAppMenuCoordinator(),
                        R.id.bookmark_this_page_id));
        Assert.assertNotNull(
                AppMenuTestSupport.getMenuItemPropertyModel(
                        mCustomTabActivityTestRule.getAppMenuCoordinator(), R.id.offline_page_id));
        Assert.assertNotNull(
                AppMenuTestSupport.getMenuItemPropertyModel(
                        mCustomTabActivityTestRule.getAppMenuCoordinator(), R.id.info_menu_id));
        Assert.assertNotNull(
                AppMenuTestSupport.getMenuItemPropertyModel(
                        mCustomTabActivityTestRule.getAppMenuCoordinator(), R.id.reload_menu_id));
        Assert.assertNotNull(
                AppMenuTestSupport.getMenuItemPropertyModel(
                        mCustomTabActivityTestRule.getAppMenuCoordinator(),
                        R.id.open_in_browser_id));
        Assert.assertNotNull(
                AppMenuTestSupport.getMenuItemPropertyModel(
                        mCustomTabActivityTestRule.getAppMenuCoordinator(), R.id.find_in_page_id));
        Assert.assertNotNull(
                AppMenuTestSupport.getMenuItemPropertyModel(
                        mCustomTabActivityTestRule.getAppMenuCoordinator(),
                        R.id.universal_install));
        Assert.assertNotNull(
                AppMenuTestSupport.getMenuItemPropertyModel(
                        mCustomTabActivityTestRule.getAppMenuCoordinator(),
                        R.id.request_desktop_site_row_menu_id));
        Assert.assertNull(
                AppMenuTestSupport.getMenuItemPropertyModel(
                        mCustomTabActivityTestRule.getAppMenuCoordinator(),
                        R.id.share_row_menu_id));

        assertHistoryMenuVisibility();

        // Assert the divider line is displayed in the correct position.
        int dividerLine =
                AppMenuTestSupport.findIndexOfMenuItemById(
                        mCustomTabActivityTestRule.getAppMenuCoordinator(), R.id.divider_line_id);
        int expectedPos = numMenuEntries + 1; // Add 1 to account for app menu icon row.
        Assert.assertEquals("Divider line at incorrect index.", expectedPos, dividerLine);

        CustomTabsTestUtils.assertMenuSize(menuItemsModelList, adjustMenuSize(expectedMenuSize));
    }

    @Test
    @SmallTest
    public void testAppMenuNoCustomEntries() throws Exception {
        Intent intent = createMinimalCustomTabIntent();
        int numMenuEntries = 0;
        CustomTabsIntentTestUtils.addMenuEntriesToIntent(intent, numMenuEntries, TEST_MENU_TITLE);
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);
        CustomTabAppMenuPropertiesDelegate propertiesDelegate =
                (CustomTabAppMenuPropertiesDelegate)
                        mCustomTabActivityTestRule
                                .getAppMenuCoordinator()
                                .getAppMenuPropertiesDelegate();
        propertiesDelegate.setHasClientPackageForTesting(true);
        openAppMenuAndAssertMenuShown();
        ModelList menuItemsModelList =
                AppMenuTestSupport.getMenuModelList(
                        mCustomTabActivityTestRule.getAppMenuCoordinator());
        final int expectedMenuSize = numMenuEntries + NUM_CHROME_MENU_ITEMS;

        Assert.assertNotNull("App menu is not initialized: ", menuItemsModelList);

        // Assert the divider line is not displayed.
        int dividerLine =
                AppMenuTestSupport.findIndexOfMenuItemById(
                        mCustomTabActivityTestRule.getAppMenuCoordinator(), R.id.divider_line_id);
        int expectedPos = -1; // No custom menu entries, not expecting a divider line.
        Assert.assertEquals("Divider present when it shouldn't be.", expectedPos, dividerLine);

        CustomTabsTestUtils.assertMenuSize(menuItemsModelList, adjustMenuSize(expectedMenuSize));
    }

    /** Test the App Menu does not show for media viewer. */
    @Test
    @SmallTest
    public void testAppMenuForMediaViewer() {
        Intent intent = createMinimalCustomTabIntent();
        intent.putExtra(CustomTabIntentDataProvider.EXTRA_UI_TYPE, CustomTabsUiType.MEDIA_VIEWER);
        IntentUtils.addTrustedIntentExtras(intent);
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);

        PostTask.runOrPostTask(
                TaskTraits.UI_DEFAULT,
                () -> {
                    mCustomTabActivityTestRule
                            .getActivity()
                            .onMenuOrKeyboardAction(R.id.show_menu, false);
                    Assert.assertNull(mCustomTabActivityTestRule.getAppMenuCoordinator());
                });
    }

    /** Test the entries in app menu for Reader Mode. */
    @Test
    @SmallTest
    public void testAppMenuForReaderMode() throws Exception {
        Intent intent = createMinimalCustomTabIntent();
        intent.putExtra(CustomTabIntentDataProvider.EXTRA_UI_TYPE, CustomTabsUiType.READER_MODE);
        IntentUtils.addTrustedIntentExtras(intent);
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);

        openAppMenuAndAssertMenuShown();
        ModelList menuItemsModelList =
                AppMenuTestSupport.getMenuModelList(
                        mCustomTabActivityTestRule.getAppMenuCoordinator());
        final int expectedMenuSize = 2;

        Assert.assertNotNull("App menu is not initialized: ", menuItemsModelList);

        Assert.assertNotNull(
                AppMenuTestSupport.getMenuItemPropertyModel(
                        mCustomTabActivityTestRule.getAppMenuCoordinator(), R.id.find_in_page_id));
        Assert.assertNotNull(
                AppMenuTestSupport.getMenuItemPropertyModel(
                        mCustomTabActivityTestRule.getAppMenuCoordinator(),
                        R.id.reader_mode_prefs_id));

        CustomTabsTestUtils.assertMenuSize(menuItemsModelList, expectedMenuSize);
    }

    /** Test the entries in app menu for media viewer. */
    @Test
    @SmallTest
    public void testAppMenuForOfflinePage() throws Exception {
        Intent intent = createMinimalCustomTabIntent();
        intent.putExtra(
                CustomTabIntentDataProvider.EXTRA_UI_TYPE,
                CustomTabIntentDataProvider.CustomTabsUiType.OFFLINE_PAGE);
        IntentUtils.addTrustedIntentExtras(intent);
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);

        openAppMenuAndAssertMenuShown();
        ModelList menuItemsModelList =
                AppMenuTestSupport.getMenuModelList(
                        mCustomTabActivityTestRule.getAppMenuCoordinator());
        final int expectedMenuSize = 3;

        Assert.assertNotNull("App menu is not initialized: ", menuItemsModelList);

        Assert.assertNotNull(
                AppMenuTestSupport.getMenuItemPropertyModel(
                        mCustomTabActivityTestRule.getAppMenuCoordinator(), R.id.find_in_page_id));
        Assert.assertNotNull(
                AppMenuTestSupport.getMenuItemPropertyModel(
                        mCustomTabActivityTestRule.getAppMenuCoordinator(),
                        R.id.request_desktop_site_row_menu_id));

        ModelList iconRowModelList =
                AppMenuTestSupport.getMenuItemPropertyModel(
                                mCustomTabActivityTestRule.getAppMenuCoordinator(),
                                R.id.icon_row_menu_id)
                        .get(AppMenuItemProperties.SUBMENU);
        final int expectedIconMenuSize = 4;
        assertEquals(expectedIconMenuSize, iconRowModelList.size());
        Assert.assertNotNull(
                AppMenuTestSupport.getMenuItemPropertyModel(
                        mCustomTabActivityTestRule.getAppMenuCoordinator(), R.id.forward_menu_id));
        Assert.assertNotNull(
                AppMenuTestSupport.getMenuItemPropertyModel(
                        mCustomTabActivityTestRule.getAppMenuCoordinator(),
                        R.id.bookmark_this_page_id));
        Assert.assertNotNull(
                AppMenuTestSupport.getMenuItemPropertyModel(
                        mCustomTabActivityTestRule.getAppMenuCoordinator(), R.id.info_menu_id));
        Assert.assertNotNull(
                AppMenuTestSupport.getMenuItemPropertyModel(
                        mCustomTabActivityTestRule.getAppMenuCoordinator(), R.id.reload_menu_id));

        CustomTabsTestUtils.assertMenuSize(menuItemsModelList, expectedMenuSize);
    }

    @Test
    @SmallTest
    public void testAppMenuBeforeFirstRun() throws Exception {
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(createMinimalCustomTabIntent());
        // Mark the first run as not completed. This has to be done after we start the intent,
        // otherwise we are going to hit the FRE.
        ThreadUtils.runOnUiThreadBlocking(() -> FirstRunStatus.setFirstRunFlowComplete(false));

        openAppMenuAndAssertMenuShown();
        ModelList menuItemsModelList =
                AppMenuTestSupport.getMenuModelList(
                        mCustomTabActivityTestRule.getAppMenuCoordinator());
        final int expectedMenuSize = 3;

        Assert.assertNotNull("App menu is not initialized: ", menuItemsModelList);

        // Checks the first row (icons).
        Assert.assertNotNull(
                AppMenuTestSupport.getMenuItemPropertyModel(
                        mCustomTabActivityTestRule.getAppMenuCoordinator(), R.id.forward_menu_id));
        Assert.assertNotNull(
                AppMenuTestSupport.getMenuItemPropertyModel(
                        mCustomTabActivityTestRule.getAppMenuCoordinator(), R.id.info_menu_id));
        Assert.assertNotNull(
                AppMenuTestSupport.getMenuItemPropertyModel(
                        mCustomTabActivityTestRule.getAppMenuCoordinator(), R.id.reload_menu_id));
        Assert.assertNull(
                AppMenuTestSupport.getMenuItemPropertyModel(
                        mCustomTabActivityTestRule.getAppMenuCoordinator(), R.id.offline_page_id));
        Assert.assertNull(
                AppMenuTestSupport.getMenuItemPropertyModel(
                        mCustomTabActivityTestRule.getAppMenuCoordinator(),
                        R.id.bookmark_this_page_id));

        // Following rows.
        Assert.assertNotNull(
                AppMenuTestSupport.getMenuItemPropertyModel(
                        mCustomTabActivityTestRule.getAppMenuCoordinator(), R.id.find_in_page_id));
        Assert.assertNotNull(
                AppMenuTestSupport.getMenuItemPropertyModel(
                        mCustomTabActivityTestRule.getAppMenuCoordinator(),
                        R.id.request_desktop_site_row_menu_id));
        Assert.assertNull(
                AppMenuTestSupport.getMenuItemPropertyModel(
                        mCustomTabActivityTestRule.getAppMenuCoordinator(),
                        R.id.open_in_browser_id));
        Assert.assertNull(
                AppMenuTestSupport.getMenuItemPropertyModel(
                        mCustomTabActivityTestRule.getAppMenuCoordinator(),
                        R.id.universal_install));
        Assert.assertNull(
                AppMenuTestSupport.getMenuItemPropertyModel(
                        mCustomTabActivityTestRule.getAppMenuCoordinator(),
                        R.id.open_history_menu_id));

        CustomTabsTestUtils.assertMenuSize(menuItemsModelList, expectedMenuSize);
    }

    /** Tests if the default share item can be shown in the app menu. */
    @Test
    @SmallTest
    @Restriction(DeviceRestriction.RESTRICTION_TYPE_NON_AUTO)
    public void testShareMenuItem() throws Exception {
        Intent intent = createMinimalCustomTabIntent();
        intent.putExtra(CustomTabsIntent.EXTRA_DEFAULT_SHARE_MENU_ITEM, true);
        intent.putExtra(CustomTabsIntent.EXTRA_SHARE_STATE, CustomTabsIntent.SHARE_STATE_OFF);
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);

        openAppMenuAndAssertMenuShown();
        PropertyModel sharePropertyModel =
                AppMenuTestSupport.getMenuItemPropertyModel(
                        mCustomTabActivityTestRule.getAppMenuCoordinator(), R.id.share_menu_id);
        Assert.assertNotNull(sharePropertyModel);
        Assert.assertTrue(sharePropertyModel.get(AppMenuItemProperties.ENABLED));
    }

    /**
     * Tests that the Add to Home screen item is not shown in the menu if there is no pin to home
     * screen capability
     */
    @Test
    @SmallTest
    public void testAddToHomeScreenMenuItemNoHomeScreen() throws Exception {
        // Clear default setting from #setUp.
        WebappsUtils.setAddToHomeIntentSupportedForTesting(null);
        Context contextToRestore = ContextUtils.getApplicationContext();
        TestContext testContext = new TestContext(contextToRestore);
        ContextUtils.initApplicationContextForTests(testContext);
        Intent intent = createMinimalCustomTabIntent();
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);

        openAppMenuAndAssertMenuShown();
        PropertyModel addToHomeScreenPropertyModel =
                AppMenuTestSupport.getMenuItemPropertyModel(
                        mCustomTabActivityTestRule.getAppMenuCoordinator(), R.id.universal_install);

        Assert.assertNull(addToHomeScreenPropertyModel);

        ContextUtils.initApplicationContextForTests(contextToRestore);
    }

    /** Test that only up to 7 entries are added to the custom menu. */
    @Test
    @SmallTest
    public void testMaxMenuItems() throws Exception {
        Intent intent = createMinimalCustomTabIntent();
        int numMenuEntries = 9;
        Assert.assertTrue(MAX_MENU_CUSTOM_ITEMS < numMenuEntries);
        CustomTabsIntentTestUtils.addMenuEntriesToIntent(intent, numMenuEntries, TEST_MENU_TITLE);
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);
        CustomTabAppMenuPropertiesDelegate propertiesDelegate =
                (CustomTabAppMenuPropertiesDelegate)
                        mCustomTabActivityTestRule
                                .getAppMenuCoordinator()
                                .getAppMenuPropertiesDelegate();
        propertiesDelegate.setHasClientPackageForTesting(true);

        openAppMenuAndAssertMenuShown();
        ModelList menuItemsModelList =
                AppMenuTestSupport.getMenuModelList(
                        mCustomTabActivityTestRule.getAppMenuCoordinator());
        final int expectedMenuSize = MAX_MENU_CUSTOM_ITEMS + NUM_CHROME_MENU_ITEMS_WITH_DIVIDER;
        Assert.assertNotNull("App menu is not initialized: ", menuItemsModelList);
        CustomTabsTestUtils.assertMenuSize(menuItemsModelList, adjustMenuSize(expectedMenuSize));
    }

    /**
     * Test whether the custom menu is correctly shown and clicking it sends the right {@link
     * PendingIntent}.
     */
    // TODO(crbug.com/40896028): Re-enable this test after fixing/diagnosing flakiness.
    @Test
    @SmallTest
    @DisabledTest(message = "https://crbug.com/1420991")
    public void testCustomMenuEntry() throws TimeoutException {
        Intent customTabIntent = createMinimalCustomTabIntent();
        Intent baseCallbackIntent = new Intent();
        baseCallbackIntent.putExtra("FOO", 42);
        final PendingIntent pi =
                CustomTabsIntentTestUtils.addMenuEntriesToIntent(
                        customTabIntent, 1, baseCallbackIntent, TEST_MENU_TITLE);
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(customTabIntent);

        final CustomTabsIntentTestUtils.OnFinishedForTest onFinished =
                new CustomTabsIntentTestUtils.OnFinishedForTest(pi);
        getCustomTabIntentDataProvider().setPendingIntentOnFinishedForTesting(onFinished);

        openAppMenuAndAssertMenuShown();
        PostTask.runOrPostTask(
                TaskTraits.UI_DEFAULT,
                () -> {
                    int itemId =
                            ((CustomTabAppMenuPropertiesDelegate)
                                            AppMenuTestSupport.getAppMenuPropertiesDelegate(
                                                    mCustomTabActivityTestRule
                                                            .getAppMenuCoordinator()))
                                    .getItemIdForTitle(TEST_MENU_TITLE);
                    Assert.assertNotEquals(AppMenuPropertiesDelegate.INVALID_ITEM_ID, itemId);
                    AppMenuTestSupport.onOptionsItemSelected(
                            mCustomTabActivityTestRule.getAppMenuCoordinator(), itemId);
                });

        onFinished.waitForCallback("Pending Intent was not sent.");
        Intent callbackIntent = onFinished.getCallbackIntent();
        assertThat(callbackIntent.getDataString(), equalTo(mTestPage));

        // Verify that the callback intent has the page title as the subject, but other extras are
        // kept intact.
        assertThat(callbackIntent.getStringExtra(Intent.EXTRA_SUBJECT), equalTo("The Google"));
        assertThat(callbackIntent.getIntExtra("FOO", 0), equalTo(42));
    }

    /**
     * Test whether clicking "Open in Chrome" takes us to a chrome normal tab, loading the same url.
     */
    @Test
    @SmallTest
    @DisabledTest(message = "https://crbug.com/361629264")
    public void testOpenInBrowser() throws Exception {
        // Augment the CustomTabsSession to catch the callback.
        CallbackHelper callbackTriggered = new CallbackHelper();
        CustomTabsSession session =
                CustomTabsTestUtils.bindWithCallback(
                                new CustomTabsCallback() {
                                    @Override
                                    public void extraCallback(String callbackName, Bundle args) {
                                        if (callbackName.equals(
                                                CustomTabsConnection.OPEN_IN_BROWSER_CALLBACK)) {
                                            callbackTriggered.notifyCalled();
                                        }
                                    }
                                })
                        .session;

        Intent intent = new CustomTabsIntent.Builder(session).build().intent;
        intent.setData(Uri.parse(mTestPage));
        intent.setComponent(
                new ComponentName(
                        ApplicationProvider.getApplicationContext(), ChromeLauncherActivity.class));
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        IntentUtils.addTrustedIntentExtras(intent);

        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);
        assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        LaunchCauseMetrics.LAUNCH_CAUSE_HISTOGRAM,
                        LaunchCauseMetrics.LaunchCause.CUSTOM_TAB));

        IntentFilter filter = new IntentFilter(Intent.ACTION_VIEW);
        filter.addDataScheme(
                Uri.parse(mCustomTabActivityTestRule.getTestServer().getURL("/")).getScheme());
        final Instrumentation.ActivityMonitor monitor =
                InstrumentationRegistry.getInstrumentation().addMonitor(filter, null, false);
        openAppMenuAndAssertMenuShown();
        PostTask.runOrPostTask(
                TaskTraits.UI_DEFAULT,
                () -> {
                    Assert.assertNotNull(
                            AppMenuTestSupport.getMenuItemPropertyModel(
                                    mCustomTabActivityTestRule.getAppMenuCoordinator(),
                                    R.id.open_in_browser_id));
                    mCustomTabActivityTestRule
                            .getActivity()
                            .onMenuOrKeyboardAction(R.id.open_in_browser_id, false);
                });
        final Activity activity =
                monitor.waitForActivityWithTimeout(CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL);

        callbackTriggered.waitForCallback(0);

        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    Criteria.checkThat(
                            RecordHistogram.getHistogramValueCountForTesting(
                                    LaunchCauseMetrics.LAUNCH_CAUSE_HISTOGRAM,
                                    LaunchCauseMetrics.LaunchCause.OPEN_IN_BROWSER_FROM_MENU),
                            Matchers.is(1));
                },
                5000L,
                CriteriaHelper.DEFAULT_POLLING_INTERVAL);
        activity.finish();
    }

    /**
     * Test whether clicking "Open in Incognito Chrome" takes us to a new chrome incognito tab,
     * loading the same url.
     */
    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.CCT_EPHEMERAL_MODE)
    public void testOpenInIncognitoBrowser() throws Exception {
        IncognitoReauthSettingUtils.setIsDeviceScreenLockEnabledForTesting(false);
        // Augment the CustomTabsSession to catch the callback.
        CallbackHelper callbackTriggered = new CallbackHelper();
        CustomTabsSession session =
                CustomTabsTestUtils.bindWithCallback(
                                new CustomTabsCallback() {
                                    @Override
                                    public void extraCallback(String callbackName, Bundle args) {
                                        if (callbackName.equals(
                                                CustomTabsConnection.OPEN_IN_BROWSER_CALLBACK)) {
                                            callbackTriggered.notifyCalled();
                                        }
                                    }
                                })
                        .session;

        Intent intent = new CustomTabsIntent.Builder(session).build().intent;
        // Set up an OTR custom tab to trigger "Open in Chrome Incognito".
        intent.putExtra(IntentHandler.EXTRA_ENABLE_EPHEMERAL_BROWSING, true);
        intent.setData(Uri.parse(mTestPage));
        intent.setComponent(
                new ComponentName(
                        ApplicationProvider.getApplicationContext(), ChromeLauncherActivity.class));
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        IntentUtils.addTrustedIntentExtras(intent);

        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);
        assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        LaunchCauseMetrics.LAUNCH_CAUSE_HISTOGRAM,
                        LaunchCauseMetrics.LaunchCause.CUSTOM_TAB));

        final Instrumentation.ActivityMonitor monitor =
                InstrumentationRegistry.getInstrumentation()
                        .addMonitor(ChromeTabbedActivity.class.getName(), null, false);
        openAppMenuAndAssertMenuShown();
        PostTask.runOrPostTask(
                TaskTraits.UI_DEFAULT,
                () -> {
                    Assert.assertNotNull(
                            AppMenuTestSupport.getMenuItemPropertyModel(
                                    mCustomTabActivityTestRule.getAppMenuCoordinator(),
                                    R.id.open_in_browser_id));
                    mCustomTabActivityTestRule
                            .getActivity()
                            .onMenuOrKeyboardAction(R.id.open_in_browser_id, false);
                });
        final ChromeTabbedActivity tabbedActivity =
                (ChromeTabbedActivity)
                        monitor.waitForActivityWithTimeout(CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL);

        callbackTriggered.waitForCallback(0);

        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    Criteria.checkThat(
                            RecordHistogram.getHistogramValueCountForTesting(
                                    LaunchCauseMetrics.LAUNCH_CAUSE_HISTOGRAM,
                                    LaunchCauseMetrics.LaunchCause.OPEN_IN_BROWSER_FROM_MENU),
                            is(1));

                    TabModel tabModel = tabbedActivity.getCurrentTabModel();
                    Criteria.checkThat(
                            "Incognito tab model not selected",
                            tabModel.isIncognitoBranded(),
                            is(true));

                    Tab tab = TabModelUtils.getCurrentTab(tabModel);
                    Criteria.checkThat("Tab is null", tab, Matchers.notNullValue());
                    Criteria.checkThat(
                            "Incognito tab not selected", tab.isIncognitoBranded(), is(true));
                    Criteria.checkThat(
                            "Wrong URL loaded in incognito tab",
                            ChromeTabUtils.getUrlStringOnUiThread(tab),
                            is(mTestPage));
                },
                5000L,
                CriteriaHelper.DEFAULT_POLLING_INTERVAL);

        tabbedActivity.finish();
    }

    @Test
    @SmallTest
    public void testOpenHistory() throws Exception {
        Intent intent = new CustomTabsIntent.Builder().build().intent;
        intent.setData(Uri.parse(mTestPage));
        intent.setComponent(
                new ComponentName(
                        ApplicationProvider.getApplicationContext(), ChromeLauncherActivity.class));
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        IntentUtils.addTrustedIntentExtras(intent);

        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);
        CustomTabAppMenuPropertiesDelegate propertiesDelegate =
                (CustomTabAppMenuPropertiesDelegate)
                        mCustomTabActivityTestRule
                                .getAppMenuCoordinator()
                                .getAppMenuPropertiesDelegate();
        propertiesDelegate.setHasClientPackageForTesting(true);
        assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        LaunchCauseMetrics.LAUNCH_CAUSE_HISTOGRAM,
                        LaunchCauseMetrics.LaunchCause.CUSTOM_TAB));

        openAppMenuAndAssertMenuShown();
        assertHistoryMenuVisibility();
    }

    @Test
    @SmallTest
    @DisableFeatures({ChromeFeatureList.APP_SPECIFIC_HISTORY})
    public void testNoHistoryItem_FeatureDisabled() throws Exception {
        Intent intent = createMinimalCustomTabIntent();
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);

        // History menu won't show, since the feature flag is disabled.
        assertHistoryMenuIsNotShown();
    }

    @Test
    @SmallTest
    @EnableFeatures({ChromeFeatureList.APP_SPECIFIC_HISTORY})
    public void testNoHistoryItem_NoClientPackage() throws Exception {
        Intent intent = createMinimalCustomTabIntent();
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);

        CustomTabAppMenuPropertiesDelegate propertiesDelegate =
                (CustomTabAppMenuPropertiesDelegate)
                        mCustomTabActivityTestRule
                                .getAppMenuCoordinator()
                                .getAppMenuPropertiesDelegate();
        propertiesDelegate.setHasClientPackageForTesting(false);

        // History menu won't show, since package name is not set.
        assertHistoryMenuIsNotShown();
    }
}

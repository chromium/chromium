// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;
import static org.robolectric.Shadows.shadowOf;

import android.Manifest;
import android.app.Activity;
import android.content.ComponentName;
import android.content.Intent;
import android.content.pm.ActivityInfo;
import android.content.pm.PackageInfo;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;

import org.chromium.base.supplier.SupplierUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.browserservices.intents.WebApkExtras;
import org.chromium.chrome.browser.browserservices.intents.WebappIcon;
import org.chromium.chrome.browser.browserservices.ui.controller.AuthTabVerifier;
import org.chromium.chrome.browser.browserservices.ui.controller.Verifier;
import org.chromium.chrome.browser.flags.ActivityType;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.fullscreen.BrowserControlsManager;
import org.chromium.chrome.browser.fullscreen.FullscreenManager;
import org.chromium.chrome.browser.init.ChromeActivityNativeDelegate;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabWebContentsDelegateAndroid;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.ui.ExclusiveAccessManager;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.desktop_windowing.DesktopWindowStateManager;
import org.chromium.components.browser_ui.util.BrowserControlsVisibilityDelegate;
import org.chromium.components.webapps.WebApkDistributor;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.webapk.lib.common.WebApkConstants;

import java.util.ArrayList;
import java.util.HashMap;

/** Tests for {@link CustomTabDelegateFactory} and its internal delegates. */
@RunWith(BaseRobolectricTestRunner.class)
@SuppressWarnings("deprecation")
public class CustomTabDelegateFactoryUnitTest {
    private static final String TEST_WEBAPK_PACKAGE_NAME = "org.chromium.webapk.testpackage";
    private static final String TEST_TWA_PACKAGE_NAME = "org.chromium.twa.testpackage";
    private static final String TWA_FOCUS_ACTIVITY_CLASS_NAME =
            "com.google.androidbrowserhelper.trusted.FocusActivity";

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private BrowserServicesIntentDataProvider mIntentDataProvider;
    @Mock private Tab mTab;
    @Mock private TabModelSelector mTabModelSelector;
    @Mock private TabModel mTabModel;
    @Mock private WindowAndroid mWindowAndroid;

    private Activity mActivity;
    private CustomTabDelegateFactory mFactory;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(Activity.class).get();
        shadowOf(mActivity).setInMultiWindowMode(true);
        // Common mocks for activateContents() execution.
        when(mTab.isIncognito()).thenReturn(false);
        when(mTab.isIncognitoBranded()).thenReturn(false);
        when(mTab.isInitialized()).thenReturn(true);
        when(mTabModelSelector.getModel(false)).thenReturn(mTabModel);
        when(mTabModel.indexOf(mTab)).thenReturn(0);
        when(mTab.getWindowAndroid()).thenReturn(mWindowAndroid);
        when(mWindowAndroid.isActivityTopResumedSupported()).thenReturn(false);
    }

    /** Installs the TWA package, optionally granting it the REORDER_TASKS permission. */
    private void installTwaPackage(boolean grantReorderTasks) {
        PackageInfo packageInfo = new PackageInfo();
        packageInfo.packageName = TEST_TWA_PACKAGE_NAME;
        packageInfo.requestedPermissions = new String[] {Manifest.permission.REORDER_TASKS};
        packageInfo.requestedPermissionsFlags =
                new int[] {grantReorderTasks ? PackageInfo.REQUESTED_PERMISSION_GRANTED : 0};
        shadowOf(mActivity.getPackageManager()).installPackage(packageInfo);
    }

    /** Adds the TWA's FocusActivity with the given task affinity. */
    private void addTwaFocusActivity(String taskAffinity) {
        ActivityInfo activityInfo = new ActivityInfo();
        activityInfo.packageName = TEST_TWA_PACKAGE_NAME;
        activityInfo.name = TWA_FOCUS_ACTIVITY_CLASS_NAME;
        activityInfo.taskAffinity = taskAffinity;
        shadowOf(mActivity.getPackageManager()).addOrUpdateActivity(activityInfo);
    }

    private void createFactory(@ActivityType int activityType) {
        mFactory =
                new CustomTabDelegateFactory(
                        mActivity,
                        mIntentDataProvider,
                        new BrowserControlsVisibilityDelegate(),
                        mock(Verifier.class),
                        mock(ChromeActivityNativeDelegate.class),
                        mock(BrowserControlsStateProvider.class),
                        mock(FullscreenManager.class),
                        mock(TabCreatorManager.class),
                        () -> mTabModelSelector,
                        SupplierUtils.ofNull(),
                        SupplierUtils.ofNull(),
                        SupplierUtils.ofNull(),
                        SupplierUtils.ofNull(),
                        activityType,
                        () -> mock(BottomSheetController.class),
                        mock(AuthTabVerifier.class),
                        mock(BrowserControlsManager.class),
                        SupplierUtils.of(/* value= */ false),
                        SupplierUtils.of(/* value= */ false),
                        mock(ExclusiveAccessManager.class),
                        mock(DesktopWindowStateManager.class));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.USE_APP_TASK_FOR_CUSTOM_TAB_ACTIVATION)
    public void testBringActivityToForeground_WebApk() {
        // Mock WebAPK configurations.
        when(mIntentDataProvider.getActivityType()).thenReturn(ActivityType.WEB_APK);
        createFactory(ActivityType.WEB_APK);
        WebApkExtras webApkExtras =
                new WebApkExtras(
                        TEST_WEBAPK_PACKAGE_NAME,
                        new WebappIcon(),
                        /* isSplashIconMaskable= */ false,
                        /* shellApkVersion= */ 0,
                        /* manifestUrl= */ null,
                        /* manifestStartUrl= */ null,
                        /* manifestId= */ null,
                        /* appKey= */ null,
                        WebApkDistributor.OTHER,
                        new HashMap<>(),
                        /* shareTarget= */ null,
                        /* isSplashProvidedByWebApk= */ false,
                        new ArrayList<>(),
                        /* webApkVersionCode= */ 0,
                        /* lastUpdateTime= */ 0,
                        /* hasCustomName= */ false);
        when(mIntentDataProvider.getWebApkExtras()).thenReturn(webApkExtras);

        TabWebContentsDelegateAndroid delegate = mFactory.createWebContentsDelegate(mTab);
        Assert.assertNotNull(delegate);

        // Invoke activateContents() which delegates to bringActivityToForeground().
        delegate.activateContents();

        // Verify the started Intent.
        Intent intent = shadowOf(mActivity).getNextStartedActivity();
        Assert.assertNotNull(intent);

        ComponentName component = intent.getComponent();
        Assert.assertNotNull(component);
        Assert.assertEquals(TEST_WEBAPK_PACKAGE_NAME, component.getPackageName());
        Assert.assertEquals(
                WebApkConstants.WEBAPK_OPAQUE_MAIN_ACTIVITY_CLASS_NAME, component.getClassName());

        Assert.assertTrue(
                intent.getBooleanExtra(
                        WebApkConstants.EXTRA_BRING_TO_FRONT, /* defaultValue= */ false));
        Assert.assertEquals(
                Intent.FLAG_ACTIVITY_NEW_TASK, intent.getFlags() & Intent.FLAG_ACTIVITY_NEW_TASK);
        Assert.assertEquals(
                Intent.FLAG_ACTIVITY_EXCLUDE_FROM_RECENTS,
                intent.getFlags() & Intent.FLAG_ACTIVITY_EXCLUDE_FROM_RECENTS);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.USE_APP_TASK_FOR_CUSTOM_TAB_ACTIVATION)
    public void testBringActivityToForeground_Twa() {
        installTwaPackage(/* grantReorderTasks= */ true);
        addTwaFocusActivity(/* taskAffinity= */ TEST_TWA_PACKAGE_NAME);

        // Mock TWA configurations using doReturn to bypass final method calls.
        doReturn(ActivityType.TRUSTED_WEB_ACTIVITY).when(mIntentDataProvider).getActivityType();
        doReturn(TEST_TWA_PACKAGE_NAME).when(mIntentDataProvider).getClientPackageName();
        createFactory(ActivityType.TRUSTED_WEB_ACTIVITY);

        TabWebContentsDelegateAndroid delegate = mFactory.createWebContentsDelegate(mTab);
        Assert.assertNotNull(delegate);

        // Invoke activateContents() which delegates to bringActivityToForeground().
        delegate.activateContents();

        // Verify the started Intent.
        Intent intent = shadowOf(mActivity).getNextStartedActivity();
        Assert.assertNotNull(intent);

        ComponentName component = intent.getComponent();
        Assert.assertNotNull(component);
        Assert.assertEquals(TEST_TWA_PACKAGE_NAME, component.getPackageName());
        Assert.assertEquals(TWA_FOCUS_ACTIVITY_CLASS_NAME, component.getClassName());

        Assert.assertEquals(
                Intent.FLAG_ACTIVITY_NEW_TASK, intent.getFlags() & Intent.FLAG_ACTIVITY_NEW_TASK);
        Assert.assertEquals(
                Intent.FLAG_ACTIVITY_EXCLUDE_FROM_RECENTS,
                intent.getFlags() & Intent.FLAG_ACTIVITY_EXCLUDE_FROM_RECENTS);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.USE_APP_TASK_FOR_CUSTOM_TAB_ACTIVATION)
    public void testBringActivityToForeground_Twa_MissingReorderPermission_FallsBack() {
        installTwaPackage(/* grantReorderTasks= */ false);
        addTwaFocusActivity(/* taskAffinity= */ TEST_TWA_PACKAGE_NAME);

        doReturn(ActivityType.TRUSTED_WEB_ACTIVITY).when(mIntentDataProvider).getActivityType();
        doReturn(TEST_TWA_PACKAGE_NAME).when(mIntentDataProvider).getClientPackageName();
        createFactory(ActivityType.TRUSTED_WEB_ACTIVITY);

        TabWebContentsDelegateAndroid delegate = mFactory.createWebContentsDelegate(mTab);
        Assert.assertNotNull(delegate);

        delegate.activateContents();

        Assert.assertNull(shadowOf(mActivity).getNextStartedActivity());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.USE_APP_TASK_FOR_CUSTOM_TAB_ACTIVATION)
    public void testBringActivityToForeground_Twa_FocusActivityNotFound_FallsBack() {
        installTwaPackage(/* grantReorderTasks= */ true);

        doReturn(ActivityType.TRUSTED_WEB_ACTIVITY).when(mIntentDataProvider).getActivityType();
        doReturn(TEST_TWA_PACKAGE_NAME).when(mIntentDataProvider).getClientPackageName();
        createFactory(ActivityType.TRUSTED_WEB_ACTIVITY);

        TabWebContentsDelegateAndroid delegate = mFactory.createWebContentsDelegate(mTab);
        Assert.assertNotNull(delegate);

        delegate.activateContents();

        Assert.assertNull(shadowOf(mActivity).getNextStartedActivity());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.USE_APP_TASK_FOR_CUSTOM_TAB_ACTIVATION)
    public void testBringActivityToForeground_Twa_EmptyTaskAffinity_FallsBack() {
        installTwaPackage(/* grantReorderTasks= */ true);
        addTwaFocusActivity(/* taskAffinity= */ "");

        doReturn(ActivityType.TRUSTED_WEB_ACTIVITY).when(mIntentDataProvider).getActivityType();
        doReturn(TEST_TWA_PACKAGE_NAME).when(mIntentDataProvider).getClientPackageName();
        createFactory(ActivityType.TRUSTED_WEB_ACTIVITY);

        TabWebContentsDelegateAndroid delegate = mFactory.createWebContentsDelegate(mTab);
        Assert.assertNotNull(delegate);

        delegate.activateContents();

        Assert.assertNull(shadowOf(mActivity).getNextStartedActivity());
    }

    @Test
    public void testEnvironmentQueriesAcrossActivityTypes() {
        // 1. CUSTOM_TAB
        createFactory(ActivityType.CUSTOM_TAB);
        Assert.assertTrue("CUSTOM_TAB should return isCustomTab() = true", mFactory.isCustomTab());
        Assert.assertFalse("CUSTOM_TAB should return isTabInPwa() = false", mFactory.isTabInPwa());
        Assert.assertFalse(
                "CUSTOM_TAB should return isTabInBrowser() = false", mFactory.isTabInBrowser());

        // 2. AUTH_TAB
        createFactory(ActivityType.AUTH_TAB);
        Assert.assertTrue("AUTH_TAB should return isCustomTab() = true", mFactory.isCustomTab());
        Assert.assertFalse("AUTH_TAB should return isTabInPwa() = false", mFactory.isTabInPwa());
        Assert.assertFalse(
                "AUTH_TAB should return isTabInBrowser() = false", mFactory.isTabInBrowser());

        // 3. TRUSTED_WEB_ACTIVITY (TWA)
        createFactory(ActivityType.TRUSTED_WEB_ACTIVITY);
        Assert.assertTrue(
                "TRUSTED_WEB_ACTIVITY should return isCustomTab() = true", mFactory.isCustomTab());
        Assert.assertTrue(
                "TRUSTED_WEB_ACTIVITY should return isTabInPwa() = true", mFactory.isTabInPwa());
        Assert.assertFalse(
                "TRUSTED_WEB_ACTIVITY should return isTabInBrowser() = false",
                mFactory.isTabInBrowser());

        // 4. WEB_APK
        createFactory(ActivityType.WEB_APK);
        Assert.assertFalse("WEB_APK should return isCustomTab() = false", mFactory.isCustomTab());
        Assert.assertTrue("WEB_APK should return isTabInPwa() = true", mFactory.isTabInPwa());
        Assert.assertFalse(
                "WEB_APK should return isTabInBrowser() = false", mFactory.isTabInBrowser());

        // 5. TABBED
        createFactory(ActivityType.TABBED);
        Assert.assertFalse("TABBED should return isCustomTab() = false", mFactory.isCustomTab());
        Assert.assertFalse("TABBED should return isTabInPwa() = false", mFactory.isTabInPwa());
        Assert.assertFalse(
                "TABBED should return isTabInBrowser() = false", mFactory.isTabInBrowser());
    }
}

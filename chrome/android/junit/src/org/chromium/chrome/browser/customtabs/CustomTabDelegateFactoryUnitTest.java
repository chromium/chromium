// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.Manifest;
import android.app.Activity;
import android.content.ComponentName;
import android.content.Intent;
import android.content.pm.ActivityInfo;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

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

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Activity mActivity;
    @Mock private PackageManager mPackageManager;
    @Mock private BrowserServicesIntentDataProvider mIntentDataProvider;
    @Mock private Tab mTab;
    @Mock private TabModelSelector mTabModelSelector;
    @Mock private TabModel mTabModel;
    @Mock private WindowAndroid mWindowAndroid;

    private CustomTabDelegateFactory mFactory;

    @Before
    public void setUp() {
        // Common mocks for activateContents() execution.
        when(mActivity.getPackageManager()).thenReturn(mPackageManager);
        when(mTab.isIncognito()).thenReturn(false);
        when(mTab.isIncognitoBranded()).thenReturn(false);
        when(mTab.isInitialized()).thenReturn(true);
        when(mTabModelSelector.getModel(false)).thenReturn(mTabModel);
        when(mTabModel.indexOf(mTab)).thenReturn(0);
        when(mTab.getWindowAndroid()).thenReturn(mWindowAndroid);
        when(mWindowAndroid.isActivityTopResumedSupported()).thenReturn(false);
        when(mActivity.isInMultiWindowMode()).thenReturn(true);
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

        // Intercept and verify the Intent.
        ArgumentCaptor<Intent> intentCaptor = ArgumentCaptor.forClass(Intent.class);
        verify(mActivity).startActivity(intentCaptor.capture());

        Intent intent = intentCaptor.getValue();
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
        final String twaPackageName = "org.chromium.twa.testpackage";
        when(mPackageManager.checkPermission(Manifest.permission.REORDER_TASKS, twaPackageName))
                .thenReturn(PackageManager.PERMISSION_GRANTED);
        ResolveInfo resolveInfo = new ResolveInfo();
        resolveInfo.activityInfo = new ActivityInfo();
        resolveInfo.activityInfo.taskAffinity = twaPackageName;
        when(mPackageManager.resolveActivity(any(), anyInt())).thenReturn(resolveInfo);

        // Mock TWA configurations using doReturn to bypass final method calls.
        doReturn(ActivityType.TRUSTED_WEB_ACTIVITY).when(mIntentDataProvider).getActivityType();
        doReturn(twaPackageName).when(mIntentDataProvider).getClientPackageName();
        createFactory(ActivityType.TRUSTED_WEB_ACTIVITY);

        TabWebContentsDelegateAndroid delegate = mFactory.createWebContentsDelegate(mTab);
        Assert.assertNotNull(delegate);

        // Invoke activateContents() which delegates to bringActivityToForeground().
        delegate.activateContents();

        // Intercept and verify the Intent.
        ArgumentCaptor<Intent> intentCaptor = ArgumentCaptor.forClass(Intent.class);
        verify(mActivity).startActivity(intentCaptor.capture());

        Intent intent = intentCaptor.getValue();
        Assert.assertNotNull(intent);

        ComponentName component = intent.getComponent();
        Assert.assertNotNull(component);
        Assert.assertEquals(twaPackageName, component.getPackageName());
        Assert.assertEquals(
                "com.google.androidbrowserhelper.trusted.FocusActivity", component.getClassName());

        Assert.assertEquals(
                Intent.FLAG_ACTIVITY_NEW_TASK, intent.getFlags() & Intent.FLAG_ACTIVITY_NEW_TASK);
        Assert.assertEquals(
                Intent.FLAG_ACTIVITY_EXCLUDE_FROM_RECENTS,
                intent.getFlags() & Intent.FLAG_ACTIVITY_EXCLUDE_FROM_RECENTS);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.USE_APP_TASK_FOR_CUSTOM_TAB_ACTIVATION)
    public void testBringActivityToForeground_Twa_MissingReorderPermission_FallsBack() {
        final String twaPackageName = "org.chromium.twa.testpackage";
        when(mPackageManager.checkPermission(Manifest.permission.REORDER_TASKS, twaPackageName))
                .thenReturn(PackageManager.PERMISSION_DENIED);

        doReturn(ActivityType.TRUSTED_WEB_ACTIVITY).when(mIntentDataProvider).getActivityType();
        doReturn(twaPackageName).when(mIntentDataProvider).getClientPackageName();
        createFactory(ActivityType.TRUSTED_WEB_ACTIVITY);

        TabWebContentsDelegateAndroid delegate = mFactory.createWebContentsDelegate(mTab);
        Assert.assertNotNull(delegate);

        delegate.activateContents();

        verify(mActivity, never()).startActivity(any());
        verify(mActivity).getTaskId();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.USE_APP_TASK_FOR_CUSTOM_TAB_ACTIVATION)
    public void testBringActivityToForeground_Twa_FocusActivityNotFound_FallsBack() {
        final String twaPackageName = "org.chromium.twa.testpackage";
        when(mPackageManager.checkPermission(Manifest.permission.REORDER_TASKS, twaPackageName))
                .thenReturn(PackageManager.PERMISSION_GRANTED);
        when(mPackageManager.resolveActivity(any(), anyInt())).thenReturn(null);

        doReturn(ActivityType.TRUSTED_WEB_ACTIVITY).when(mIntentDataProvider).getActivityType();
        doReturn(twaPackageName).when(mIntentDataProvider).getClientPackageName();
        createFactory(ActivityType.TRUSTED_WEB_ACTIVITY);

        TabWebContentsDelegateAndroid delegate = mFactory.createWebContentsDelegate(mTab);
        Assert.assertNotNull(delegate);

        delegate.activateContents();

        verify(mActivity, never()).startActivity(any());
        verify(mActivity).getTaskId();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.USE_APP_TASK_FOR_CUSTOM_TAB_ACTIVATION)
    public void testBringActivityToForeground_Twa_EmptyTaskAffinity_FallsBack() {
        final String twaPackageName = "org.chromium.twa.testpackage";
        when(mPackageManager.checkPermission(Manifest.permission.REORDER_TASKS, twaPackageName))
                .thenReturn(PackageManager.PERMISSION_GRANTED);
        ResolveInfo resolveInfo = new ResolveInfo();
        resolveInfo.activityInfo = new ActivityInfo();
        resolveInfo.activityInfo.taskAffinity = "";
        when(mPackageManager.resolveActivity(any(), anyInt())).thenReturn(resolveInfo);

        doReturn(ActivityType.TRUSTED_WEB_ACTIVITY).when(mIntentDataProvider).getActivityType();
        doReturn(twaPackageName).when(mIntentDataProvider).getClientPackageName();
        createFactory(ActivityType.TRUSTED_WEB_ACTIVITY);

        TabWebContentsDelegateAndroid delegate = mFactory.createWebContentsDelegate(mTab);
        Assert.assertNotNull(delegate);

        delegate.activateContents();

        verify(mActivity, never()).startActivity(any());
        verify(mActivity).getTaskId();
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

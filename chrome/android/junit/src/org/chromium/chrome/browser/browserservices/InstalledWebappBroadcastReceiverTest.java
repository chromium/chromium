// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.content.Intent;
import android.net.Uri;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.RuntimeEnvironment;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.app.tabwindow.TabWindowManagerSingleton;
import org.chromium.chrome.browser.browserservices.permissiondelegation.InstalledWebappPermissionStore;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.notifications.channels.SiteChannelsManager;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabwindow.TabWindowManager;
import org.chromium.chrome.browser.webapps.WebappRegistry;
import org.chromium.components.browser_ui.notifications.BaseNotificationManagerProxyFactory;
import org.chromium.components.browser_ui.notifications.NotificationManagerProxy;
import org.chromium.components.browser_ui.notifications.NotificationWrapper;
import org.chromium.components.embedder_support.util.Origin;
import org.chromium.components.permissions.PermissionsAndroidFeatureList;
import org.chromium.components.webapps.AppBannerManager;
import org.chromium.components.webapps.AppBannerManagerJni;
import org.chromium.content_public.browser.WebContents;
import org.chromium.url.GURL;

import java.util.Arrays;
import java.util.Collections;
import java.util.HashSet;
import java.util.Set;

/** Tests for {@link InstalledWebappBroadcastReceiver}. */
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures(PermissionsAndroidFeatureList.APPROXIMATE_GEOLOCATION_PERMISSION)
public class InstalledWebappBroadcastReceiverTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock public Context mContext;
    @Mock public InstalledWebappBroadcastReceiver.ClearDataStrategy mMockStrategy;
    @Mock public InstalledWebappPermissionStore mStore;
    @Mock public SiteChannelsManager mSiteChannelsManager;
    @Mock public NotificationManagerProxy mNotificationManager;
    @Mock private LibraryLoader mMockLibraryLoader;
    @Mock private AppBannerManager.Natives mMockAppBannerManagerJni;
    @Mock private TabWindowManager mTabWindowManager;
    @Mock private TabModelSelector mTabModelSelector;
    @Mock private TabModel mTabModel;
    @Mock private Tab mTab;
    @Mock private WebContents mWebContents;
    @Mock private AppBannerManager mAppBannerManager;

    private InstalledWebappBroadcastReceiver mReceiver;

    @Before
    public void setUp() {
        WebappRegistry.getInstance().setPermissionStoreForTesting(mStore);
        SiteChannelsManager.setInstanceForTesting(mSiteChannelsManager);
        BaseNotificationManagerProxyFactory.setInstanceForTesting(mNotificationManager);

        mReceiver = new InstalledWebappBroadcastReceiver(mMockStrategy);
        mContext = RuntimeEnvironment.application;

        AppBannerManagerJni.setInstanceForTesting(mMockAppBannerManagerJni);
        TabWindowManagerSingleton.setTabWindowManagerForTesting(mTabWindowManager);
    }

    @After
    public void tearDown() {
        AppBannerManagerJni.setInstanceForTesting(null);
        TabWindowManagerSingleton.resetTabModelSelectorFactoryForTesting();
        ApplicationStatus.destroyForJUnitTests();
    }

    private Intent createMockIntent(int id, String action) {
        Intent intent = new Intent();
        intent.putExtra(Intent.EXTRA_UID, id);
        intent.setAction(action);
        intent.setData(Uri.parse("package:com.package"));
        return intent;
    }

    private void addToRegister(int id, String appName, Set<GURL> urls) {
        for (GURL gurl : urls) {
            InstalledWebappDataRegister.registerPackageForOrigin(
                    appName, "com.package", gurl.getHost(), Origin.create(gurl.getSpec()));
        }
    }

    /** Makes sure we don't show a notification if we don't have any data for the app. */
    @Test
    @Feature("TrustedWebActivities")
    public void chromeHoldsNoData() {
        mReceiver.onReceive(mContext, createMockIntent(12, Intent.ACTION_PACKAGE_FULLY_REMOVED));

        verify(mMockStrategy, never()).execute(any(), any(), anyBoolean());
    }

    /** Tests the basic flow. */
    @Test
    @Feature("TrustedWebActivities")
    public void chromeHoldsData() {
        int id = 23;
        String appName = "App Name";
        GURL url = new GURL("https://www.example.com");
        Set<GURL> urls = new HashSet<>(Arrays.asList(url));

        addToRegister(id, appName, urls);

        mReceiver.onReceive(mContext, createMockIntent(id, Intent.ACTION_PACKAGE_FULLY_REMOVED));

        verify(mMockStrategy).execute(any(), eq("com.package"), eq(true));
    }

    /** Tests we plumb the correct information to the {@link ClearDataDialogActivity}. */
    @Test
    @Feature("TrustedWebActivities")
    public void execute_ValidIntent() {
        mReceiver =
                new InstalledWebappBroadcastReceiver(
                        new InstalledWebappBroadcastReceiver.ClearDataStrategy());

        int id = 67;
        String appName = "App Name 3";
        GURL url1 = new GURL("https://www.example.com");
        GURL url2 = new GURL("https://www.example2.com");
        Set<GURL> urls = new HashSet<>(Arrays.asList(url1, url2));
        Set<String> domains = new HashSet<>(Arrays.asList(url1.getHost(), url2.getHost()));

        addToRegister(id, appName, urls);

        Context context = mock(Context.class);

        mReceiver.onReceive(context, createMockIntent(id, Intent.ACTION_PACKAGE_FULLY_REMOVED));

        ArgumentCaptor<Intent> intentArgumentCaptor = ArgumentCaptor.forClass(Intent.class);
        verify(context).startActivity(intentArgumentCaptor.capture());

        Intent intent = intentArgumentCaptor.getValue();

        assertEquals(appName, ClearDataDialogActivity.getAppNameFromIntent(intent));
        assertTrue(ClearDataDialogActivity.getIsAppUninstalledFromIntent(intent));
        assertEquals(domains, new HashSet<>(ClearDataDialogActivity.getDomainsFromIntent(intent)));
    }

    /** Tests we call the PermissionUpdater. */
    @Test
    @Feature("TrustedwebActivities")
    public void execute_UpdatePermissions() {
        mReceiver =
                new InstalledWebappBroadcastReceiver(
                        new InstalledWebappBroadcastReceiver.ClearDataStrategy());

        int id = 67;
        String appName = "App Name 3";
        GURL url1 = new GURL("https://www.example.com");
        GURL url2 = new GURL("https://www.example2.com");
        Set<GURL> urls = new HashSet<>(Arrays.asList(url1, url2));

        addToRegister(id, appName, urls);

        mReceiver.onReceive(mContext, createMockIntent(id, Intent.ACTION_PACKAGE_FULLY_REMOVED));

        verify(mStore, times(2)).resetPermission(eq(Origin.create(url1.getSpec())), anyInt());
        verify(mStore, times(2)).resetPermission(eq(Origin.create(url2.getSpec())), anyInt());
    }

    /** Tests we differentiate between app uninstalled and data cleared. */
    @Test
    @Feature("TrustedWebActivities")
    public void onDataClear() {
        int id = 23;
        String appName = "App Name";
        Set<GURL> urls = new HashSet<>(Arrays.asList(new GURL("https://www.example.com")));

        addToRegister(id, appName, urls);

        mReceiver.onReceive(mContext, createMockIntent(id, Intent.ACTION_PACKAGE_DATA_CLEARED));
        verify(mMockStrategy).execute(any(), eq("com.package"), eq(false));
    }

    @Test
    @Feature("WebApk")
    public void webApkUninstall_DefersTracking() {
        String webApkPackage = "org.chromium.webapk.test";
        Intent intent = new Intent();
        intent.putExtra(Intent.EXTRA_UID, 12);
        intent.setAction(Intent.ACTION_PACKAGE_FULLY_REMOVED);
        intent.setData(Uri.parse("package:" + webApkPackage));

        mReceiver.onReceive(mContext, intent);

        Set<String> uninstalled =
                ChromeSharedPreferences.getInstance()
                        .readStringSet(ChromePreferenceKeys.WEBAPK_UNINSTALLED_PACKAGES);
        assertTrue(uninstalled.contains(webApkPackage));
    }

    @Test
    @Feature("TrustedWebActivities")
    public void twaUninstall_TriggersRecheck() throws Exception {
        // Mock native loaded.
        when(mMockLibraryLoader.isInitialized()).thenReturn(true);
        LibraryLoader.setLibraryLoaderForTesting(mMockLibraryLoader);

        int id = 23;
        String appName = "App Name";
        String scope = "https://www.example.com/scope";
        Set<GURL> urls = new HashSet<>(Arrays.asList(new GURL(scope)));

        addToRegister(id, appName, urls);

        // Setup TabWindowManager to return the selector.
        when(mTabWindowManager.getAllTabModelSelectors())
                .thenReturn(Collections.singletonList(mTabModelSelector));
        when(mTabWindowManager.getCustomTabsTabModelSelectors())
                .thenReturn(Collections.emptyList());

        when(mTabModelSelector.getModels()).thenReturn(Collections.singletonList(mTabModel));
        when(mTabModel.getCount()).thenReturn(1);
        when(mTabModel.getTabAt(0)).thenReturn(mTab);
        when(mTab.getWebContents()).thenReturn(mWebContents);

        // Mock tab URL to match scope.
        GURL url = new GURL(scope + "/page.html");
        when(mWebContents.getLastCommittedUrl()).thenReturn(url);

        // Mock AppBannerManager.forWebContents.
        when(mMockAppBannerManagerJni.getJavaBannerManagerForWebContents(mWebContents))
                .thenReturn(mAppBannerManager);

        // Send broadcast.
        mReceiver.onReceive(mContext, createMockIntent(id, Intent.ACTION_PACKAGE_FULLY_REMOVED));

        // Verify AppBannerManager JNI recheck is called.
        verify(mAppBannerManager).recheckInstallability();
    }

    @Test
    @Feature("TrustedWebActivities")
    public void packageRemoved_TriggersExecute() {
        int id = 23;
        String appName = "App Name";
        GURL url = new GURL("https://www.example.com");
        Set<GURL> urls = new HashSet<>(Arrays.asList(url));

        addToRegister(id, appName, urls);

        mReceiver.onReceive(mContext, createMockIntent(id, Intent.ACTION_PACKAGE_REMOVED));

        verify(mMockStrategy).execute(any(), eq("com.package"), eq(true));
    }

    @Test
    @Feature("TrustedWebActivities")
    public void packageRemoved_Replacing_DoesNotTrigger() {
        int id = 23;
        String appName = "App Name";
        GURL url = new GURL("https://www.example.com");
        Set<GURL> urls = new HashSet<>(Arrays.asList(url));

        addToRegister(id, appName, urls);

        Intent intent = createMockIntent(id, Intent.ACTION_PACKAGE_REMOVED);
        intent.putExtra(Intent.EXTRA_REPLACING, true);
        mReceiver.onReceive(mContext, intent);

        verify(mMockStrategy, never()).execute(any(), any(), anyBoolean());
    }

    @Test
    @Feature("TrustedWebActivities")
    @EnableFeatures(ChromeFeatureList.DESKTOP_ANDROID_TWA_DELETE_BROWSER_DATA)
    public void execute_FlagEnabled_ShowsNotification() {
        mReceiver =
                new InstalledWebappBroadcastReceiver(
                        new InstalledWebappBroadcastReceiver.ClearDataStrategy());

        int id = 67;
        String appName = "App Name 3";
        GURL url = new GURL("https://www.example.com");
        Set<GURL> urls = new HashSet<>(Arrays.asList(url));

        addToRegister(id, appName, urls);

        mReceiver.onReceive(mContext, createMockIntent(id, Intent.ACTION_PACKAGE_FULLY_REMOVED));

        verify(mNotificationManager).notify(any(NotificationWrapper.class));
    }

    @Test
    @Feature("TrustedWebActivities")
    @DisableFeatures(ChromeFeatureList.DESKTOP_ANDROID_TWA_DELETE_BROWSER_DATA)
    public void execute_FlagDisabled_StartsActivity() {
        mReceiver =
                new InstalledWebappBroadcastReceiver(
                        new InstalledWebappBroadcastReceiver.ClearDataStrategy());

        int id = 67;
        String appName = "App Name 3";
        GURL url = new GURL("https://www.example.com");
        Set<GURL> urls = new HashSet<>(Arrays.asList(url));

        addToRegister(id, appName, urls);

        Context context = mock(Context.class);
        mReceiver.onReceive(context, createMockIntent(id, Intent.ACTION_PACKAGE_FULLY_REMOVED));

        verify(mNotificationManager, never()).notify(any(NotificationWrapper.class));
        verify(context).startActivity(any());
    }
}

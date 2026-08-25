// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.RobolectricUtil;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.chrome.browser.app.tabwindow.TabWindowManagerSingleton;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.browserservices.metrics.WebApkUkmRecorder;
import org.chromium.chrome.browser.browserservices.metrics.WebApkUkmRecorderJni;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabwindow.TabWindowManager;
import org.chromium.chrome.test.util.browser.webapps.WebApkIntentDataProviderBuilder;
import org.chromium.components.webapps.AppBannerManager;
import org.chromium.components.webapps.AppBannerManagerJni;
import org.chromium.content_public.browser.WebContents;
import org.chromium.url.GURL;

import java.util.Collections;

/** Tests the WebApkUninstallTracker class. */
@RunWith(BaseRobolectricTestRunner.class)
public class WebApkUninstallTrackerTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private WebApkSyncService.Natives mMockWebApkSyncServiceJni;
    @Mock private AppBannerManager.Natives mMockAppBannerManagerJni;
    @Mock private WebApkUkmRecorder.Natives mMockWebApkUkmRecorderJni;
    @Mock private LibraryLoader mMockLibraryLoader;

    @Mock private TabWindowManager mTabWindowManager;
    @Mock private TabModelSelector mTabModelSelector;
    @Mock private TabModel mTabModel;
    @Mock private Tab mTab;
    @Mock private WebContents mWebContents;
    @Mock private AppBannerManager mAppBannerManager;

    private static final String WEBAPK_PACKAGE = "org.chromium.webapk.test";
    private static final String MANIFEST_ID = "https://example.com/manifest";
    private static final String SCOPE = "https://example.com/scope";

    @Before
    public void setUp() {
        WebApkSyncServiceJni.setInstanceForTesting(mMockWebApkSyncServiceJni);
        AppBannerManagerJni.setInstanceForTesting(mMockAppBannerManagerJni);
        WebApkUkmRecorderJni.setInstanceForTesting(mMockWebApkUkmRecorderJni);

        when(mMockLibraryLoader.isInitialized()).thenReturn(true);
        LibraryLoader.setLibraryLoaderForTesting(mMockLibraryLoader);
        TabWindowManagerSingleton.setTabWindowManagerForTesting(mTabWindowManager);
    }

    @After
    public void tearDown() {
        WebApkSyncServiceJni.setInstanceForTesting(null);
        AppBannerManagerJni.setInstanceForTesting(null);
        WebApkUkmRecorderJni.setInstanceForTesting(null);
        TabWindowManagerSingleton.resetTabModelSelectorFactoryForTesting();
        ApplicationStatus.destroyForJUnitTests();
    }

    private WebappDataStorage registerWebappAndGetStorage(String packageName) throws Exception {
        String webappId =
                org.chromium.chrome.browser.browserservices.intents.WebappIntentUtils
                        .getIdForWebApkPackage(packageName);
        CallbackHelper helper = new CallbackHelper();
        WebappRegistry.getInstance()
                .register(
                        webappId,
                        new WebappRegistry.FetchWebappDataStorageCallback() {
                            @Override
                            public void onWebappDataStorageRetrieved(WebappDataStorage storage) {
                                helper.notifyCalled();
                            }
                        });
        RobolectricUtil.runAllBackgroundAndUi();
        helper.waitForOnly();

        return WebappRegistry.getInstance().getWebappDataStorage(webappId);
    }

    @Test
    public void testUninstallTriggersSyncAndRecheck() throws Exception {
        // Register WebAPK in WebappRegistry.
        WebappDataStorage storage = registerWebappAndGetStorage(WEBAPK_PACKAGE);

        BrowserServicesIntentDataProvider intentDataProvider =
                new WebApkIntentDataProviderBuilder(WEBAPK_PACKAGE, SCOPE + "/start")
                        .setWebApkManifestId(MANIFEST_ID)
                        .setScope(SCOPE)
                        .build();
        storage.updateFromWebappIntentDataProvider(intentDataProvider);

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
        GURL url = new GURL(SCOPE + "/page.html");
        when(mWebContents.getLastCommittedUrl()).thenReturn(url);

        // Mock AppBannerManager.forWebContents.
        when(mMockAppBannerManagerJni.getJavaBannerManagerForWebContents(mWebContents))
                .thenReturn(mAppBannerManager);

        // Call tracker.
        WebApkUninstallTracker.deferRecordWebApkUninstalled(WEBAPK_PACKAGE);

        // Verify sync is called.
        verify(mMockWebApkSyncServiceJni).onWebApkUninstalled(MANIFEST_ID);

        // Verify AppBannerManager JNI recheck is called.
        verify(mAppBannerManager).recheckInstallability();
    }
}

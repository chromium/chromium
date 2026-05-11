// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.content.ComponentName;
import android.content.Intent;

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
    @Mock private BrowserServicesIntentDataProvider mIntentDataProvider;
    @Mock private Tab mTab;
    @Mock private TabModelSelector mTabModelSelector;
    @Mock private TabModel mTabModel;
    @Mock private WindowAndroid mWindowAndroid;

    private CustomTabDelegateFactory mFactory;

    @Before
    public void setUp() {
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
                        ActivityType.WEB_APK,
                        () -> mock(BottomSheetController.class),
                        mock(AuthTabVerifier.class),
                        mock(BrowserControlsManager.class),
                        SupplierUtils.of(false),
                        SupplierUtils.of(false),
                        mock(ExclusiveAccessManager.class),
                        mock(DesktopWindowStateManager.class));

        // Common mocks for activateContents() execution.
        when(mTab.isIncognito()).thenReturn(false);
        when(mTab.isIncognitoBranded()).thenReturn(false);
        when(mTab.isInitialized()).thenReturn(true);
        when(mTabModelSelector.getModel(false)).thenReturn(mTabModel);
        when(mTabModel.indexOf(mTab)).thenReturn(0);
        when(mTab.getWindowAndroid()).thenReturn(mWindowAndroid);
        when(mWindowAndroid.isActivityTopResumedSupported()).thenReturn(false);
        when(mActivity.isInMultiWindowMode()).thenReturn(true);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.USE_APP_TASK_FOR_CUSTOM_TAB_ACTIVATION)
    public void testBringActivityToForeground_WebApk() {
        // Mock WebAPK configurations.
        when(mIntentDataProvider.getActivityType()).thenReturn(ActivityType.WEB_APK);
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

        Assert.assertTrue(intent.getBooleanExtra(WebApkConstants.EXTRA_BRING_TO_FRONT, false));
        Assert.assertEquals(
                Intent.FLAG_ACTIVITY_NEW_TASK, intent.getFlags() & Intent.FLAG_ACTIVITY_NEW_TASK);
        Assert.assertEquals(
                Intent.FLAG_ACTIVITY_EXCLUDE_FROM_RECENTS,
                intent.getFlags() & Intent.FLAG_ACTIVITY_EXCLUDE_FROM_RECENTS);
    }
}

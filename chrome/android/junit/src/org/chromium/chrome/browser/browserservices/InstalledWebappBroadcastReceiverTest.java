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
import static org.mockito.Mockito.verify;

import android.content.Context;
import android.content.Intent;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.browserservices.permissiondelegation.InstalledWebappPermissionStore;
import org.chromium.chrome.browser.notifications.channels.SiteChannelsManager;
import org.chromium.chrome.browser.webapps.WebappRegistry;
import org.chromium.components.embedder_support.util.Origin;
import org.chromium.url.GURL;

import java.util.Arrays;
import java.util.HashSet;
import java.util.Set;

/** Tests for {@link InstalledWebappBroadcastReceiver}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class InstalledWebappBroadcastReceiverTest {
    @Mock public Context mContext;
    @Mock public InstalledWebappBroadcastReceiver.ClearDataStrategy mMockStrategy;
    @Mock public InstalledWebappPermissionStore mStore;
    @Mock public SiteChannelsManager mSiteChannelsManager;

    private InstalledWebappBroadcastReceiver mReceiver;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        WebappRegistry.getInstance().setPermissionStoreForTesting(mStore);
        SiteChannelsManager.setInstanceForTesting(mSiteChannelsManager);

        mReceiver = new InstalledWebappBroadcastReceiver(mMockStrategy);
        mContext = RuntimeEnvironment.application;
    }

    private Intent createMockIntent(int id, String action) {
        Intent intent = new Intent();
        intent.putExtra(Intent.EXTRA_UID, id);
        intent.setAction(action);
        return intent;
    }

    private void addToRegister(int id, String appName, Set<GURL> urls) {
        for (GURL gurl : urls) {
            InstalledWebappDataRegister.registerPackageForOrigin(
                    id, appName, "com.package", gurl.getHost(), Origin.create(gurl.getSpec()));
        }
    }

    /** Makes sure we don't show a notification if we don't have any data for the app. */
    @Test
    @Feature("TrustedWebActivities")
    public void chromeHoldsNoData() {
        mReceiver.onReceive(mContext, createMockIntent(12, Intent.ACTION_PACKAGE_FULLY_REMOVED));

        verify(mMockStrategy, never()).execute(any(), anyInt(), anyBoolean());
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

        verify(mMockStrategy).execute(any(), eq(id), eq(true));
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

        verify(mStore).resetPermission(eq(Origin.create(url1.getSpec())), anyInt());
        verify(mStore).resetPermission(eq(Origin.create(url2.getSpec())), anyInt());
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
        verify(mMockStrategy).execute(any(), eq(id), eq(false));
    }
}

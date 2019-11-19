// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
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
import org.chromium.chrome.browser.browserservices.permissiondelegation.NotificationPermissionUpdater;
import org.chromium.chrome.browser.preferences.ChromePreferenceManager;

import java.util.Arrays;
import java.util.HashSet;
import java.util.Set;

/**
 * Tests for {@link ClientAppBroadcastReceiver}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ClientAppBroadcastReceiverTest {
    @Mock public Context mContext;
    @Mock public ClientAppDataRegister mDataRegister;
    @Mock public ClientAppBroadcastReceiver.ClearDataStrategy mMockStrategy;
    @Mock public NotificationPermissionUpdater mPermissionUpdater;

    private ClientAppBroadcastReceiver mReceiver;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        mReceiver = new ClientAppBroadcastReceiver(mMockStrategy, mDataRegister,
                mock(ChromePreferenceManager.class), mPermissionUpdater);
        mContext = RuntimeEnvironment.application;
    }

    private Intent createMockIntent(int id, String action) {
        Intent intent = new Intent();
        intent.putExtra(Intent.EXTRA_UID, id);
        intent.setAction(action);
        return intent;
    }

    private void addToRegister(int id, String appName, Set<String> domainAndRegistries,
            Set<String> origins) {
        doReturn(true).when(mDataRegister).chromeHoldsDataForPackage(eq(id));
        doReturn(appName).when(mDataRegister).getAppNameForRegisteredUid(eq(id));
        doReturn(domainAndRegistries)
                .when(mDataRegister)
                .getDomainsForRegisteredUid(eq(id));

        if (origins == null) return;
        doReturn(origins)
                .when(mDataRegister)
                .getOriginsForRegisteredUid(eq(id));
    }

    private void addToRegister(int id, String appName, Set<String> domainAndRegistries) {
        addToRegister(id, appName, domainAndRegistries, null);
    }

    /** Makes sure we don't show a notification if we don't have any data for the app. */
    @Test
    @Feature("TrustedWebActivities")
    public void chromeHoldsNoData() {
        mReceiver.onReceive(mContext, createMockIntent(12, Intent.ACTION_PACKAGE_FULLY_REMOVED));

        verify(mMockStrategy, never()).execute(any(), any(), any(), anyInt(), anyBoolean());
    }

    /** Tests the basic flow. */
    @Test
    @Feature("TrustedWebActivities")
    public void chromeHoldsData() {
        int id = 23;
        String appName = "App Name";
        String domain = "example.com";
        Set<String> domains = new HashSet<>(Arrays.asList(domain));

        addToRegister(id, appName, domains);

        mReceiver.onReceive(mContext, createMockIntent(id, Intent.ACTION_PACKAGE_FULLY_REMOVED));

        verify(mMockStrategy).execute(any(), any(), any(), eq(id), eq(true));
    }

    /** Tests we plumb the correct information to the {@link ClearDataDialogActivity}. */
    @Test
    @Feature("TrustedWebActivities")
    public void execute_ValidIntent() {
        mReceiver = new ClientAppBroadcastReceiver(
                new ClientAppBroadcastReceiver.ClearDataStrategy(), mDataRegister,
                mock(ChromePreferenceManager.class), mPermissionUpdater);

        int id = 67;
        String appName = "App Name 3";
        Set<String> domains = new HashSet<>(Arrays.asList("example.com", "example2.com"));

        addToRegister(id, appName, domains);

        Context context = mock(Context.class);

        mReceiver.onReceive(context, createMockIntent(id, Intent.ACTION_PACKAGE_FULLY_REMOVED));

        ArgumentCaptor<Intent> intentArgumentCaptor = ArgumentCaptor.forClass(Intent.class);
        verify(context).startActivity(intentArgumentCaptor.capture());

        Intent intent = intentArgumentCaptor.getValue();

        assertEquals(appName, ClearDataDialogActivity.getAppNameFromIntent(intent));
        assertTrue(ClearDataDialogActivity.getIsAppUninstalledFromIntent(intent));
        assertEquals(domains, new HashSet<>(ClearDataDialogActivity.getDomainsFromIntent(intent)));
    }

    /** Tests we call the NotificationPermissionUpdater. */
    @Test
    @Feature("TrustedwebActivities")
    public void execute_UpdatePermissions() {
        mReceiver = new ClientAppBroadcastReceiver(
                new ClientAppBroadcastReceiver.ClearDataStrategy(), mDataRegister,
                mock(ChromePreferenceManager.class), mPermissionUpdater);

        int id = 67;
        String appName = "App Name 3";
        Set<String> domains = new HashSet<>(Arrays.asList("example.com", "example2.com"));

        Origin origin1 = Origin.create("https://www.example.com");
        Origin origin2 = Origin.create("https://www.example2.com");
        Set<String> origins = new HashSet<>(Arrays.asList(origin1.toString(), origin2.toString()));

        addToRegister(id, appName, domains, origins);

        mReceiver.onReceive(mContext, createMockIntent(id, Intent.ACTION_PACKAGE_FULLY_REMOVED));

        verify(mPermissionUpdater).onClientAppUninstalled(origin1);
        verify(mPermissionUpdater).onClientAppUninstalled(origin2);
    }

    /** Tests we differentiate between app uninstalled and data cleared. */
    @Test
    @Feature("TrustedWebActivities")
    public void onDataClear() {
        int id = 23;
        String appName = "App Name";
        String domain = "example.com";
        Set<String> domains = new HashSet<>(Arrays.asList(domain));

        addToRegister(id, appName, domains);

        mReceiver.onReceive(mContext, createMockIntent(id, Intent.ACTION_PACKAGE_DATA_CLEARED));
        verify(mMockStrategy).execute(any(), any(), any(), eq(id), eq(false));
    }
}

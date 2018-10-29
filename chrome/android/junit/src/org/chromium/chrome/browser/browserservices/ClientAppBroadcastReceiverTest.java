// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import android.content.Context;
import android.content.Intent;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;

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
    @Mock public ClearDataNotificationPublisher mNotificationManager;

    private ClientAppBroadcastReceiver mReceiver;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        mReceiver = new ClientAppBroadcastReceiver(mNotificationManager, mDataRegister);
        mContext = RuntimeEnvironment.application;
    }

    private Intent createMockIntent(int id, String action) {
        Intent intent = new Intent();
        intent.putExtra(Intent.EXTRA_UID, id);
        intent.setAction(action);
        return intent;
    }

    private void addToRegister(int id, String appName, Set<String> domainAndRegistries) {
        doReturn(true).when(mDataRegister).chromeHoldsDataForPackage(eq(id));
        doReturn(appName).when(mDataRegister).getAppNameForRegisteredUid(eq(id));
        doReturn(domainAndRegistries)
                .when(mDataRegister)
                .getDomainsForRegisteredUid(eq(id));
    }

    /** Makes sure we don't show a notification if we don't have any data for the app. */
    @Test
    @Feature("TrustedWebActivities")
    public void chromeHoldsNoData() {
        mReceiver.onReceive(mContext, createMockIntent(12, Intent.ACTION_PACKAGE_FULLY_REMOVED));

        verify(mNotificationManager, never())
                .showClearDataNotification(any(), anyString(), anyString(), anyBoolean());
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
        verify(mNotificationManager)
                .showClearDataNotification(any(), eq(appName), eq(domain), eq(true));
    }

    /** Tests we deal with multiple domains well. */
    @Test
    @Feature("TrustedWebActivities")
    public void multipleDomains() {
        int id = 45;
        String appName = "App Name 2";
        String domain1 = "example.com";
        String domain2 = "example2.com";
        Set<String> domains = new HashSet<>(Arrays.asList(domain1, domain2));

        addToRegister(id, appName, domains);

        mReceiver.onReceive(mContext, createMockIntent(id, Intent.ACTION_PACKAGE_FULLY_REMOVED));

        verify(mNotificationManager).showClearDataNotification(
                any(), eq(appName), eq(domain1), eq(true));
        verify(mNotificationManager).showClearDataNotification(
                any(), eq(appName), eq(domain2), eq(true));
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
        verify(mNotificationManager)
                .showClearDataNotification(any(), eq(appName), eq(domain), eq(false));
    }
}

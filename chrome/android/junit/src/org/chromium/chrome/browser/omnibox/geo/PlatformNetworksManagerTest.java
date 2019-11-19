// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.geo;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.argThat;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.Manifest;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.pm.PackageManager;
import android.net.ConnectivityManager;
import android.net.NetworkInfo;
import android.net.wifi.ScanResult;
import android.net.wifi.WifiInfo;
import android.net.wifi.WifiManager;
import android.os.Build;
import android.telephony.CellIdentityCdma;
import android.telephony.CellIdentityGsm;
import android.telephony.CellIdentityLte;
import android.telephony.CellIdentityWcdma;
import android.telephony.CellInfoCdma;
import android.telephony.CellInfoGsm;
import android.telephony.CellInfoLte;
import android.telephony.CellInfoWcdma;
import android.telephony.TelephonyManager;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.ArgumentMatcher;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;
import org.robolectric.util.ReflectionHelpers;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.omnibox.geo.VisibleNetworks.VisibleCell;
import org.chromium.chrome.browser.omnibox.geo.VisibleNetworks.VisibleCell.RadioType;
import org.chromium.chrome.browser.omnibox.geo.VisibleNetworks.VisibleWifi;

import java.util.Arrays;
import java.util.Collections;
import java.util.HashSet;
import java.util.Set;
import java.util.concurrent.TimeUnit;

/**
 * Robolectric tests for {@link PlatformNetworksManager}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class PlatformNetworksManagerTest {
    private static final VisibleWifi CONNECTED_WIFI =
            VisibleWifi.create("ssid1", "11:11:11:11:11:11", -1, 10L);
    private static final VisibleWifi NOT_CONNECTED_WIFI =
            VisibleWifi.create("ssid2", "11:11:11:11:11:12", -2, 20L);
    private static final long CONNECTED_WIFI_AGE = 1000L;
    private static final long NOT_CONNECTED_WIFI_AGE = 2000L;

    private static final VisibleCell CDMA_CELL = VisibleCell.builder(RadioType.CDMA)
                                                         .setCellId(40)
                                                         .setLocationAreaCode(41)
                                                         .setMobileNetworkCode(43)
                                                         .setTimestamp(47L)
                                                         .build();
    private static final VisibleCell LTE_CELL = VisibleCell.builder(RadioType.LTE)
                                                        .setCellId(50)
                                                        .setMobileCountryCode(52)
                                                        .setMobileNetworkCode(53)
                                                        .setPhysicalCellId(55)
                                                        .setTrackingAreaCode(56)
                                                        .setTimestamp(57L)
                                                        .build();
    private static final VisibleCell WCDMA_CELL = VisibleCell.builder(RadioType.WCDMA)
                                                          .setCellId(60)
                                                          .setLocationAreaCode(61)
                                                          .setMobileCountryCode(62)
                                                          .setMobileNetworkCode(63)
                                                          .setPrimaryScramblingCode(64)
                                                          .setTimestamp(67L)
                                                          .build();
    private static final VisibleCell GSM_CELL = VisibleCell.builder(RadioType.GSM)
                                                        .setCellId(70)
                                                        .setLocationAreaCode(71)
                                                        .setMobileCountryCode(72)
                                                        .setMobileNetworkCode(73)
                                                        .setTimestamp(77L)
                                                        .build();
    private static final VisibleCell UNKNOWN_VISIBLE_CELL =
            VisibleCell.builder(RadioType.UNKNOWN).build();
    private static final VisibleCell UNKNOWN_MISSING_LOCATION_PERMISSION_VISIBLE_CELL =
            VisibleCell.builder(RadioType.UNKNOWN_MISSING_LOCATION_PERMISSION).build();
    private static final VisibleWifi UNKNOWN_VISIBLE_WIFI =
            VisibleWifi.create(null, null, null, null);
    private static final long LTE_CELL_AGE = 3000L;
    private static final long WCDMA_CELL_AGE = 4000L;
    private static final long GSM_CELL_AGE = 5000L;
    private static final long CDMA_CELL_AGE = 6000L;

    private static final long CURRENT_TIME_MS = 90000000L;
    private static final long CURRENT_ELAPSED_TIME_MS = 7000L;

    @Mock
    private Context mContext;
    @Mock
    private TelephonyManager mTelephonyManager;
    @Mock
    private WifiManager mWifiManager;
    @Mock
    private NetworkInfo mCellNetworkInfo;
    @Mock
    private NetworkInfo mWifiNetworkInfo;
    @Mock
    private NetworkInfo mEthernetNetworkInfo;
    @Mock
    private WifiInfo mWifiInfo;
    @Mock
    private ScanResult mWifiScanResult;
    @Mock
    private ScanResult mWifiScanResultNotConnected;
    @Mock
    private ScanResult mWifiScanResultUnknown;
    @Mock
    private CellInfoLte mCellInfoLte;
    @Mock
    private CellInfoWcdma mCellInfoWcdma;
    @Mock
    private CellInfoGsm mCellInfoGsm;
    @Mock
    private CellInfoCdma mCellInfoCdma;
    @Mock
    private CellIdentityLte mCellIdentityLte;
    @Mock
    private CellIdentityWcdma mCellIdentityWcdma;
    @Mock
    private CellIdentityGsm mCellIdentityGsm;
    @Mock
    private CellIdentityCdma mCellIdentityCdma;
    @Mock
    private Intent mNetworkStateChangedIntent;
    @Mock
    private Callback<Set<VisibleCell>> mVisibleCellCallback;
    @Captor
    ArgumentCaptor<Set<VisibleCell>> mVisibleCellsArgument;
    @Mock
    private Callback<VisibleNetworks> mVisibleNetworksCallback;
    @Captor
    ArgumentCaptor<VisibleNetworks> mVisibleNetworksArgument;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        PlatformNetworksManager.sTimeProvider = new PlatformNetworksManager.TimeProvider() {
            @Override
            public long getCurrentTime() {
                return CURRENT_TIME_MS;
            }
            @Override
            public long getElapsedRealtime() {
                return CURRENT_ELAPSED_TIME_MS;
            }
        };
        when(mContext.getSystemService(Context.TELEPHONY_SERVICE)).thenReturn(mTelephonyManager);
        when(mContext.getSystemService(Context.WIFI_SERVICE)).thenReturn(mWifiManager);
        when(mContext.getApplicationContext()).thenReturn(mContext);
        when(mCellNetworkInfo.getType()).thenReturn(ConnectivityManager.TYPE_MOBILE);
        when(mWifiNetworkInfo.getType()).thenReturn(ConnectivityManager.TYPE_WIFI);
        when(mEthernetNetworkInfo.getType()).thenReturn(ConnectivityManager.TYPE_ETHERNET);
        // Add double quotation, since getSSID would add it.
        when(mWifiInfo.getSSID()).thenReturn("\"" + CONNECTED_WIFI.ssid() + "\"");
        when(mWifiInfo.getBSSID()).thenReturn(CONNECTED_WIFI.bssid());
        when(mWifiManager.getConnectionInfo()).thenReturn(mWifiInfo);

        mWifiScanResult.SSID = CONNECTED_WIFI.ssid();
        mWifiScanResult.BSSID = CONNECTED_WIFI.bssid();
        mWifiScanResult.level = CONNECTED_WIFI.level();
        mWifiScanResult.timestamp =
                TimeUnit.MILLISECONDS.toMicros(CURRENT_ELAPSED_TIME_MS - CONNECTED_WIFI_AGE);
        mWifiScanResultNotConnected.SSID = NOT_CONNECTED_WIFI.ssid();
        mWifiScanResultNotConnected.BSSID = NOT_CONNECTED_WIFI.bssid();
        mWifiScanResultNotConnected.level = NOT_CONNECTED_WIFI.level();
        mWifiScanResultNotConnected.timestamp =
                TimeUnit.MILLISECONDS.toMicros(CURRENT_ELAPSED_TIME_MS - NOT_CONNECTED_WIFI_AGE);
        mWifiScanResultUnknown.SSID = "any_value";
        mWifiScanResultUnknown.BSSID = null; // This is what makes it unknown.
        mWifiScanResultUnknown.level = 0;
        when(mWifiManager.getScanResults())
                .thenReturn(Arrays.asList(
                        mWifiScanResult, mWifiScanResultNotConnected, mWifiScanResultUnknown));

        when(mCellIdentityLte.getCi()).thenReturn(LTE_CELL.cellId().intValue());
        when(mCellIdentityLte.getPci()).thenReturn(LTE_CELL.physicalCellId().intValue());
        when(mCellIdentityLte.getTac()).thenReturn(LTE_CELL.trackingAreaCode().intValue());
        when(mCellIdentityLte.getMcc()).thenReturn(LTE_CELL.mobileCountryCode().intValue());
        when(mCellIdentityLte.getMnc()).thenReturn(LTE_CELL.mobileNetworkCode().intValue());
        when(mCellInfoLte.getCellIdentity()).thenReturn(mCellIdentityLte);
        when(mCellInfoLte.isRegistered()).thenReturn(true);
        when(mCellInfoLte.getTimeStamp())
                .thenReturn(TimeUnit.MILLISECONDS.toNanos(CURRENT_ELAPSED_TIME_MS - LTE_CELL_AGE));
        when(mCellIdentityWcdma.getCid()).thenReturn(WCDMA_CELL.cellId().intValue());
        when(mCellIdentityWcdma.getLac()).thenReturn(WCDMA_CELL.locationAreaCode().intValue());
        when(mCellIdentityWcdma.getMcc()).thenReturn(WCDMA_CELL.mobileCountryCode().intValue());
        when(mCellIdentityWcdma.getMnc()).thenReturn(WCDMA_CELL.mobileNetworkCode().intValue());
        when(mCellIdentityWcdma.getPsc()).thenReturn(WCDMA_CELL.primaryScramblingCode().intValue());
        when(mCellInfoWcdma.getCellIdentity()).thenReturn(mCellIdentityWcdma);
        when(mCellInfoWcdma.isRegistered()).thenReturn(false);
        when(mCellInfoWcdma.getTimeStamp())
                .thenReturn(
                        TimeUnit.MILLISECONDS.toNanos(CURRENT_ELAPSED_TIME_MS - WCDMA_CELL_AGE));
        when(mCellIdentityGsm.getCid()).thenReturn(GSM_CELL.cellId().intValue());
        when(mCellIdentityGsm.getLac()).thenReturn(GSM_CELL.locationAreaCode().intValue());
        when(mCellIdentityGsm.getMcc()).thenReturn(GSM_CELL.mobileCountryCode().intValue());
        when(mCellIdentityGsm.getMnc()).thenReturn(GSM_CELL.mobileNetworkCode().intValue());
        when(mCellInfoGsm.getCellIdentity()).thenReturn(mCellIdentityGsm);
        when(mCellInfoGsm.isRegistered()).thenReturn(false);
        when(mCellInfoGsm.getTimeStamp())
                .thenReturn(TimeUnit.MILLISECONDS.toNanos(CURRENT_ELAPSED_TIME_MS - GSM_CELL_AGE));
        when(mCellIdentityCdma.getBasestationId()).thenReturn(CDMA_CELL.cellId().intValue());
        when(mCellIdentityCdma.getNetworkId()).thenReturn(CDMA_CELL.locationAreaCode().intValue());
        when(mCellIdentityCdma.getSystemId()).thenReturn(CDMA_CELL.mobileNetworkCode().intValue());
        when(mCellInfoCdma.getCellIdentity()).thenReturn(mCellIdentityCdma);
        when(mCellInfoCdma.isRegistered()).thenReturn(false);
        when(mCellInfoCdma.getTimeStamp())
                .thenReturn(TimeUnit.MILLISECONDS.toNanos(CURRENT_ELAPSED_TIME_MS - CDMA_CELL_AGE));
        when(mTelephonyManager.getAllCellInfo())
                .thenReturn(
                        Arrays.asList(mCellInfoLte, mCellInfoWcdma, mCellInfoGsm, mCellInfoCdma));
        allPermissionsGranted();

        when(mContext.registerReceiver(eq(null), any(IntentFilter.class)))
                .thenReturn(mNetworkStateChangedIntent);
        when(mNetworkStateChangedIntent.getParcelableExtra(eq(WifiManager.EXTRA_WIFI_INFO)))
                .thenReturn(mWifiInfo);
    }

    @Test
    public void testGetConnectedCell_allPermissionsDenied() {
        ReflectionHelpers.setStaticField(Build.VERSION.class, "SDK_INT", Build.VERSION_CODES.M);
        allPermissionsDenied();
        VisibleCell visibleCell =
                PlatformNetworksManager.getConnectedCell(mContext, mTelephonyManager);
        assertEquals(UNKNOWN_MISSING_LOCATION_PERMISSION_VISIBLE_CELL, visibleCell);
        assertNull(visibleCell.timestampMs());
    }

    @Test
    public void testGetAllVisibleCells_Q() {
        // TODO(crbug.com/954620): Add test once Q is supported by Robolectric.
    }

    @Test
    public void testGetAllVisibleCells_JBMR2() {
        ReflectionHelpers.setStaticField(
                Build.VERSION.class, "SDK_INT", Build.VERSION_CODES.JELLY_BEAN_MR2);
        PlatformNetworksManager.getAllVisibleCells(
                mContext, mTelephonyManager, mVisibleCellCallback);
        verify(mVisibleCellCallback).onResult(mVisibleCellsArgument.capture());

        assertEquals(4, mVisibleCellsArgument.getValue().size());
        for (VisibleCell visibleCell : mVisibleCellsArgument.getValue()) {
            switch (visibleCell.radioType()) {
                case RadioType.LTE:
                    assertEquals(LTE_CELL, visibleCell);
                    assertEquals(Long.valueOf(CURRENT_TIME_MS - LTE_CELL_AGE),
                            visibleCell.timestampMs());
                    break;
                case RadioType.WCDMA:
                    assertEquals(visibleCell, WCDMA_CELL);
                    assertEquals(Long.valueOf(CURRENT_TIME_MS - WCDMA_CELL_AGE),
                            visibleCell.timestampMs());
                    break;
                case RadioType.GSM:
                    assertEquals(visibleCell, GSM_CELL);
                    assertEquals(Long.valueOf(CURRENT_TIME_MS - GSM_CELL_AGE),
                            visibleCell.timestampMs());
                    break;
                case RadioType.CDMA:
                    assertEquals(visibleCell, CDMA_CELL);
                    assertEquals(Long.valueOf(CURRENT_TIME_MS - CDMA_CELL_AGE),
                            visibleCell.timestampMs());
                    break;
                default:
                    break;
            }
        }
    }

    @Test
    public void testGetAllVisibleCells_allPermissionsDenied() {
        ReflectionHelpers.setStaticField(Build.VERSION.class, "SDK_INT", Build.VERSION_CODES.M);
        allPermissionsDenied();
        PlatformNetworksManager.getAllVisibleCells(
                mContext, mTelephonyManager, mVisibleCellCallback);
        verify(mVisibleCellCallback).onResult(mVisibleCellsArgument.capture());

        // Empty set expected
        assertEquals(0, mVisibleCellsArgument.getValue().size());
    }

    @Test
    public void testGetConnectedWifi() {
        VisibleWifi visibleWifi = PlatformNetworksManager.getConnectedWifi(mContext, mWifiManager);
        assertEquals(CONNECTED_WIFI, visibleWifi);
        // When we get it through get connected wifi, we should see the current time.
        assertEquals(Long.valueOf(CURRENT_TIME_MS), visibleWifi.timestampMs());
    }

    @Test
    public void testGetConnectedWifi_allPermissionsDenied() {
        ReflectionHelpers.setStaticField(Build.VERSION.class, "SDK_INT", Build.VERSION_CODES.M);
        allPermissionsDenied();
        VisibleWifi visibleWifi = PlatformNetworksManager.getConnectedWifi(mContext, mWifiManager);
        assertEquals(UNKNOWN_VISIBLE_WIFI, visibleWifi);
        assertNull(visibleWifi.timestampMs());
    }

    @Test
    public void testGetConnectedWifi_locationGrantedWifiDenied() {
        ReflectionHelpers.setStaticField(
                Build.VERSION.class, "SDK_INT", Build.VERSION_CODES.LOLLIPOP);
        locationGrantedWifiDenied();
        VisibleWifi visibleWifi = PlatformNetworksManager.getConnectedWifi(mContext, mWifiManager);
        assertEquals(CONNECTED_WIFI, visibleWifi);
        assertEquals(Long.valueOf(CURRENT_TIME_MS), visibleWifi.timestampMs());
        verifyNetworkStateAction();
    }

    @Test
    public void testGetConnectedWifi_locationGrantedWifiDenied_noWifiInfo() {
        ReflectionHelpers.setStaticField(
                Build.VERSION.class, "SDK_INT", Build.VERSION_CODES.LOLLIPOP);
        locationGrantedWifiDenied();
        when(mNetworkStateChangedIntent.getParcelableExtra(eq(WifiManager.EXTRA_WIFI_INFO)))
                .thenReturn(null);
        VisibleWifi visibleWifi = PlatformNetworksManager.getConnectedWifi(mContext, mWifiManager);
        assertEquals(UNKNOWN_VISIBLE_WIFI, visibleWifi);
        assertNull(visibleWifi.timestampMs());
        verifyNetworkStateAction();
    }

    @Test
    public void testGetConnectedWifi_locationDeniedWifiGranted() {
        ReflectionHelpers.setStaticField(Build.VERSION.class, "SDK_INT", Build.VERSION_CODES.M);
        locationDeniedWifiGranted();
        VisibleWifi visibleWifi = PlatformNetworksManager.getConnectedWifi(mContext, mWifiManager);
        assertEquals(UNKNOWN_VISIBLE_WIFI, visibleWifi);
        assertNull(visibleWifi.timestampMs());
    }

    @Test
    public void testGetAllVisibleWifis() {
        Set<VisibleWifi> visibleWifis =
                PlatformNetworksManager.getAllVisibleWifis(mContext, mWifiManager);
        assertEquals(2, visibleWifis.size());
        // When we get all wifis, we should get the scan time.
        for (VisibleWifi visibleWifi : visibleWifis) {
            if (visibleWifi.bssid().equals(CONNECTED_WIFI.bssid())) {
                assertEquals(CONNECTED_WIFI, visibleWifi);
                assertEquals(Long.valueOf(CURRENT_TIME_MS - CONNECTED_WIFI_AGE),
                        visibleWifi.timestampMs());
            } else {
                assertEquals(NOT_CONNECTED_WIFI, visibleWifi);
                assertEquals(Long.valueOf(CURRENT_TIME_MS - NOT_CONNECTED_WIFI_AGE),
                        visibleWifi.timestampMs());
            }
        }
    }

    @Test
    public void testGetAllVisibleWifis_allPermissionsDenied() {
        ReflectionHelpers.setStaticField(Build.VERSION.class, "SDK_INT", Build.VERSION_CODES.M);
        allPermissionsDenied();
        Set<VisibleWifi> visibleWifis =
                PlatformNetworksManager.getAllVisibleWifis(mContext, mWifiManager);
        // Empty set expected
        assertTrue(visibleWifis.isEmpty());
    }

    @Test
    public void testGetAllVisibleWifis_locationGrantedWifiDenied() {
        ReflectionHelpers.setStaticField(Build.VERSION.class, "SDK_INT", Build.VERSION_CODES.M);
        locationGrantedWifiDenied();
        Set<VisibleWifi> visibleWifis =
                PlatformNetworksManager.getAllVisibleWifis(mContext, mWifiManager);
        // Empty set expected
        assertTrue(visibleWifis.isEmpty());
    }

    @Test
    public void testGetAllVisibleWifis_locationDeniedWifiGranted() {
        ReflectionHelpers.setStaticField(Build.VERSION.class, "SDK_INT", Build.VERSION_CODES.M);
        locationDeniedWifiGranted();
        Set<VisibleWifi> visibleWifis =
                PlatformNetworksManager.getAllVisibleWifis(mContext, mWifiManager);
        // Empty set expected
        assertTrue(visibleWifis.isEmpty());
    }

    @Test
    public void testComputeVisibleNetworks_withoutNonConnectedNetworks() {
        VisibleNetworks expectedVisibleNetworks =
                VisibleNetworks.create(CONNECTED_WIFI, LTE_CELL, null, null);
        VisibleNetworks visibleNetworks =
                PlatformNetworksManager.computeConnectedNetworks(mContext);
        assertEquals(expectedVisibleNetworks, visibleNetworks);
    }

    @Test
    public void testComputeVisibleNetworks_withNonConnectedNetworks() {
        Set<VisibleCell> expectedVisibleCells =
                new HashSet<VisibleCell>(Arrays.asList(LTE_CELL, WCDMA_CELL, GSM_CELL, CDMA_CELL));
        Set<VisibleWifi> expectedVisibleWifis =
                new HashSet<VisibleWifi>(Arrays.asList(CONNECTED_WIFI, NOT_CONNECTED_WIFI));
        VisibleNetworks expectedVisibleNetworks = VisibleNetworks.create(
                CONNECTED_WIFI, LTE_CELL, expectedVisibleWifis, expectedVisibleCells);
        PlatformNetworksManager.computeVisibleNetworks(mContext, mVisibleNetworksCallback);
        verify(mVisibleNetworksCallback).onResult(mVisibleNetworksArgument.capture());
        assertEquals(expectedVisibleNetworks, mVisibleNetworksArgument.getValue());
    }

    @Test
    public void testComputeVisibleNetworks_allPermissionsDenied() {
        allPermissionsDenied();

        PlatformNetworksManager.computeVisibleNetworks(mContext, mVisibleNetworksCallback);
        verify(mVisibleNetworksCallback).onResult(mVisibleNetworksArgument.capture());

        assertTrue(mVisibleNetworksArgument.getValue().isEmpty());
    }

    @Test
    public void testComputeVisibleNetworks_locationGrantedWifiDenied() {
        Set<VisibleCell> expectedVisibleCells =
                new HashSet<VisibleCell>(Arrays.asList(LTE_CELL, WCDMA_CELL, GSM_CELL, CDMA_CELL));
        Set<VisibleWifi> expectedVisibleWifis = Collections.emptySet();
        VisibleNetworks expectedVisibleNetworks = VisibleNetworks.create(
                CONNECTED_WIFI, LTE_CELL, expectedVisibleWifis, expectedVisibleCells);
        locationGrantedWifiDenied();

        PlatformNetworksManager.computeVisibleNetworks(mContext, mVisibleNetworksCallback);
        verify(mVisibleNetworksCallback).onResult(mVisibleNetworksArgument.capture());

        assertEquals(expectedVisibleNetworks, mVisibleNetworksArgument.getValue());
        verifyNetworkStateAction();
    }

    @Test
    public void testComputeVisibleNetworks_locationDeniedWifiGranted() {
        Set<VisibleCell> expectedVisibleCells = Collections.emptySet();
        Set<VisibleWifi> expectedVisibleWifis = Collections.emptySet();
        locationDeniedWifiGranted();
        VisibleNetworks expectedVisibleNetworks =
                VisibleNetworks.create(null, null, expectedVisibleWifis, expectedVisibleCells);

        PlatformNetworksManager.computeVisibleNetworks(mContext, mVisibleNetworksCallback);
        verify(mVisibleNetworksCallback).onResult(mVisibleNetworksArgument.capture());

        assertEquals(expectedVisibleNetworks, mVisibleNetworksArgument.getValue());
    }

    private void allPermissionsGranted() {
        setPermissions(true, true, true);
    }

    private void allPermissionsDenied() {
        setPermissions(false, false, false);
    }

    private void locationGrantedWifiDenied() {
        setPermissions(true, true, false);
    }

    private void locationDeniedWifiGranted() {
        setPermissions(false, false, true);
    }

    private void setPermissions(
            boolean coarseLocationGranted, boolean fineLocationGranted, boolean wifiStateGranted) {
        int coarseLocationPermission = (coarseLocationGranted) ? PackageManager.PERMISSION_GRANTED
                                                               : PackageManager.PERMISSION_DENIED;
        int fineLocationPermission = (fineLocationGranted) ? PackageManager.PERMISSION_GRANTED
                                                           : PackageManager.PERMISSION_DENIED;
        int wifiStatePermission = (wifiStateGranted) ? PackageManager.PERMISSION_GRANTED
                                                     : PackageManager.PERMISSION_DENIED;

        when(mContext.checkPermission(eq(Manifest.permission.ACCESS_COARSE_LOCATION),
                     any(Integer.class), any(Integer.class)))
                .thenReturn(coarseLocationPermission);
        when(mContext.checkPermission(eq(Manifest.permission.ACCESS_FINE_LOCATION),
                     any(Integer.class), any(Integer.class)))
                .thenReturn(fineLocationPermission);
        when(mContext.checkPermission(eq(Manifest.permission.ACCESS_WIFI_STATE), any(Integer.class),
                     any(Integer.class)))
                .thenReturn(wifiStatePermission);
    }

    private void verifyNetworkStateAction() {
        verify(mContext).registerReceiver(eq(null), argThat(new ArgumentMatcher<IntentFilter>() {
            @Override
            public boolean matches(IntentFilter intentFilter) {
                return intentFilter.hasAction(WifiManager.NETWORK_STATE_CHANGED_ACTION);
            }
        }));
    }
}

// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.geo;

import android.Manifest;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.pm.PackageManager;
import android.net.wifi.ScanResult;
import android.net.wifi.WifiInfo;
import android.net.wifi.WifiManager;
import android.os.Process;
import android.os.SystemClock;
import android.telephony.CellIdentityCdma;
import android.telephony.CellIdentityGsm;
import android.telephony.CellIdentityLte;
import android.telephony.CellIdentityWcdma;
import android.telephony.CellInfo;
import android.telephony.CellInfoCdma;
import android.telephony.CellInfoGsm;
import android.telephony.CellInfoLte;
import android.telephony.CellInfoWcdma;
import android.telephony.TelephonyManager;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.BuildInfo;
import org.chromium.base.Callback;
import org.chromium.base.task.PostTask;
import org.chromium.chrome.browser.omnibox.geo.VisibleNetworks.VisibleCell;
import org.chromium.chrome.browser.omnibox.geo.VisibleNetworks.VisibleWifi;
import org.chromium.content_public.browser.UiThreadTaskTraits;

import java.util.Collections;
import java.util.HashSet;
import java.util.List;
import java.util.Set;
import java.util.concurrent.TimeUnit;

/**
 * Util methods for platform networking APIs.
 */
class PlatformNetworksManager {
    @VisibleForTesting
    static TimeProvider sTimeProvider = new TimeProvider();

    /**
     * Equivalent to WifiSsid.NONE which is hidden for some reason. This is returned by
     * {@link WifiManager} if it cannot get the ssid for the connected wifi access point.
     */
    static final String UNKNOWN_SSID = "<unknown ssid>";

    /**
     * Get the connected wifi, but do not use it (nullify it) if its BSSID is unknown.
     *
     * @param context The application context
     * @param wifiManager Provides access to wifi information on the device
     * @return The possibly null connected wifi
     */
    private static VisibleWifi getConnectedWifiIfKnown(Context context, WifiManager wifiManager) {
        VisibleWifi connectedWifi = getConnectedWifi(context, wifiManager);
        if (connectedWifi != null && connectedWifi.bssid() == null) {
            return null;
        }
        return connectedWifi;
    }

    static VisibleWifi getConnectedWifi(Context context, WifiManager wifiManager) {
        if (hasLocationAndWifiPermission(context)) {
            WifiInfo wifiInfo = wifiManager.getConnectionInfo();
            return connectedWifiInfoToVisibleWifi(wifiInfo);
        }
        if (hasLocationPermission(context)) {
            // Only location permission, so fallback to pre-marshmallow.
            return getConnectedWifiPreMarshmallow(context);
        }
        return VisibleWifi.NO_WIFI_INFO;
    }

    static VisibleWifi getConnectedWifiPreMarshmallow(Context context) {
        Intent intent = context.getApplicationContext().registerReceiver(
                null, new IntentFilter(WifiManager.NETWORK_STATE_CHANGED_ACTION));
        if (intent != null) {
            WifiInfo wifiInfo = intent.getParcelableExtra(WifiManager.EXTRA_WIFI_INFO);
            return connectedWifiInfoToVisibleWifi(wifiInfo);
        }
        return VisibleWifi.NO_WIFI_INFO;
    }

    private static VisibleWifi connectedWifiInfoToVisibleWifi(@Nullable WifiInfo wifiInfo) {
        if (wifiInfo == null) {
            return VisibleWifi.NO_WIFI_INFO;
        }
        String ssid = wifiInfo.getSSID();
        if (ssid == null || UNKNOWN_SSID.equals(ssid)) {
            // No SSID.
            ssid = null;
        } else {
            // Remove double quotation if ssid has double quotation.
            if (ssid.startsWith("\"") && ssid.endsWith("\"") && ssid.length() > 2) {
                ssid = ssid.substring(1, ssid.length() - 1);
            }
        }
        String bssid = wifiInfo.getBSSID();
        // It's connected, so use current time.
        return VisibleWifi.create(ssid, bssid, null, sTimeProvider.getCurrentTime());
    }

    static Set<VisibleWifi> getAllVisibleWifis(Context context, WifiManager wifiManager) {
        if (!hasLocationAndWifiPermission(context)) {
            return Collections.emptySet();
        }
        Set<VisibleWifi> visibleWifis = new HashSet<>();
        // Do not trigger a scan, but use current visible networks from latest scan.
        List<ScanResult> scanResults = wifiManager.getScanResults();
        if (scanResults == null) {
            return visibleWifis;
        }
        long elapsedTime = sTimeProvider.getElapsedRealtime();
        long currentTime = sTimeProvider.getCurrentTime();
        for (int i = 0; i < scanResults.size(); i++) {
            ScanResult scanResult = scanResults.get(i);
            String bssid = scanResult.BSSID;
            if (bssid == null) continue;
            long ageMs = elapsedTime - TimeUnit.MICROSECONDS.toMillis(scanResult.timestamp);
            long wifiTimestamp = currentTime - ageMs;
            visibleWifis.add(
                    VisibleWifi.create(scanResult.SSID, bssid, scanResult.level, wifiTimestamp));
        }
        return visibleWifis;
    }

    static void getAllVisibleCells(Context context, TelephonyManager telephonyManager,
            Callback<Set<VisibleCell>> callback) {
        if (!hasLocationPermission(context)) {
            callback.onResult(Collections.emptySet());
            return;
        }

        CellInfoDelegate.requestCellInfoUpdate(telephonyManager, (cellInfos) -> {
            PostTask.postTask(UiThreadTaskTraits.DEFAULT,
                    () -> callback.onResult(getAllVisibleCellsFromCellInfo(cellInfos)));
        });
    }

    private static Set<VisibleCell> getAllVisibleCellsFromCellInfo(List<CellInfo> cellInfos) {
        Set<VisibleCell> visibleCells = new HashSet<>();
        if (cellInfos == null) {
            return visibleCells;
        }

        long elapsedTime = sTimeProvider.getElapsedRealtime();
        long currentTime = sTimeProvider.getCurrentTime();
        for (int i = 0; i < cellInfos.size(); i++) {
            CellInfo cellInfo = cellInfos.get(i);
            VisibleCell visibleCell = getVisibleCell(cellInfo, elapsedTime, currentTime);
            if (visibleCell.radioType() != VisibleCell.RadioType.UNKNOWN) {
                visibleCells.add(visibleCell);
            }
        }
        return visibleCells;
    }

    /**
     * Get the connected cell network, but do not use it (nullify it) if its radio type is unknown.
     *
     * @param context The application context
     * @param telephonyManager Provides access to cell information on the device
     * @return The possibly null connected cell
     */
    private static VisibleCell getConnectedCellIfKnown(
            Context context, TelephonyManager telephonyManager) {
        VisibleCell connectedCell = getConnectedCell(context, telephonyManager);
        if (connectedCell != null
                && (connectedCell.radioType() == VisibleCell.RadioType.UNKNOWN
                        || connectedCell.radioType()
                                == VisibleCell.RadioType.UNKNOWN_MISSING_LOCATION_PERMISSION)) {
            return null;
        }
        return connectedCell;
    }

    static VisibleCell getConnectedCell(Context context, TelephonyManager telephonyManager) {
        if (!hasLocationPermission(context)) {
            return VisibleCell.UNKNOWN_MISSING_LOCATION_PERMISSION_VISIBLE_CELL;
        }
        CellInfo cellInfo = getActiveCellInfo(telephonyManager);
        return getVisibleCell(
                cellInfo, sTimeProvider.getElapsedRealtime(), sTimeProvider.getCurrentTime());
    }

    private static VisibleCell getVisibleCell(
            @Nullable CellInfo cellInfo, long elapsedTime, long currentTime) {
        if (cellInfo == null) {
            return VisibleCell.UNKNOWN_VISIBLE_CELL;
        }
        long cellInfoAge = elapsedTime - TimeUnit.NANOSECONDS.toMillis(cellInfo.getTimeStamp());
        long cellTimestamp = currentTime - cellInfoAge;
        if (cellInfo instanceof CellInfoCdma) {
            CellIdentityCdma cellIdentityCdma = ((CellInfoCdma) cellInfo).getCellIdentity();
            return VisibleCell.builder(VisibleCell.RadioType.CDMA)
                    .setCellId(cellIdentityCdma.getBasestationId())
                    .setLocationAreaCode(cellIdentityCdma.getNetworkId())
                    .setMobileNetworkCode(cellIdentityCdma.getSystemId())
                    .setTimestamp(cellTimestamp)
                    .build();
        }
        if (cellInfo instanceof CellInfoGsm) {
            CellIdentityGsm cellIdentityGsm = ((CellInfoGsm) cellInfo).getCellIdentity();
            return VisibleCell.builder(VisibleCell.RadioType.GSM)
                    .setCellId(cellIdentityGsm.getCid())
                    .setLocationAreaCode(cellIdentityGsm.getLac())
                    .setMobileCountryCode(cellIdentityGsm.getMcc())
                    .setMobileNetworkCode(cellIdentityGsm.getMnc())
                    .setTimestamp(cellTimestamp)
                    .build();
        }
        if (cellInfo instanceof CellInfoLte) {
            CellIdentityLte cellIdLte = ((CellInfoLte) cellInfo).getCellIdentity();
            return VisibleCell.builder(VisibleCell.RadioType.LTE)
                    .setCellId(cellIdLte.getCi())
                    .setMobileCountryCode(cellIdLte.getMcc())
                    .setMobileNetworkCode(cellIdLte.getMnc())
                    .setPhysicalCellId(cellIdLte.getPci())
                    .setTrackingAreaCode(cellIdLte.getTac())
                    .setTimestamp(cellTimestamp)
                    .build();
        }
        if (cellInfo instanceof CellInfoWcdma) {
            CellIdentityWcdma cellIdentityWcdma = ((CellInfoWcdma) cellInfo).getCellIdentity();
            return VisibleCell.builder(VisibleCell.RadioType.WCDMA)
                    .setCellId(cellIdentityWcdma.getCid())
                    .setLocationAreaCode(cellIdentityWcdma.getLac())
                    .setMobileCountryCode(cellIdentityWcdma.getMcc())
                    .setMobileNetworkCode(cellIdentityWcdma.getMnc())
                    .setPrimaryScramblingCode(cellIdentityWcdma.getPsc())
                    .setTimestamp(cellTimestamp)
                    .build();
        }
        return VisibleCell.UNKNOWN_VISIBLE_CELL;
    }

    /**
     * Returns a CellInfo object representing the currently registered base stations, containing
     * its identity fields and signal strength. Null if no base station is active.
     */
    @Nullable
    private static CellInfo getActiveCellInfo(TelephonyManager telephonyManager) {
        int numRegisteredCellInfo = 0;
        List<CellInfo> cellInfos = telephonyManager.getAllCellInfo();

        if (cellInfos == null) {
            return null;
        }
        CellInfo result = null;

        for (int i = 0; i < cellInfos.size(); i++) {
            CellInfo cellInfo = cellInfos.get(i);
            if (cellInfo.isRegistered()) {
                numRegisteredCellInfo++;
                if (numRegisteredCellInfo > 1) {
                    return null;
                }
                result = cellInfo;
            }
        }
        // Only found one registered cellinfo, so we know which base station was used to measure
        // network quality
        return result;
    }

    /**
     * Computes the connected networks.
     *
     * Only includes network connections that are active or in the process of being set up.
     *
     * @param context The application context
     */
    static VisibleNetworks computeConnectedNetworks(Context context) {
        WifiManager wifiManager = getWifiManager(context);
        TelephonyManager telephonyManager = getTelephonyManager(context);

        VisibleWifi connectedWifi = getConnectedWifiIfKnown(context, wifiManager);
        VisibleCell connectedCell = getConnectedCellIfKnown(context, telephonyManager);

        return VisibleNetworks.create(connectedWifi, connectedCell, null, null);
    }

    /**
     * Computes all visible networks.
     *
     * Along with connected networks, also includes all networks found in the most recent {@link
     * WifiManager} scan, and triggers an update to get refreshed {@link TelephonyManager} {@link
     * CellInfo} data. The {@link CellInfo} includes all available cell information from all radios
     * on the device including the camped/registered, serving, and neighboring cells. This update
     * can degrade latency which is why it is performed asynchronously.
     *
     * @param context The application context
     * @param callback The callback to invoke with the results of this computation
     */
    static void computeVisibleNetworks(Context context, Callback<VisibleNetworks> callback) {
        WifiManager wifiManager = getWifiManager(context);
        TelephonyManager telephonyManager = getTelephonyManager(context);

        VisibleWifi connectedWifi = getConnectedWifiIfKnown(context, wifiManager);
        VisibleCell connectedCell = getConnectedCellIfKnown(context, telephonyManager);

        Set<VisibleWifi> allVisibleWifis = getAllVisibleWifis(context, wifiManager);

        getAllVisibleCells(context, telephonyManager, (allVisibleCells) -> {
            callback.onResult(VisibleNetworks.create(
                    connectedWifi, connectedCell, allVisibleWifis, allVisibleCells));
        });
    }

    private static TelephonyManager getTelephonyManager(Context context) {
        return (TelephonyManager) context.getApplicationContext().getSystemService(
                Context.TELEPHONY_SERVICE);
    }

    private static WifiManager getWifiManager(Context context) {
        Context applicationContext = context.getApplicationContext();
        Object wifiManager = applicationContext.getSystemService(Context.WIFI_SERVICE);
        return (WifiManager) wifiManager;
    }

    private static boolean hasPermission(Context context, String permission) {
        return ApiCompatibilityUtils.checkPermission(
                       context, permission, Process.myPid(), Process.myUid())
                == PackageManager.PERMISSION_GRANTED;
    }

    private static boolean hasLocationPermission(Context context) {
        if (BuildInfo.isAtLeastQ()) {
            return hasPermission(context, Manifest.permission.ACCESS_FINE_LOCATION);
        }

        return hasPermission(context, Manifest.permission.ACCESS_COARSE_LOCATION)
                || hasPermission(context, Manifest.permission.ACCESS_FINE_LOCATION);
    }

    private static boolean hasLocationAndWifiPermission(Context context) {
        return hasLocationPermission(context)
                && hasPermission(context, Manifest.permission.ACCESS_WIFI_STATE);
    }

    /**
     * Wrapper around static time providers that allows us to mock the implementation in
     * tests.
     */
    static class TimeProvider {
        /**
         * Get current time in milliseconds.
         */
        long getCurrentTime() {
            return System.currentTimeMillis();
        }

        /**
         * Get elapsed real time in milliseconds.
         */
        long getElapsedRealtime() {
            return SystemClock.elapsedRealtime();
        }
    }
}

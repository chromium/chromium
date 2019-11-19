// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.geo;

import android.support.v4.util.ObjectsCompat;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.Arrays;
import java.util.Set;

/**
 * Visible networks. Stores the data of connected and visible networks.
 */
class VisibleNetworks {
    private static final String TAG = "VisibleNetworks";

    @Nullable
    private final VisibleWifi mConnectedWifi;
    @Nullable
    private final VisibleCell mConnectedCell;
    @Nullable
    private final Set<VisibleWifi> mAllVisibleWifis;
    @Nullable
    private final Set<VisibleCell> mAllVisibleCells;

    private VisibleNetworks(@Nullable VisibleWifi connectedWifi,
            @Nullable VisibleCell connectedCell, @Nullable Set<VisibleWifi> allVisibleWifis,
            @Nullable Set<VisibleCell> allVisibleCells) {
        mConnectedWifi = connectedWifi;
        mConnectedCell = connectedCell;
        mAllVisibleWifis = allVisibleWifis;
        mAllVisibleCells = allVisibleCells;
    }

    static VisibleNetworks create(@Nullable VisibleWifi connectedWifi,
            @Nullable VisibleCell connectedCell, @Nullable Set<VisibleWifi> allVisibleWifis,
            @Nullable Set<VisibleCell> allVisibleCells) {
        return new VisibleNetworks(connectedWifi, connectedCell, allVisibleWifis, allVisibleCells);
    }

    /**
     * Returns the connected {@link VisibleWifi} or null if the connected wifi is unknown.
     */
    @Nullable
    VisibleWifi connectedWifi() {
        return mConnectedWifi;
    }

    /**
     * Returns the connected {@link VisibleCell} or null if the connected cell is unknown.
     */
    @Nullable
    VisibleCell connectedCell() {
        return mConnectedCell;
    }

    /**
     * Returns the current set of {@link VisibleWifi}s that are visible (including not connected
     * networks), or null if the set is unknown.
     */
    @Nullable
    Set<VisibleWifi> allVisibleWifis() {
        return mAllVisibleWifis;
    }

    /**
     * Returns the current set of {@link VisibleCell}s that are visible (including not connected
     * networks), or null if the set is unknown.
     */
    @Nullable
    Set<VisibleCell> allVisibleCells() {
        return mAllVisibleCells;
    }

    /**
     * Returns whether this object is empty, meaning there is no visible networks at all.
     */
    final boolean isEmpty() {
        Set<VisibleWifi> allVisibleWifis = allVisibleWifis();
        Set<VisibleCell> allVisibleCells = allVisibleCells();
        return connectedWifi() == null && connectedCell() == null
                && (allVisibleWifis == null || allVisibleWifis.size() == 0)
                && (allVisibleCells == null || allVisibleCells.size() == 0);
    }

    /**
     * Compares the specified object with this VisibleNetworks for equality.  Returns
     * {@code true} if the given object is a VisibleNetworks and has identical values for
     * all of its fields.
     */
    @Override
    public boolean equals(Object object) {
        if (!(object instanceof VisibleNetworks)) {
            return false;
        }
        VisibleNetworks that = (VisibleNetworks) object;
        return ObjectsCompat.equals(mConnectedWifi, that.connectedWifi())
                && ObjectsCompat.equals(mConnectedCell, that.connectedCell())
                && ObjectsCompat.equals(mAllVisibleWifis, that.allVisibleWifis())
                && ObjectsCompat.equals(mAllVisibleCells, that.allVisibleCells());
    }

    private static int objectsHashCode(Object o) {
        return o != null ? o.hashCode() : 0;
    }

    private static int objectsHash(Object... a) {
        return Arrays.hashCode(a);
    }

    @Override
    public int hashCode() {
        return objectsHash(mConnectedWifi, mConnectedCell, objectsHashCode(mAllVisibleWifis),
                objectsHashCode(mAllVisibleCells));
    }

    /**
     * Specification of a visible wifi.
     */
    static class VisibleWifi {
        static final VisibleWifi NO_WIFI_INFO = VisibleWifi.create(null, null, null, null);

        @Nullable
        private final String mSsid;
        @Nullable
        private final String mBssid;
        @Nullable
        private final Integer mLevel;
        @Nullable
        private final Long mTimestampMs;

        private VisibleWifi(@Nullable String ssid, @Nullable String bssid, @Nullable Integer level,
                @Nullable Long timestampMs) {
            mSsid = ssid;
            mBssid = bssid;
            mLevel = level;
            mTimestampMs = timestampMs;
        }

        static VisibleWifi create(@Nullable String ssid, @Nullable String bssid,
                @Nullable Integer level, @Nullable Long timestampMs) {
            return new VisibleWifi(ssid, bssid, level, timestampMs);
        }

        /**
         * Returns the SSID of the visible Wifi, or null if unknown.
         */
        @Nullable
        String ssid() {
            return mSsid;
        }

        /**
         * Returns the BSSID of the visible Wifi, or null if unknown.
         */
        @Nullable
        String bssid() {
            return mBssid;
        }

        /**
         * Returns the signal level in dBm (RSSI), {@code null} if unknown.
         */
        @Nullable
        Integer level() {
            return mLevel;
        }

        /**
         * Returns the timestamp in Ms, {@code null} if unknown.
         */
        @Nullable
        Long timestampMs() {
            return mTimestampMs;
        }

        /**
         * Compares the specified object with this VisibleWifi for equality.  Returns
         * {@code true} if the given object is a VisibleWifi and has identical values for
         * all of its fields except level and timestampMs.
         */
        @Override
        public boolean equals(Object object) {
            if (!(object instanceof VisibleWifi)) {
                return false;
            }

            VisibleWifi that = (VisibleWifi) object;
            return ObjectsCompat.equals(mSsid, that.ssid())
                    && ObjectsCompat.equals(mBssid, that.bssid());
        }

        @Override
        public int hashCode() {
            return VisibleNetworks.objectsHash(mSsid, mBssid);
        }

        /**
         * Encodes a VisibleWifi into its corresponding PartnerLocationDescriptor.VisibleNetwork
         * proto.
         */
        PartnerLocationDescriptor.VisibleNetwork toProto(boolean connected) {
            PartnerLocationDescriptor.VisibleNetwork.Builder visibleNetworkBuilder =
                    PartnerLocationDescriptor.VisibleNetwork.newBuilder();

            PartnerLocationDescriptor.VisibleNetwork.WiFi.Builder wifiBuilder =
                    PartnerLocationDescriptor.VisibleNetwork.WiFi.newBuilder();

            if (bssid() != null) wifiBuilder.setBssid(bssid());
            if (level() != null) wifiBuilder.setLevelDbm(level());

            visibleNetworkBuilder.setWifi(wifiBuilder.build());
            if (timestampMs() != null) visibleNetworkBuilder.setTimestampMs(timestampMs());
            visibleNetworkBuilder.setConnected(connected);

            return visibleNetworkBuilder.build();
        }
    }

    /**
     * Specification of a visible cell.
     */
    static class VisibleCell {
        static final VisibleCell UNKNOWN_VISIBLE_CELL =
                VisibleCell.builder(VisibleCell.RadioType.UNKNOWN).build();
        static final VisibleCell UNKNOWN_MISSING_LOCATION_PERMISSION_VISIBLE_CELL =
                VisibleCell.builder(VisibleCell.RadioType.UNKNOWN_MISSING_LOCATION_PERMISSION)
                        .build();

        /**
         * Represents all possible values of radio type that we track.
         */
        @IntDef({RadioType.UNKNOWN, RadioType.UNKNOWN_MISSING_LOCATION_PERMISSION, RadioType.CDMA,
                RadioType.GSM, RadioType.LTE, RadioType.WCDMA})
        @Retention(RetentionPolicy.SOURCE)
        @interface RadioType {
            int UNKNOWN = 0;
            int UNKNOWN_MISSING_LOCATION_PERMISSION = 1;
            int CDMA = 2;
            int GSM = 3;
            int LTE = 4;
            int WCDMA = 5;
            int NUM_ENTRIES = 6;
        }

        static Builder builder(@RadioType int radioType) {
            return new VisibleCell.Builder().setRadioType(radioType);
        }

        @RadioType
        private final int mRadioType;
        @Nullable
        private final Integer mCellId;
        @Nullable
        private final Integer mLocationAreaCode;
        @Nullable
        private final Integer mMobileCountryCode;
        @Nullable
        private final Integer mMobileNetworkCode;
        @Nullable
        private final Integer mPrimaryScramblingCode;
        @Nullable
        private final Integer mPhysicalCellId;
        @Nullable
        private final Integer mTrackingAreaCode;
        @Nullable
        private Long mTimestampMs;

        private VisibleCell(Builder builder) {
            mRadioType = builder.mRadioType;
            mCellId = builder.mCellId;
            mLocationAreaCode = builder.mLocationAreaCode;
            mMobileCountryCode = builder.mMobileCountryCode;
            mMobileNetworkCode = builder.mMobileNetworkCode;
            mPrimaryScramblingCode = builder.mPrimaryScramblingCode;
            mPhysicalCellId = builder.mPhysicalCellId;
            mTrackingAreaCode = builder.mTrackingAreaCode;
            mTimestampMs = builder.mTimestampMs;
        }

        /**
         * Returns the radio type of the visible cell.
         */
        @RadioType
        int radioType() {
            return mRadioType;
        }

        /**
         * Returns the gsm cell id, {@code null} if unknown.
         */
        @Nullable
        Integer cellId() {
            return mCellId;
        }

        /**
         * Returns the gsm location area code, {@code null} if unknown.
         */
        @Nullable
        Integer locationAreaCode() {
            return mLocationAreaCode;
        }

        /**
         * Returns the mobile country code, {@code null} if unknown or GSM.
         */
        @Nullable
        Integer mobileCountryCode() {
            return mMobileCountryCode;
        }

        /**
         * Returns the mobile network code, {@code null} if unknown or GSM.
         */
        @Nullable
        Integer mobileNetworkCode() {
            return mMobileNetworkCode;
        }

        /**
         * On a UMTS network, returns the primary scrambling code of the serving cell, {@code null}
         * if unknown or GSM.
         */
        @Nullable
        Integer primaryScramblingCode() {
            return mPrimaryScramblingCode;
        }

        /**
         * Returns the physical cell id, {@code null} if unknown or not LTE.
         */
        @Nullable
        Integer physicalCellId() {
            return mPhysicalCellId;
        }

        /**
         * Returns the tracking area code, {@code null} if unknown or not LTE.
         */
        @Nullable
        Integer trackingAreaCode() {
            return mTrackingAreaCode;
        }

        /**
         * Returns the timestamp in Ms, {@code null} if unknown.
         */
        @Nullable
        Long timestampMs() {
            return mTimestampMs;
        }

        /**
         * Compares the specified object with this VisibleCell for equality.  Returns
         * {@code true} if the given object is a VisibleWifi and has identical values for
         * all of its fields except timestampMs.
         */
        @Override
        public boolean equals(Object object) {
            if (!(object instanceof VisibleCell)) {
                return false;
            }
            VisibleCell that = (VisibleCell) object;
            return ObjectsCompat.equals(mRadioType, that.radioType())
                    && ObjectsCompat.equals(mCellId, that.cellId())
                    && ObjectsCompat.equals(mLocationAreaCode, that.locationAreaCode())
                    && ObjectsCompat.equals(mMobileCountryCode, that.mobileCountryCode())
                    && ObjectsCompat.equals(mMobileNetworkCode, that.mobileNetworkCode())
                    && ObjectsCompat.equals(mPrimaryScramblingCode, that.primaryScramblingCode())
                    && ObjectsCompat.equals(mPhysicalCellId, that.physicalCellId())
                    && ObjectsCompat.equals(mTrackingAreaCode, that.trackingAreaCode());
        }

        @Override
        public int hashCode() {
            return VisibleNetworks.objectsHash(mRadioType, mCellId, mLocationAreaCode,
                    mMobileCountryCode, mMobileNetworkCode, mPrimaryScramblingCode, mPhysicalCellId,
                    mTrackingAreaCode);
        }

        /**
         * Encodes a VisibleCell into its corresponding PartnerLocationDescriptor.VisibleNetwork
         * proto.
         */
        PartnerLocationDescriptor.VisibleNetwork toProto(boolean connected) {
            PartnerLocationDescriptor.VisibleNetwork.Builder visibleNetworkBuilder =
                    PartnerLocationDescriptor.VisibleNetwork.newBuilder();

            PartnerLocationDescriptor.VisibleNetwork.Cell.Builder cellBuilder =
                    PartnerLocationDescriptor.VisibleNetwork.Cell.newBuilder();

            switch (radioType()) {
                case VisibleCell.RadioType.CDMA:
                    cellBuilder.setType(PartnerLocationDescriptor.VisibleNetwork.Cell.Type.CDMA);
                    break;
                case VisibleCell.RadioType.GSM:
                    cellBuilder.setType(PartnerLocationDescriptor.VisibleNetwork.Cell.Type.GSM);
                    break;
                case VisibleCell.RadioType.LTE:
                    cellBuilder.setType(PartnerLocationDescriptor.VisibleNetwork.Cell.Type.LTE);
                    break;
                case VisibleCell.RadioType.WCDMA:
                    cellBuilder.setType(PartnerLocationDescriptor.VisibleNetwork.Cell.Type.WCDMA);
                    break;
                case VisibleCell.RadioType.UNKNOWN:
                case VisibleCell.RadioType.UNKNOWN_MISSING_LOCATION_PERMISSION:
                default:
                    cellBuilder.setType(PartnerLocationDescriptor.VisibleNetwork.Cell.Type.UNKNOWN);
                    break;
            }
            if (cellId() != null) cellBuilder.setCellId(cellId());
            if (locationAreaCode() != null) cellBuilder.setLocationAreaCode(locationAreaCode());
            if (mobileCountryCode() != null) cellBuilder.setMobileCountryCode(mobileCountryCode());
            if (mobileNetworkCode() != null) cellBuilder.setMobileNetworkCode(mobileNetworkCode());
            if (primaryScramblingCode() != null)
                cellBuilder.setPrimaryScramblingCode(primaryScramblingCode());
            if (physicalCellId() != null) cellBuilder.setPhysicalCellId(physicalCellId());
            if (trackingAreaCode() != null) cellBuilder.setTrackingAreaCode(trackingAreaCode());

            visibleNetworkBuilder.setCell(cellBuilder.build());
            if (timestampMs() != null) visibleNetworkBuilder.setTimestampMs(timestampMs());
            visibleNetworkBuilder.setConnected(connected);

            return visibleNetworkBuilder.build();
        }

        /**
         * A {@link VisibleCell} builder.
         */
        static class Builder {
            @RadioType
            private int mRadioType;
            @Nullable
            private Integer mCellId;
            @Nullable
            private Integer mLocationAreaCode;
            @Nullable
            private Integer mMobileCountryCode;
            @Nullable
            private Integer mMobileNetworkCode;
            @Nullable
            private Integer mPrimaryScramblingCode;
            @Nullable
            private Integer mPhysicalCellId;
            @Nullable
            private Integer mTrackingAreaCode;
            @Nullable
            private Long mTimestampMs;

            Builder setRadioType(@RadioType int radioType) {
                mRadioType = radioType;
                return this;
            }

            Builder setCellId(@Nullable Integer cellId) {
                mCellId = cellId;
                return this;
            }

            Builder setLocationAreaCode(@Nullable Integer locationAreaCode) {
                mLocationAreaCode = locationAreaCode;
                return this;
            }

            Builder setMobileCountryCode(@Nullable Integer mobileCountryCode) {
                mMobileCountryCode = mobileCountryCode;
                return this;
            }

            Builder setMobileNetworkCode(@Nullable Integer mobileNetworkCode) {
                mMobileNetworkCode = mobileNetworkCode;
                return this;
            }

            Builder setPrimaryScramblingCode(@Nullable Integer primaryScramblingCode) {
                mPrimaryScramblingCode = primaryScramblingCode;
                return this;
            }

            Builder setPhysicalCellId(@Nullable Integer physicalCellId) {
                mPhysicalCellId = physicalCellId;
                return this;
            }

            Builder setTrackingAreaCode(@Nullable Integer trackingAreaCode) {
                mTrackingAreaCode = trackingAreaCode;
                return this;
            }

            Builder setTimestamp(@Nullable Long timestampMs) {
                mTimestampMs = timestampMs;
                return this;
            }

            VisibleCell build() {
                return new VisibleCell(this);
            }
        }
    }
}

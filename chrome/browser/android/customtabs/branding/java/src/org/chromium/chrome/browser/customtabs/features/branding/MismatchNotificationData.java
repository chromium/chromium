// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features.branding;

import androidx.annotation.NonNull;

import com.google.common.io.BaseEncoding;
import com.google.protobuf.InvalidProtocolBufferException;

import org.chromium.chrome.browser.customtabs.features.branding.proto.AccountMismatchData.Account;
import org.chromium.chrome.browser.customtabs.features.branding.proto.AccountMismatchData.AllAccounts;
import org.chromium.chrome.browser.customtabs.features.branding.proto.AccountMismatchData.App;
import org.chromium.chrome.browser.customtabs.features.branding.proto.AccountMismatchData.CloseType;

import java.util.HashMap;
import java.util.Map;

/** Collection of app data storing the information on sign-in prompt UI for a specific profile. */
public class MismatchNotificationData {
    /** Per-app prompt UI data */
    public static class AppUiData {
        /** How many times the UI was shown to user. */
        public int showCount;

        /** How the UI was closed. */
        public int closeType;

        /** How many times the UI was actively dismissed. */
        public int userActCount;

        @Override
        public boolean equals(Object o) {
            if (o instanceof AppUiData data) {
                return data.showCount == showCount
                        && data.closeType == closeType
                        && data.userActCount == userActCount;
            }
            return false;
        }

        public boolean isEmpty() {
            return showCount == 0
                    && closeType == CloseType.UNKNOWN.getNumber()
                    && userActCount == 0;
        }
    }

    /**
     * The entire notification data for apps. Nested map of {@link AppUiData} with the account/app
     * ID as a key.
     */
    private final Map<String, Map<String, AppUiData>> mDataMap = new HashMap<>();

    /**
     * Returns the notification data for a given account/app ID. An empty data is returned if the
     * corresponding entry has not been created yet.
     *
     * @param account Account ID
     * @param appId App ID
     */
    public @NonNull AppUiData getAppData(String accountId, String appId) {
        Map<String, AppUiData> accountData = mDataMap.get(accountId);
        AppUiData res = accountData != null ? accountData.get(appId) : null;
        return res != null ? res : new AppUiData();
    }

    /**
     * Stores the notification data for a given account/app ID.
     *
     * @param account Account ID
     * @param appId App ID
     */
    public void setAppData(String accountId, String appId, @NonNull AppUiData data) {
        Map<String, AppUiData> accountData = mDataMap.get(accountId);
        if (accountData != null) {
            accountData.put(appId, data);
        } else {
            // Create a new inner app -> data map if it doesn't exist.
            accountData = new HashMap<String, AppUiData>();
            accountData.put(appId, data);
            mDataMap.put(accountId, accountData);
        }
    }

    /** Serialize the class to a String in Base64 encoding. */
    String toBase64() {
        var mimDataBuilder = AllAccounts.newBuilder();
        for (var account : mDataMap.entrySet()) {
            String accountId = account.getKey();
            var accountDataBuilder = Account.newBuilder();
            accountDataBuilder.setObfuscatedGaia(accountId);
            for (var app : account.getValue().entrySet()) {
                String appId = app.getKey();
                AppUiData aud = app.getValue();
                var appData =
                        App.newBuilder()
                                .setId(appId)
                                .setShowCount(aud.showCount)
                                .setCloseType(CloseType.forNumber(aud.closeType))
                                .setUserActCount(aud.userActCount)
                                .build();
                accountDataBuilder.addAppData(appData);
            }
            mimDataBuilder.addAccountData(accountDataBuilder.build());
        }
        return BaseEncoding.base64().encode(mimDataBuilder.build().toByteArray());
    }

    /** Restore the class from a serialized string in Base64 encoding. */
    static MismatchNotificationData fromBase64(String s) {
        MismatchNotificationData mimData = null;
        AllAccounts protoData = null;
        try {
            protoData = AllAccounts.parseFrom(BaseEncoding.base64().decode(s));
        } catch (InvalidProtocolBufferException e) {
            return null;
        }
        for (var account : protoData.getAccountDataList()) {
            String accountId = account.getObfuscatedGaia();
            for (var app : account.getAppDataList()) {
                String appId = app.getId();
                var appData = new AppUiData();
                appData.showCount = app.getShowCount();
                appData.closeType = app.getCloseType().getNumber();
                appData.userActCount = app.getUserActCount();
                if (mimData == null) mimData = new MismatchNotificationData();
                mimData.setAppData(accountId, appId, appData);
            }
        }
        return mimData;
    }

    public boolean isEmpty() {
        return mDataMap.isEmpty();
    }

    public Map<String, Map<String, AppUiData>> getAllForTesting() {
        return mDataMap;
    }
}

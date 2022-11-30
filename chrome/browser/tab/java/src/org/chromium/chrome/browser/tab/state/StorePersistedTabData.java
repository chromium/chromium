// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab.state;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import com.google.protobuf.InvalidProtocolBufferException;

import org.json.JSONArray;
import org.json.JSONObject;

import org.chromium.base.Callback;
import org.chromium.base.Log;
import org.chromium.build.annotations.DoNotClassMerge;
import org.chromium.chrome.browser.endpoint_fetcher.EndpointFetcher;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.proto.StorePersistedTabData.StorePersistedTabDataProto;
import org.chromium.net.NetworkTrafficAnnotationTag;

import java.nio.ByteBuffer;
import java.util.Locale;
import java.util.concurrent.TimeUnit;

/**
 * {@link PersistedTabData} for Store websites with opening/closing hours.
 * TODO(crbug.com/1199134) Add tests for StorePersistedTabData
 *
 * This class should not be merged because it is being used as a key in a Map
 * in PersistedTabDataConfiguration.java.
 */
@DoNotClassMerge
public class StorePersistedTabData extends PersistedTabData {
    private static final String MISSING_STRING = "missing";
    private static final String COLON_STRING = ":";
    private static final String TIME_SEPARATOR_STRING = " - ";
    private static final String AM_STRING = " A.M";
    private static final String PM_STRING = " P.M";
    private static final String IS_PM_STRING = "PM";
    private static final String AM_PM_STRING = "amPm";
    private static final String MINUTE_STRING = "minute";
    private static final String HOUR_STRING = "hour";
    private static final String CLOSING_TIME_OBJECT = "closingTime";
    private static final String OPENING_TIME_OBJECT = "openingTime";
    private static final String TIME_SPAN_ARRAY = "timeSpan";
    private static final String STORE_HOURS = "storeHours";
    private static final String REPRESENTATIONS_ARRAY = "representations";
    private static final int HOURS_TWELVE = 12;
    private static final int MINUTES_TEN = 10;
    private static final String DIGIT_ZERO_STRING = "0";
    private static final int STANDARD_MINUTES_ONE_PM = 1300;
    private static final int STANDARD_MINUTES_ONE_AM = 100;
    private static final int STANDARD_MINUTES_NOON = 1200;
    private static final int STANDARD_MINUTES_ONE_HOUR = 60;
    private static final int STANDARD_MINUTES_TEN_AM = 1000;
    private static final int STANDARD_MINUTES_MIDNIGHT = 0;
    private static final int FALLBACK = -1;

    // TODO(crbug.com/1196065) Make endpoint finch configurable.
    private static final String ENDPOINT =
            "https://task-management-chrome.sandbox.google.com/tabs/representations?url=%s&locale=en:US";
    private static final String[] SCOPES =
            new String[] {"https://www.googleapis.com/auth/userinfo.email",
                    "https://www.googleapis.com/auth/userinfo.profile"};
    private static final long TIMEOUT_MS = 10000L;
    private static final String HTTPS_METHOD = "GET";
    private static final String CONTENT_TYPE = "application/json; charset=UTF-8";
    private static final String EMPTY_POST_DATA = "";
    private static final String PERSISTED_TAB_DATA_ID = "STPTD";
    private static final String TAG = "STPTD";

    @VisibleForTesting
    public static final long ONE_HOUR_MS = TimeUnit.HOURS.toMillis(1);

    @VisibleForTesting
    @Nullable
    protected StoreHours mStoreHours;

    /**
     * Constructor for {@link StorePersistedTabData}
     * @param tab {@link Tab} StorePersistedTabData is created for
     * @param data serialized {@link StorePersistedTabData} (this is for when the data is acquired
     *         from storage because it was cached or the app was closed).
     * @param storage {@link PersistedTabDataStorage}
     * @param persistedTabDataId id for {@link StorePersistedTabData}
     */
    protected StorePersistedTabData(
            Tab tab, ByteBuffer data, PersistedTabDataStorage storage, String persistedTabDataId) {
        super(tab, storage, persistedTabDataId);
    }

    /**
     * Acquire {@link StorePersistedTabData} for a {@link Tab}
     * @param tab {@link Tab} StorePersistedTabData is created for
     * @param storage {@link PersistedTabDataStorage}
     * @param persistedTabDataId id for {@link StorePersistedTabData}
     */
    StorePersistedTabData(
            Tab tab, PersistedTabDataStorage persistedTabDataStorage, String persistedTabDataId) {
        super(tab, persistedTabDataStorage, persistedTabDataId);
    }

    /**
     * Acquire {@link StorePersistedTabData} for a {@link Tab}
     * @param tab {@link Tab} StorePersistedTabData is created for
     * @param storeHours {@link StoreHours} Store hours for the website open in this tab
     */
    StorePersistedTabData(Tab tab, StoreHours storeHours) {
        // TODO(crbug.com/1192808) Implement reading of storage implementation and unique identifier
        this(tab);
        mStoreHours = storeHours;
    }

    /**
     * Acquire {@link StorePersistedTabData} for a {@link Tab}
     * @param tab {@link Tab} StorePersistedTabData is created for
     */
    StorePersistedTabData(Tab tab) {
        // TODO(crbug.com/1192808) Implement reading of storage implementation and unique identifier
        super(tab,
                PersistedTabDataConfiguration.get(StorePersistedTabData.class, tab.isIncognito())
                        .getStorage(),
                PersistedTabDataConfiguration.get(StorePersistedTabData.class, tab.isIncognito())
                        .getId());
    }

    /**
     * StoreHours data type for {@link StorePersistedTabData}
     */
    public static class StoreHours {
        protected final int mOpeningTime;
        protected final int mClosingTime;

        /**
         * @param openingTime opening time in military minutes 0000 = 12:00 A.M
         * @param closingTime closing time in military minutes 0000 = 12:00 A.M
         */
        public StoreHours(int openingTime, int closingTime) {
            mOpeningTime = openingTime;
            mClosingTime = closingTime;
        }
    }

    @Nullable
    public String getStoreHoursString() {
        if (mStoreHours != null && (mStoreHours.mOpeningTime > 0 || mStoreHours.mClosingTime > 0)) {
            return formatStoreHours(mStoreHours.mOpeningTime, mStoreHours.mClosingTime);
        } else {
            return null;
        }
    }

    // Formats opening and closing time to string displayed in frontend.
    private static String formatStoreHours(int openingTime, int closingTime) {
        // TODO(crbug.com/1210725) Look into and implement RTL support if needed.
        StringBuilder sb = new StringBuilder();
        sb.append(createTimeString(openingTime));
        sb.append(TIME_SEPARATOR_STRING);
        sb.append(createTimeString(closingTime));
        return sb.toString();
    }

    private static String createTimeString(int time) {
        int standardTimeMinutes = time;
        if (time < STANDARD_MINUTES_ONE_AM) {
            standardTimeMinutes += STANDARD_MINUTES_NOON;
        }
        if (time >= STANDARD_MINUTES_ONE_PM) {
            standardTimeMinutes -= STANDARD_MINUTES_NOON;
        }
        return getTimeStringFromStandardTimeMinutes(standardTimeMinutes, time >= 1200);
    }

    @VisibleForTesting
    protected static String getTimeStringFromStandardTimeMinutesReplace(
            int standardTimeMinutes, boolean isPM) {
        if (standardTimeMinutes < 0) return "";
        StringBuilder standardTimeMinutesSB =
                new StringBuilder(String.valueOf(standardTimeMinutes));
        StringBuilder timeString = new StringBuilder();
        if (standardTimeMinutesSB.length() <= 3) {
            timeString.append(standardTimeMinutesSB.charAt(0));
            timeString.append(COLON_STRING);
            timeString.append(standardTimeMinutesSB.substring(1, 3));
        } else {
            timeString.append(standardTimeMinutesSB.substring(0, 2));
            timeString.append(COLON_STRING);
            timeString.append(standardTimeMinutesSB.substring(2, 4));
        }
        if (isPM) {
            timeString.append(PM_STRING);
        } else {
            timeString.append(AM_STRING);
        }
        return timeString.toString();
    }

    @VisibleForTesting
    protected static String getTimeStringFromStandardTimeMinutes(
            int standardTimeMinutes, boolean isPM) {
        if (standardTimeMinutes <= FALLBACK) return "";
        StringBuilder standardTimeMinutesSB =
                new StringBuilder(String.valueOf(standardTimeMinutes));
        StringBuilder timeString = new StringBuilder();
        if (standardTimeMinutes < STANDARD_MINUTES_ONE_HOUR) {
            timeString.append(HOURS_TWELVE);
            timeString.append(COLON_STRING);
            if (standardTimeMinutes < MINUTES_TEN) {
                timeString.append(DIGIT_ZERO_STRING);
            }
            timeString.append(standardTimeMinutes);
        } else if (standardTimeMinutes < STANDARD_MINUTES_TEN_AM) {
            timeString.append(standardTimeMinutesSB.charAt(0));
            timeString.append(COLON_STRING);
            timeString.append(standardTimeMinutesSB.substring(1, 3));
        } else if (standardTimeMinutes < STANDARD_MINUTES_NOON) {
            timeString.append(standardTimeMinutesSB.substring(0, 2));
            timeString.append(COLON_STRING);
            timeString.append(standardTimeMinutesSB.substring(2, 4));
        } else if (standardTimeMinutes < STANDARD_MINUTES_ONE_PM) {
            timeString.append(standardTimeMinutesSB.substring(0, 2));
            timeString.append(COLON_STRING);
            timeString.append(standardTimeMinutesSB.substring(2, 4));
        } else {
            timeString.append(
                    Integer.parseInt(standardTimeMinutesSB.substring(0, 2)) - HOURS_TWELVE);
            timeString.append(COLON_STRING);
            timeString.append(standardTimeMinutesSB.substring(2, 4));
        }
        if (isPM) {
            timeString.append(PM_STRING);
        } else {
            timeString.append(AM_STRING);
        }
        return timeString.toString();
    }

    @Override
    Serializer<ByteBuffer> getSerializer() {
        StorePersistedTabDataProto.Builder builder =
                StorePersistedTabDataProto.newBuilder()
                        .setOpeningTime(mStoreHours.mOpeningTime)
                        .setClosingTime(mStoreHours.mClosingTime);
        return () -> {
            return builder.build().toByteString().asReadOnlyByteBuffer();
        };
    }

    @Override
    boolean deserialize(@Nullable ByteBuffer bytes) {
        if (bytes == null) {
            return false;
        }
        try {
            StorePersistedTabDataProto persistedStorePersistedTabData =
                    StorePersistedTabDataProto.parseFrom(bytes);
            mStoreHours = new StoreHours(persistedStorePersistedTabData.getOpeningTime(),
                    persistedStorePersistedTabData.getClosingTime());
        } catch (InvalidProtocolBufferException e) {
            Log.e(TAG, "Error while deserializing: " + e);
            return false;
        }
        return true;
    }

    @Override
    public String getUmaTag() {
        return TAG;
    }

    @Override
    public long getTimeToLiveMs() {
        return ONE_HOUR_MS;
    }

    /**
     * Acquire {@link StorePersistedTabData} for a {@link Tab}
     * @param tab {@link Tab} {@link StorePersistedTabData} is acquired for
     * @param callback {@link Callback} {@link StorePersistedTabData is passed back in}
     */
    public static void from(Tab tab, Callback<StorePersistedTabData> callback) {
        // TODO(crbug.com/995852): Replace MISSING_TRAFFIC_ANNOTATION with a real traffic
        // annotation.
        PersistedTabData.from(tab,
                (data, storage, id, factoryCallback)
                        -> {
                    factoryCallback.onResult(new StorePersistedTabData(tab, storage, id));
                },
                (supplierCallback)
                        -> {
                    EndpointFetcher.fetchUsingOAuth(
                            (endpointResponse)
                                    -> {
                                supplierCallback.onResult(
                                        build(tab, endpointResponse.getResponseString()));
                            },
                            Profile.getLastUsedRegularProfile(), PERSISTED_TAB_DATA_ID,
                            String.format(Locale.US, ENDPOINT, tab.getUrl().getSpec()),
                            HTTPS_METHOD, CONTENT_TYPE, SCOPES, EMPTY_POST_DATA, TIMEOUT_MS,
                            NetworkTrafficAnnotationTag.MISSING_TRAFFIC_ANNOTATION);
                },
                StorePersistedTabData.class, callback);
    }

    @Nullable
    private static StorePersistedTabData build(Tab tab, String responseString) {
        try {
            JSONObject jsonObject = new JSONObject(responseString);
            JSONArray optStoreHours = jsonObject.optJSONArray(REPRESENTATIONS_ARRAY)
                                              .optJSONObject(0)
                                              .optJSONArray(STORE_HOURS);
            return new StorePersistedTabData(tab, findWidestHours(optStoreHours));
        } catch (Exception e) {
            Log.e(TAG, "Error parsing JSON: " + e.getMessage());
            return null;
        }
    }

    @Nullable
    private static StoreHours findWidestHours(JSONArray storeHoursArray) {
        int currentGap = 0;
        int maxGap = 0;
        int maxGapOpeningTime = 0;
        int maxGapClosingTime = 0;
        int openingTime;
        int closingTime;
        if (storeHoursArray == null) {
            return null;
        }
        for (int i = 0; i < storeHoursArray.length(); i++) {
            try {
                JSONObject storeHours = storeHoursArray.optJSONObject(i).optJSONObject(STORE_HOURS);
                openingTime = getTimeInMilitaryMinutes(storeHours.optJSONArray(TIME_SPAN_ARRAY)
                                                               .optJSONObject(0)
                                                               .optJSONObject(OPENING_TIME_OBJECT),
                        OPENING_TIME_OBJECT);
                closingTime = getTimeInMilitaryMinutes(storeHours.optJSONArray(TIME_SPAN_ARRAY)
                                                               .optJSONObject(0)
                                                               .optJSONObject(CLOSING_TIME_OBJECT),
                        CLOSING_TIME_OBJECT);
            } catch (NullPointerException e) {
                continue;
            }
            currentGap = closingTime - openingTime;
            if (currentGap > maxGap) {
                maxGap = currentGap;
                maxGapOpeningTime = openingTime;
                maxGapClosingTime = closingTime;
            }
        }
        return new StoreHours(maxGapOpeningTime, maxGapClosingTime);
    }

    private static int getTimeInMilitaryMinutes(JSONObject timeObject, String label) {
        if (timeObject == null) return STANDARD_MINUTES_MIDNIGHT;
        int timeHours = timeObject.optInt(HOUR_STRING, FALLBACK);
        int timeMinute = timeObject.optInt(MINUTE_STRING, FALLBACK);
        String amPm = timeObject.optString(AM_PM_STRING, MISSING_STRING);
        StringBuilder sb = new StringBuilder();
        if (amPm.equals(IS_PM_STRING)
                || ((amPm.equals(MISSING_STRING) && label.equals(CLOSING_TIME_OBJECT)))) {
            timeHours += HOURS_TWELVE;
        }

        if (timeHours != FALLBACK) {
            sb.append(timeHours);
        } else {
            sb.append(DIGIT_ZERO_STRING);
        }

        if (timeMinute != FALLBACK) {
            if (timeMinute == 0) {
                sb.append(DIGIT_ZERO_STRING);
            }
            sb.append(timeMinute);
        } else {
            sb.append(DIGIT_ZERO_STRING);
            sb.append(DIGIT_ZERO_STRING);
        }

        return Integer.parseInt(sb.toString());
    }
}

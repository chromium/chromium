// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.nonembedded.crash;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.json.JSONException;
import org.json.JSONObject;

import java.io.InvalidObjectException;
import java.util.HashMap;
import java.util.Iterator;
import java.util.Map;

/** A class that bundles various information about a crash. */
public class CrashInfo {
    /** Crash file report/minidump upload status. */
    public static enum UploadState {
        SKIPPED,
        PENDING,
        PENDING_USER_REQUESTED,
        UPLOADED,
    }

    /** Upload state for the crash. */
    @Nullable public UploadState uploadState;

    /** ID for locally stored data that may or may not be uploaded. */
    @NonNull public String localId;

    /**
     * The time the data was captured in millisecs since epoch.
     * This is useful if the data is stored locally when
     * captured and uploaded at a later time.
     */
    public long captureTime = -1;

    /**
     * ID the crash collecting servers responded with after the crash report is uploaded.
     * Only valid when |uploadState| == Uploaded.
     */
    @Nullable public String uploadId;

    /**
     * The time when the crash report is uploaded in millisecs since epoch.
     * Only valid when |uploadState| == Uploaded.
     */
    public long uploadTime = -1;

    /**
     * Boolean that identifies if the crash should be hidden from the view or not.
     * Default value is false.
     */
    public boolean isHidden;

    @NonNull private final Map<String, String> mCrashKeys;

    private static final String CRASH_LOCAL_ID_KEY = "crash-local-id";
    private static final String CRASH_CAPTURE_TIME_KEY = "crash-capture-time";
    private static final String CRASH_UPLOAD_ID_KEY = "crash-upload-id";
    private static final String CRASH_UPLOAD_TIME_KEY = "crash-upload-time";
    private static final String CRASH_IS_HIDDEN_KEY = "crash-is-hidden";
    private static final String CRASH_KEYS_KEY = "crash-keys";
    // Should match the crash keys used in minidump reports see for example of some keys:
    // {@link android_webview/common/crash_reporter/crash_keys.cc}
    public static final String APP_PACKAGE_NAME_KEY = "app-package-name";
    public static final String APP_PACKAGE_VERSION_CODE_KEY = "app-package-version-code";
    public static final String WEBVIEW_VERSION_KEY = "ver";
    public static final String WEBVIEW_CHANNEL_KEY = "channel";
    public static final String ANDROID_SDK_INT_KEY = "android-sdk-int";

    /**
     * {@code localId} should always has a non-null value because it used to join crash info from
     * different sources together.
     */
    public CrashInfo(@NonNull String localId) {
        this(localId, new HashMap<>());
    }

    /** @param crashKeys crash keys map extracted from the minidump crash report. */
    public CrashInfo(@NonNull String localId, @NonNull Map<String, String> crashKeys) {
        this.localId = localId;
        mCrashKeys = crashKeys;
    }

    /**
     * Create a {@link CrashInfo} from merging the fields of the given two objects.
     * Both of the {@link CrashInfo} should have the same {@code localId}.
     */
    public CrashInfo(@NonNull CrashInfo a, @NonNull CrashInfo b) {
        assert a.localId.equals(b.localId)
                : "CrashInfo objects should be only merged if they have the same localId";
        this.localId = a.localId;

        this.uploadId = a.uploadId != null ? a.uploadId : b.uploadId;
        this.uploadTime = a.uploadTime != -1 ? a.uploadTime : b.uploadTime;

        // When merging two CrashInfos if one of the two UploadStates is UPLOADED then the merged
        // object will have an UPLOADED state regardless of the order. Difference in UploadState my
        // be caused by the file suffix not updated or deleted by the time
        // UnuploadedFilesStateLoader parses the crash directory.
        if (a.uploadState != null && b.uploadState != null) {
            if (a.uploadState == UploadState.UPLOADED || b.uploadState == UploadState.UPLOADED) {
                this.uploadState = UploadState.UPLOADED;
            } else {
                assert a.uploadState == b.uploadState;
            }
        } else {
            this.uploadState = a.uploadState == null ? b.uploadState : a.uploadState;
        }
        // Since capture time may be the last time the crash file is modified, the oldest capture
        // time will be used regardless of the merging order.
        if (a.captureTime != -1 && b.captureTime != -1) {
            this.captureTime = Math.min(a.captureTime, b.captureTime);
        } else {
            this.captureTime = a.captureTime != -1 ? a.captureTime : b.captureTime;
        }

        mCrashKeys = a.mCrashKeys;
        mCrashKeys.putAll(b.mCrashKeys);
    }

    /**
     * Create a {@link CrashInfo} object for testing.
     *
     * {@code appPackageName} is used as a representative of crash keys in tests.
     */
    public static CrashInfo createCrashInfoForTesting(
            String localId,
            long captureTime,
            String uploadId,
            long uploadTime,
            String appPackageName,
            UploadState state) {
        Map<String, String> crashKeys = new HashMap<>();
        if (appPackageName != null) {
            crashKeys.put("app-package-name", appPackageName);
        }
        CrashInfo crashInfo = new CrashInfo(localId, crashKeys);
        crashInfo.captureTime = captureTime;
        crashInfo.uploadId = uploadId;
        crashInfo.uploadTime = uploadTime;
        crashInfo.uploadState = state;

        return crashInfo;
    }

    /**
     * Return the string value of the given crash key extracted from the crash minidump report.
     *
     * @return the string value of the given crash key or {@code null} if not found.
     */
    @Nullable
    public String getCrashKey(@NonNull String key) {
        return mCrashKeys.get(key);
    }

    /**
     * Return the string value of the given crash key extracted from the crash minidump report.
     *
     * This is an accessory method similar to {@link Map#getOrDefault(Object, Object)} which is
     * only supported starting from API 24.
     *
     * @return the string value of the given crash key or {@code default} if not found.
     */
    @NonNull
    public String getCrashKeyOrDefault(@NonNull String key, @NonNull String defaultValue) {
        String value = mCrashKeys.get(key);
        return value == null ? defaultValue : value;
    }

    /**
     * Serialize {@code CrashInfo} object into a JSON object string.
     *
     * This doesn't serialize upload id, upload time or upload state.
     *
     * @return serialized string for the object.
     */
    public String serializeToJson() {
        try {
            JSONObject jsonObj = new JSONObject();
            jsonObj.put(CRASH_LOCAL_ID_KEY, localId);
            if (captureTime != -1) {
                jsonObj.put(CRASH_CAPTURE_TIME_KEY, captureTime);
            }
            if (uploadId != null) {
                jsonObj.put(CRASH_UPLOAD_ID_KEY, uploadId);
            }
            if (uploadTime != -1) {
                jsonObj.put(CRASH_UPLOAD_TIME_KEY, uploadTime);
            }
            jsonObj.put(CRASH_IS_HIDDEN_KEY, isHidden);
            jsonObj.put(CRASH_KEYS_KEY, new JSONObject(mCrashKeys));
            return jsonObj.toString();
        } catch (JSONException e) {
            return null;
        }
    }

    /**
     * Load {@code CrashInfo} from a JSON string.
     *
     * @param jsonString JSON string to load {@code CrashInfo} from.
     * @return {@code CrashInfo} loaded from the serialized JSON object string.
     * @throws JSONException if it's a malformatted JSON string.
     * @throws InvalidObjectException if the JSON Object doesn't have "crash-local-id" field.
     */
    public static CrashInfo readFromJsonString(String jsonString)
            throws JSONException, InvalidObjectException {
        JSONObject jsonObj = new JSONObject(jsonString);
        if (!jsonObj.has(CRASH_LOCAL_ID_KEY)) {
            throw new InvalidObjectException(
                    "JSON Object doesn't have the field " + CRASH_LOCAL_ID_KEY);
        }
        CrashInfo crashInfo = new CrashInfo(jsonObj.getString(CRASH_LOCAL_ID_KEY));

        if (jsonObj.has(CRASH_CAPTURE_TIME_KEY)) {
            crashInfo.captureTime = jsonObj.getLong(CRASH_CAPTURE_TIME_KEY);
        }
        if (jsonObj.has(CRASH_UPLOAD_ID_KEY)) {
            crashInfo.uploadId = jsonObj.getString(CRASH_UPLOAD_ID_KEY);
        }
        if (jsonObj.has(CRASH_UPLOAD_TIME_KEY)) {
            crashInfo.uploadTime = jsonObj.getLong(CRASH_UPLOAD_TIME_KEY);
        }

        if (jsonObj.has(CRASH_IS_HIDDEN_KEY)) {
            crashInfo.isHidden = jsonObj.getBoolean(CRASH_IS_HIDDEN_KEY);
        }

        if (jsonObj.has(CRASH_KEYS_KEY)) {
            JSONObject crashKeysObj = jsonObj.getJSONObject(CRASH_KEYS_KEY);
            Iterator<String> keyIterator = crashKeysObj.keys();
            while (keyIterator.hasNext()) {
                String key = keyIterator.next();
                crashInfo.mCrashKeys.put(key, crashKeysObj.getString(key));
            }
        }

        return crashInfo;
    }
}

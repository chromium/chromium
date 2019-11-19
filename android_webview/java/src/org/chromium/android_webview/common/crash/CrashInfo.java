// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.common.crash;

import androidx.annotation.NonNull;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import java.io.InvalidObjectException;
import java.util.ArrayList;
import java.util.List;

/**
 * A class that bundles various information about a crash.
 */
public class CrashInfo {
    /**
     * Crash file report/minidump upload status.
     */
    public static enum UploadState {
        SKIPPED,
        PENDING,
        PENDING_USER_REQUESTED,
        UPLOADED,
    }

    /**
     * Upload state for the crash.
     */
    public UploadState uploadState;

    /**
     * ID for locally stored data that may or may not be uploaded.
     */
    @NonNull
    public String localId;
    /**
     * The time the data was captured in millisecs since epoch.
     * This is useful if the data is stored locally when
     * captured and uploaded at a later time.
     */
    public long captureTime = -1;
    /**
     * The application package name where a crash happened.
     */
    public String packageName;
    /**
     * List of variation/experiment keys activated when the crash happened.
     */
    public List<String> variations;

    /**
     * ID the crash collecting servers responded with after the crash report is uploaded.
     * Only valid when |uploadState| == Uploaded.
     */
    public String uploadId;
    /**
     * The time when the crash report is uploaded in millisecs since epoch.
     * Only valid when |uploadState| == Uploaded.
     */
    public long uploadTime = -1;

    private static final String CRASH_LOCAL_ID_KEY = "crash-local-id";
    private static final String CRASH_CAPTURE_TIME_KEY = "crash-capture-time";
    private static final String CRASH_PACKAGE_NAME_KEY = "app-package-name";
    private static final String CRASH_VARIATIONS_KEY = "variations";
    private static final String CRASH_UPLOAD_ID_KEY = "crash-upload-id";
    private static final String CRASH_UPLOAD_TIME_KEY = "crash-upload-time";

    /**
     * Create a {@code CrashInfo} object with a non-null localId.
     * This Enforces that localId can never be null, hence eliminate the need for null check.
     * localId should always has a non-null value because it used to join crash info from
     * different sources together.
     */
    public CrashInfo(@NonNull String localId) {
        this.localId = localId;
    }

    /**
     * Serialize {@code CrashInfo} object into a JSON object string.
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
            if (packageName != null) {
                jsonObj.put(CRASH_PACKAGE_NAME_KEY, packageName);
            }
            if (variations != null && !variations.isEmpty()) {
                jsonObj.put(CRASH_VARIATIONS_KEY, new JSONArray(variations));
            }
            if (uploadId != null) {
                jsonObj.put(CRASH_UPLOAD_ID_KEY, uploadId);
            }
            if (uploadTime != -1) {
                jsonObj.put(CRASH_UPLOAD_TIME_KEY, uploadTime);
            }
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
        if (jsonObj.has(CRASH_PACKAGE_NAME_KEY)) {
            crashInfo.packageName = jsonObj.getString(CRASH_PACKAGE_NAME_KEY);
        }
        if (jsonObj.has(CRASH_VARIATIONS_KEY)) {
            JSONArray variationsJSONArr = jsonObj.getJSONArray(CRASH_VARIATIONS_KEY);
            if (variationsJSONArr != null) {
                crashInfo.variations = new ArrayList<>();
                for (int i = 0; i < variationsJSONArr.length(); i++) {
                    crashInfo.variations.add(variationsJSONArr.getString(i));
                }
            }
        }
        if (jsonObj.has(CRASH_UPLOAD_ID_KEY)) {
            crashInfo.uploadId = jsonObj.getString(CRASH_UPLOAD_ID_KEY);
        }
        if (jsonObj.has(CRASH_UPLOAD_TIME_KEY)) {
            crashInfo.uploadTime = jsonObj.getLong(CRASH_UPLOAD_TIME_KEY);
        }

        return crashInfo;
    }
}

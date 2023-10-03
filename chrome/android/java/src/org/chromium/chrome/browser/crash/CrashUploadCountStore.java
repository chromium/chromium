// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.crash;

import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.crash.MinidumpUploadServiceImpl.ProcessType;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;

/**
 * Records number of crashes uploaded in SharedPreferences.
 *
 * These numbers may be recorded even when running in background and the main Chrome Activity does
 * not exist, so they are persisted until the next time it runs.
 */
public class CrashUploadCountStore {
    private static final CrashUploadCountStore INSTANCE = new CrashUploadCountStore();

    private final SharedPreferencesManager mManager;

    private CrashUploadCountStore() {
        mManager = ChromeSharedPreferences.getInstance();
    }

    /**
     * @return the CrashUploadCountStore singleton
     */
    public static CrashUploadCountStore getInstance() {
        return INSTANCE;
    }

    /**
     * @return Number of times of successful crash upload.
     */
    int getCrashSuccessUploadCount(@ProcessType String process) {
        return mManager.readInt(successUploadKey(process));
    }

    void incrementCrashSuccessUploadCount(@ProcessType String process) {
        mManager.incrementInt(successUploadKey(process));
    }

    private String successUploadKey(@ProcessType String process) {
        switch (process) {
            case ProcessType.BROWSER:
                return ChromePreferenceKeys.CRASH_UPLOAD_SUCCESS_BROWSER;
            case ProcessType.RENDERER:
                return ChromePreferenceKeys.CRASH_UPLOAD_SUCCESS_RENDERER;
            case ProcessType.GPU:
                return ChromePreferenceKeys.CRASH_UPLOAD_SUCCESS_GPU;
            case ProcessType.OTHER:
                return ChromePreferenceKeys.CRASH_UPLOAD_SUCCESS_OTHER;
            default:
                throw new IllegalArgumentException("Process type unknown: " + process);
        }
    }

    /**
     * @return Number of times of failure crash upload after reaching the max number of tries.
     */
    int getCrashFailureUploadCount(@ProcessType String process) {
        return mManager.readInt(failureUploadKey(process));
    }

    void incrementCrashFailureUploadCount(@ProcessType String process) {
        mManager.incrementInt(failureUploadKey(process));
    }

    private String failureUploadKey(@ProcessType String process) {
        switch (process) {
            case ProcessType.BROWSER:
                return ChromePreferenceKeys.CRASH_UPLOAD_FAILURE_BROWSER;
            case ProcessType.RENDERER:
                return ChromePreferenceKeys.CRASH_UPLOAD_FAILURE_RENDERER;
            case ProcessType.GPU:
                return ChromePreferenceKeys.CRASH_UPLOAD_FAILURE_GPU;
            case ProcessType.OTHER:
                return ChromePreferenceKeys.CRASH_UPLOAD_FAILURE_OTHER;
            default:
                throw new IllegalArgumentException("Process type unknown: " + process);
        }
    }

    void resetCrashUploadCounts(@ProcessType String process) {
        mManager.writeInt(successUploadKey(process), 0);
        mManager.writeInt(failureUploadKey(process), 0);
    }
}

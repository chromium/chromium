// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feedback;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.content_public.common.ContentSwitches;

import java.util.HashMap;
import java.util.Map;

/** Grabs feedback about the current system. */
@JNINamespace("chrome::android")
public class ProcessIdFeedbackSource implements AsyncFeedbackSource {
    // Process types used for feedback report. These should be in sync with the enum
    // in //content/public/common/process_type.h
    private static final int PROCESS_TYPE_RENDERER = 3;
    private static final int PROCESS_TYPE_UTILITY = 6;
    private static final int PROCESS_TYPE_GPU = 9;

    private static final Map<String, Integer> PROCESS_TYPES = new HashMap<>();

    static {
        PROCESS_TYPES.put(ContentSwitches.SWITCH_RENDERER_PROCESS, PROCESS_TYPE_RENDERER);
        PROCESS_TYPES.put(ContentSwitches.SWITCH_UTILITY_PROCESS, PROCESS_TYPE_UTILITY);
        PROCESS_TYPES.put(ContentSwitches.SWITCH_GPU_PROCESS, PROCESS_TYPE_GPU);
    }

    /** A map of process type -> list of PIDs for that type. Can be {@code null}. */
    private Map<String, String> mProcessMap;

    private boolean mIsReady;

    ProcessIdFeedbackSource() {}

    private static final String processTypeToFeedbackKey(String type) {
        return "Process IDs (" + type + ")";
    }

    // AsyncFeedbackSource implementation.
    @Override
    public void start(final Runnable callback) {
        ProcessIdFeedbackSourceJni.get().start(this);
    }

    @CalledByNative
    private void prepareCompleted(long nativeSource) {
        mProcessMap = new HashMap<>();
        for (Map.Entry<String, Integer> entry : PROCESS_TYPES.entrySet()) {
            long[] pids =
                    ProcessIdFeedbackSourceJni.get()
                            .getProcessIdsForType(
                                    nativeSource, ProcessIdFeedbackSource.this, entry.getValue());
            if (pids.length == 0) continue;
            StringBuilder spids = new StringBuilder();
            for (long pid : pids) {
                if (spids.length() > 0) spids.append(", ");
                spids.append(String.valueOf(pid));
            }
            mProcessMap.put(processTypeToFeedbackKey(entry.getKey()), spids.toString());
        }
        mProcessMap.put(
                processTypeToFeedbackKey("browser"),
                Long.toString(ProcessIdFeedbackSourceJni.get().getCurrentPid()));
        mIsReady = true;
    }

    @Override
    public boolean isReady() {
        return mIsReady;
    }

    @Override
    public Map<String, String> getFeedback() {
        return mProcessMap;
    }

    @NativeMethods
    interface Natives {
        long getCurrentPid();

        void start(ProcessIdFeedbackSource source);

        long[] getProcessIdsForType(
                long nativeProcessIdFeedbackSource,
                ProcessIdFeedbackSource caller,
                int processType);
    }
}

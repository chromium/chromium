// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import org.chromium.base.ThreadUtils;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.components.safe_browsing.SafeBrowsingApiHandler;

import java.util.HashMap;
import java.util.Map;

/** SafeBrowsingApiHandler with fake responses. Create a new instance for each test. */
public class MockSafeBrowsingApiHandler implements SafeBrowsingApiHandler {
    private Observer mObserver;
    // Mock time it takes for a lookup request to complete.
    private static final long DEFAULT_CHECK_DELTA_US = 15;

    // These codes are defined in safe_browsing_api_handler_util.h
    public static final int SOCIAL_ENGINEERING_CODE = 2;
    public static final int ABUSIVE_EXPERIENCE_VIOLATION_CODE = 20;
    public static final int BETTER_ADS_VIOLATION_CODE = 21;
    public static final int THREAT_ATTRIBUTE_CANARY_CODE = 1;
    private static final int NO_THREAT_CODE = 0;
    private static final int SUCCESS_RESPONSE_STATUS_CODE = 0;

    // Accessed on the UI thread.
    private final Map<String, Integer> mResponseThreatTypeMap = new HashMap<>();
    private final Map<String, int[]> mResponseThreatAttributesMap = new HashMap<>();

    @Override
    public void startUriLookup(long callbackId, String uri, int[] threatTypes, int protocol) {
        PostTask.runOrPostTask(
                TaskTraits.UI_DEFAULT,
                () ->
                        mObserver.onUrlCheckDone(
                                callbackId,
                                LookupResult.SUCCESS,
                                getReturnedThreatType(uri, threatTypes),
                                getReturnedThreatAttributes(uri),
                                SUCCESS_RESPONSE_STATUS_CODE,
                                DEFAULT_CHECK_DELTA_US));
    }

    @Override
    public void setObserver(Observer observer) {
        mObserver = observer;
    }

    /** Adds a mock response. Call on the UI thread. */
    public void addMockResponse(String uri, int returnedThreatType) {
        ThreadUtils.assertOnUiThread();
        mResponseThreatTypeMap.put(uri, returnedThreatType);
    }

    /** Adds a mock response with threat attributes. Call on the UI thread. */
    public void addMockResponse(
            String uri, int returnedThreatType, int[] returnedThreatAttributes) {
        addMockResponse(uri, returnedThreatType);
        mResponseThreatAttributesMap.put(uri, returnedThreatAttributes);
    }

    private int getReturnedThreatType(String uri, int[] threatTypes) {
        if (!mResponseThreatTypeMap.containsKey(uri)) {
            return NO_THREAT_CODE;
        }
        int returnedThreatType = mResponseThreatTypeMap.get(uri);
        for (int threatType : threatTypes) {
            if (returnedThreatType == threatType) {
                return returnedThreatType;
            }
        }
        return NO_THREAT_CODE;
    }

    private int[] getReturnedThreatAttributes(String uri) {
        return mResponseThreatAttributesMap.getOrDefault(uri, new int[0]);
    }
}

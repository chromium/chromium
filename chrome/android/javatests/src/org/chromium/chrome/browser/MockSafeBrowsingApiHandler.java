// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.components.safe_browsing.SafeBrowsingApiHandler;

import java.util.HashMap;
import java.util.Map;

/** SafeBrowsingApiHandler that sends fake responses. */
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

    // Global url -> threatType map. In practice there is only one SafeBrowsingApiHandler, but
    // it is cumbersome for tests to reach into the singleton instance directly. So just make this
    // static and modifiable from java tests using a static method.
    private static final Map<String, Integer> sResponseThreatTypeMap = new HashMap<>();
    // Global url -> threatAttributes map.
    private static final Map<String, int[]> sResponseThreatAttributesMap = new HashMap<>();

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

    /*
     * Adds a mock response to the static map.
     * Should be called before the main activity starts up, to avoid thread-unsafe behavior.
     */
    public static void addMockResponse(String uri, int returnedThreatType) {
        sResponseThreatTypeMap.put(uri, returnedThreatType);
    }

    /*
     * Adds a mock response with threat attributes to the static map.
     * Should be called before the main activity starts up, to avoid thread-unsafe behavior.
     */
    public static void addMockResponse(
            String uri, int returnedThreatType, int[] returnedThreatAttributes) {
        sResponseThreatTypeMap.put(uri, returnedThreatType);
        sResponseThreatAttributesMap.put(uri, returnedThreatAttributes);
    }

    /*
     * Clears the mock responses from the static map.
     * Should be called in the test tearDown method.
     */
    public static void clearMockResponses() {
        sResponseThreatTypeMap.clear();
        sResponseThreatAttributesMap.clear();
    }

    private int getReturnedThreatType(String uri, int[] threatTypes) {
        if (!sResponseThreatTypeMap.containsKey(uri)) {
            return NO_THREAT_CODE;
        }
        int returnedThreatType = sResponseThreatTypeMap.get(uri);
        for (int threatType : threatTypes) {
            if (returnedThreatType == threatType) {
                return returnedThreatType;
            }
        }
        return NO_THREAT_CODE;
    }

    private int[] getReturnedThreatAttributes(String uri) {
        return sResponseThreatAttributesMap.getOrDefault(uri, new int[0]);
    }
}

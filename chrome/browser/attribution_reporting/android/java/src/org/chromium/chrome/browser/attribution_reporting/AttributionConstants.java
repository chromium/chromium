// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.attribution_reporting;

/** Public constants for the attribution_reporting module. */
public class AttributionConstants {
    public static final String ACTION_APP_ATTRIBUTION = "android.web.action.APP_ATTRIBUTION";
    public static final String EXTRA_ATTRIBUTION_INTENT = "android.web.extra.ATTRIBUTION_INTENT";

    // Attribution parameters.
    public static final String EXTRA_ATTRIBUTION_SOURCE_EVENT_ID = "attributionSourceEventId";
    public static final String EXTRA_ATTRIBUTION_DESTINATION = "attributionDestination";
    // Optional
    public static final String EXTRA_ATTRIBUTION_REPORT_TO = "attributionReportTo";
    // Optional
    public static final String EXTRA_ATTRIBUTION_EXPIRY = "attributionExpiry";
    // Input event used for validation.
    public static final String EXTRA_INPUT_EVENT = "inputEvent";
}

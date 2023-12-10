// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.historyreport;

/** Represents report about page visit. */
public class UsageReport {
    public final String reportId;

    /** Unique id of a url visited by user. */
    public final String pageId;

    /** Time of the page visit. */
    public final long timestampMs;

    /** Whether page visit was caused by user typing the page url in omnibox. */
    public final boolean typedVisit;

    public UsageReport(String reportId, String pageId, long timestampMs, boolean typedVisit) {
        this.reportId = reportId;
        this.pageId = pageId;
        this.timestampMs = timestampMs;
        this.typedVisit = typedVisit;
    }
}

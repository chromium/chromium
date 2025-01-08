// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.data_sharing.ui.recent_activity;

/** Information required to render the description and timestamp line in the recent activity row. */
class DescriptionAndTimestamp {
    /** The description text which is the left half of the description line. */
    public String description;

    /* The separator string between description and timestamp. */
    public String separator;

    /* The timestamp text. */
    public String timestamp;

    /* The resource ID for the full description line.*/
    public int descriptionFullTextResId;
}

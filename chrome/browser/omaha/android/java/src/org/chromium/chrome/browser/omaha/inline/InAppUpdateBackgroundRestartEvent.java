// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omaha.inline;

import androidx.annotation.IntDef;

import org.chromium.build.annotations.NullMarked;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Events for the background install lifecycle of Chrome Android in-app updates: scheduling,
 * cancellation, guard skips, and the completeUpdate() call from the background task.
 *
 * <p>These values are persisted to logs. Entries should not be renumbered and numeric values should
 * never be reused.
 */
// LINT.IfChange(InAppUpdateBackgroundRestartEvent)
@NullMarked
@IntDef({
    InAppUpdateBackgroundRestartEvent.SCHEDULED,
    InAppUpdateBackgroundRestartEvent.CANCELED,
    InAppUpdateBackgroundRestartEvent.SKIPPED_SCREEN_LOCKED,
    InAppUpdateBackgroundRestartEvent.SKIPPED_INCOGNITO,
    InAppUpdateBackgroundRestartEvent.SKIPPED_VISIBLE_AT_RUN,
    InAppUpdateBackgroundRestartEvent.SKIPPED_NOT_DOWNLOADED,
    InAppUpdateBackgroundRestartEvent.COMPLETE_UPDATE_CALLED,
    InAppUpdateBackgroundRestartEvent.COMPLETE_UPDATE_FAILED,
    InAppUpdateBackgroundRestartEvent.GET_APP_UPDATE_INFO_FAILED,
    InAppUpdateBackgroundRestartEvent.SKIPPED_FOREGROUND_SERVICE,
    InAppUpdateBackgroundRestartEvent.RETRY_SCHEDULED
})
@Retention(RetentionPolicy.SOURCE)
public @interface InAppUpdateBackgroundRestartEvent {
    int SCHEDULED = 0;
    int CANCELED = 1;
    int SKIPPED_SCREEN_LOCKED = 2;
    int SKIPPED_INCOGNITO = 3;
    int SKIPPED_VISIBLE_AT_RUN = 4;
    int SKIPPED_NOT_DOWNLOADED = 5;
    int COMPLETE_UPDATE_CALLED = 6;
    int COMPLETE_UPDATE_FAILED = 7;
    int GET_APP_UPDATE_INFO_FAILED = 8;
    int SKIPPED_FOREGROUND_SERVICE = 9;
    int RETRY_SCHEDULED = 10;
    int COUNT = 11;
}
// LINT.ThenChange(//tools/metrics/histograms/metadata/android/enums.xml:InAppUpdateBackgroundRestartEvent)

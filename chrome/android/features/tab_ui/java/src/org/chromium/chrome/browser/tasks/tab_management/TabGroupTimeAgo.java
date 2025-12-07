// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import androidx.annotation.IntDef;

import org.chromium.build.annotations.NullMarked;

import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;

/** Represents an event that occurred some time ago for a tab group. */
@NullMarked
public class TabGroupTimeAgo {
    @IntDef({TimestampEvent.CREATED, TimestampEvent.UPDATED})
    @Retention(RetentionPolicy.SOURCE)
    @Target(ElementType.TYPE_USE)
    public @interface TimestampEvent {
        int CREATED = 0;
        int UPDATED = 1;
    }

    public final long timestampMs;
    public final @TimestampEvent int eventType;

    /**
     * @param timestampMs The time in milliseconds for the event.
     * @param eventType The event represented by the timestamp.
     */
    public TabGroupTimeAgo(long timestampMs, @TimestampEvent int eventType) {
        this.timestampMs = timestampMs;
        this.eventType = eventType;
    }
}

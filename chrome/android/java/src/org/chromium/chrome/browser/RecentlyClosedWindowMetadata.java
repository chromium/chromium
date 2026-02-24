// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import org.jni_zero.CalledByNative;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tabmodel.TabModel;

import java.util.Objects;

/**
 * Helper class for the RecentlyClosedEntriesManager. Allows a callback to call into C++ with
 * multiple objects. equals() is provided so tests can check equality. Exists as a standalone class
 * (rather than an inner class) to simplify JNI access via JNI Zero.
 */
@NullMarked
public class RecentlyClosedWindowMetadata {
    public @Nullable TabModel tabModel;
    public long timestamp;
    public int instanceId;

    @CalledByNative
    public @Nullable TabModel getTabModel() {
        return tabModel;
    }

    @CalledByNative
    public long getTimestamp() {
        return timestamp;
    }

    @CalledByNative
    public int getInstanceId() {
        return instanceId;
    }

    @Override
    public boolean equals(@Nullable Object obj) {
        if (this == obj) return true;
        if (obj instanceof RecentlyClosedWindowMetadata other) {
            return other.tabModel == tabModel
                    && other.timestamp == timestamp
                    && other.instanceId == instanceId;
        }
        return false;
    }

    @Override
    public int hashCode() {
        return Objects.hash(tabModel, timestamp);
    }
}

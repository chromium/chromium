// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.util.Objects;

/** Options for querying browsing history. */
@NullMarked
public class QueryOptions {
    public final @Nullable String appId;
    public final @Nullable String hostName;
    public final @Nullable String clientId;

    public QueryOptions() {
        this(null, null, null);
    }

    public QueryOptions(
            @Nullable String appId, @Nullable String hostName, @Nullable String clientId) {
        this.appId = appId;
        this.hostName = hostName;
        this.clientId = clientId;
    }

    @Override
    public boolean equals(Object o) {
        if (this == o) return true;
        if (!(o instanceof QueryOptions)) return false;
        QueryOptions that = (QueryOptions) o;
        return Objects.equals(appId, that.appId)
                && Objects.equals(hostName, that.hostName)
                && Objects.equals(clientId, that.clientId);
    }

    @Override
    public int hashCode() {
        return Objects.hash(appId, hostName, clientId);
    }
}

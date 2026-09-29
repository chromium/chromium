// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.util.Collections;
import java.util.List;
import java.util.Objects;

/** Options for querying browsing history. */
@NullMarked
public class QueryOptions {
    // App Id to restrict the query to. If empty, visits from all apps are returned.
    public final @Nullable String appId;
    // Hostname to restrict the query to. If empty, visits from all hosts are returned.
    public final @Nullable String hostName;
    // Client IDs to restrict the query to. If empty, visits from all clients are returned.
    public final List<String> clientIds;

    public QueryOptions() {
        this(null, null, Collections.emptyList());
    }

    public QueryOptions(@Nullable String appId, @Nullable String hostName, List<String> clientIds) {
        this.appId = appId;
        this.hostName = hostName;
        this.clientIds = List.copyOf(clientIds);
    }

    @Override
    public boolean equals(Object o) {
        if (this == o) return true;
        if (!(o instanceof QueryOptions)) return false;
        QueryOptions that = (QueryOptions) o;
        return Objects.equals(appId, that.appId)
                && Objects.equals(hostName, that.hostName)
                && Objects.equals(clientIds, that.clientIds);
    }

    @Override
    public int hashCode() {
        return Objects.hash(appId, hostName, clientIds);
    }
}

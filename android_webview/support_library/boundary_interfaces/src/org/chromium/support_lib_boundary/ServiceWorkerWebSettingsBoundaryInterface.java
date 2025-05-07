// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.support_lib_boundary;

import org.jspecify.annotations.NullMarked;

import java.util.Set;

/** Boundary interface for ServiceWorkerWebSettings. */
@NullMarked
public interface ServiceWorkerWebSettingsBoundaryInterface {
    void setCacheMode(int mode);

    int getCacheMode();

    void setAllowContentAccess(boolean allow);

    boolean getAllowContentAccess();

    void setAllowFileAccess(boolean allow);

    boolean getAllowFileAccess();

    void setBlockNetworkLoads(boolean flag);

    boolean getBlockNetworkLoads();

    void setRequestedWithHeaderOriginAllowList(Set<String> allowedOriginRules);

    Set<String> getRequestedWithHeaderOriginAllowList();

    void setIncludeCookiesOnIntercept(boolean includeCookiesOnIntercept);

    boolean getIncludeCookiesOnIntercept();
}

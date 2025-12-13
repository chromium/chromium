// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.support_lib_boundary;

import org.jspecify.annotations.NullMarked;

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

    void setIncludeCookiesOnIntercept(boolean includeCookiesOnIntercept);

    boolean getIncludeCookiesOnIntercept();
}

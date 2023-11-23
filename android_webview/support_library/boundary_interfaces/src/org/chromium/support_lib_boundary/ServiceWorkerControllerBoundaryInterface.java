// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.support_lib_boundary;

import java.lang.reflect.InvocationHandler;

/** Boundary interface for ServiceWorkerController. */
public interface ServiceWorkerControllerBoundaryInterface {
    /* ServiceWorkerWebSettings */ InvocationHandler getServiceWorkerWebSettings();

    void setServiceWorkerClient(/* ServiceWorkerClient */ InvocationHandler client);
}

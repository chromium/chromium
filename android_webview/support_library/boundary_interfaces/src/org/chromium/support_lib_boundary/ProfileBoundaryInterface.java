// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.support_lib_boundary;

import android.webkit.CookieManager;
import android.webkit.GeolocationPermissions;
import android.webkit.ServiceWorkerController;
import android.webkit.WebStorage;

/** Boundary interface for Profile. */
public interface ProfileBoundaryInterface {
    String getName();

    CookieManager getCookieManager();

    WebStorage getWebStorage();

    GeolocationPermissions getGeoLocationPermissions();

    ServiceWorkerController getServiceWorkerController();
}

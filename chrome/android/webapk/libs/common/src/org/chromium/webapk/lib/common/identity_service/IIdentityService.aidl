// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webapk.lib.common.identity_service;

/** IdentityService allows browsers to query information about the WebAPK. */
interface IIdentityService {
    // Gets the package name of its runtime host browser.
    String getRuntimeHostBrowserPackageName();
}

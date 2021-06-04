// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.common.services;

interface ISafeModeService {
    // Sets the WebView SafeMode config in the WebView provider's SafeModeService. This config will
    // apply to all WebView-based apps.
    //
    // `callingPackageName` is for the caller to pass in their own package name, so that the
    //     SafeModeService can verify the caller. Only certain trusted (Google-managed) services are
    //     permitted to call this API, as determined by an allowlist. The value of
    //     callingPackageName will be verified by checking the caller's UID provided by the
    //     framework. If the caller is not trusted, SafeModeService will throw a SecurityException.
    // `actions` is a collection of Strings, each of which represents a SafeModeAction. Passing an
    //     empty list will disable SafeMode.
    void setSafeMode(String callingPackageName, in List<String> actions);
}

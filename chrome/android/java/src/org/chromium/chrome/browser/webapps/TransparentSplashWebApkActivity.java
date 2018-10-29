// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

/**
 * WebApkActivity variant to use when ShellAPK displays a splash screen.
 * TransparentSplashWebApkActivity is fully transparent while the page is
 * loading, enabling the ShellAPK's splash screen to show from underneath the
 * WebApkActivity. Once the page is loaded, the activity becomes opaque hiding
 * the splash screen underneath.
 */
public class TransparentSplashWebApkActivity extends WebApkActivity {}

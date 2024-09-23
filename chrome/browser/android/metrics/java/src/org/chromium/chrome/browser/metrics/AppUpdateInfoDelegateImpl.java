// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.metrics;

/**
 * Instantiable version of {@link AppUpdateInfoDelegate}; don't add anything to this class.
 * Downstream targets may provide a different implementation. In GN, we specify that {@link
 * AppUpdateInfoDelegate} is compiled separately from its implementation; other projects may specify
 * a different AppUpdateInfoDelegate via GN.
 */
public class AppUpdateInfoDelegateImpl extends AppUpdateInfoDelegate {}

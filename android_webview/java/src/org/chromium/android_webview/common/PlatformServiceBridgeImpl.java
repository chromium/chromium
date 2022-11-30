// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.common;

/**
 * Instantiable version of {@link PlatformServiceBridge}, don't add anything to this class!
 * Downstream targets may provide a different implementation. In GN, we specify that
 * {@link PlatformServiceBridge} is compiled separately from its implementation; other
 * projects may specify a different PlatformServiceBridgeImpl via GN.
 */
public class PlatformServiceBridgeImpl extends PlatformServiceBridge {}

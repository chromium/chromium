// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

parcelable IDeviceInfo {
  String gmsVersionCode;
  boolean isAutomotive;
  boolean isDesktop;
  boolean isFoldable;
  boolean isTv;
  int vulkanDeqpLevel;
  boolean isXr;
  // Display width is >= 600dp. At this width, we can display desktop pages instead of mobile
  boolean wasLaunchedOnLargeDisplay;
}

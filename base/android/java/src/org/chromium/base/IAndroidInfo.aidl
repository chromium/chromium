// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.base;

parcelable IAndroidInfo {
  String abiName;
  String androidBuildFp;
  String androidBuildId;
  String board;
  String brand;
  String buildType;
  String codename;
  String device;
  String hardware;
  boolean isDebugAndroid;
  String manufacturer;
  String model;
  int sdkInt;
  String securityPatch;
  // Available only on android S+. For S-, this method returns empty string.
  String socManufacturer;
  String versionIncremental;
}

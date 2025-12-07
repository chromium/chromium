// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

parcelable IApkInfo {
  /**
   * The application name (e.g. "Chrome"). For WebView, this is name of the embedding app. In the
   * context of the SDK Runtime, this is the name of the app that owns this particular instance of
   * the SDK Runtime.
   */
  String hostPackageLabel;
  /**
   * The package name of the host app which has loaded WebView, retrieved from the application
   * context. In the context of the SDK Runtime, the package name of the app that owns this
   * particular instance of the SDK Runtime will also be included. e.g.
   * com.google.android.sdksandbox:com:com.example.myappwithads
   */
  String hostPackageName;
  /**
   * By default: same as versionCode. For WebView: versionCode of the embedding app. In the
   * context of the SDK Runtime, this is the versionCode of the app that owns this particular
   * instance of the SDK Runtime.
   */
  String hostVersionCode;
  /** Result of PackageManager.getInstallerPackageName(). Never null, but may be "". */
  String installerPackageName;
  /*
   * Check if the app is declared debuggable in its manifest.
   * In WebView, this refers to the host app.
   */
  boolean isDebugApp;
  /**
   * The packageName of Chrome/WebView. Use application context for host app packageName. Same as
   * the host information within any child process.
   */
  String packageName;
  /** The value of android:versionCode */
  String packageVersionCode;
  /** The versionName of Chrome/WebView. Use application context for host app versionName. */
  String packageVersionName;
  /** Product version as stored in Android resources. */
  String resourcesVersion;
  int targetSdkVersion;
}

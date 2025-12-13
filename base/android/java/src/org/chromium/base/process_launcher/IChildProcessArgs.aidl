// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.process_launcher;

import org.chromium.base.process_launcher.IFileDescriptorInfo;
import org.chromium.base.IApkInfo;
import org.chromium.base.IAndroidInfo;
import org.chromium.base.IDeviceInfo;
import org.chromium.base.library_loader.IRelroLibInfo;

parcelable IChildProcessArgs {
  IApkInfo apkInfo;
  IAndroidInfo androidInfo;
  boolean bindToCaller;
  int channel;
  String[] commandLine;
  int cpuCount;
  long cpuFeatures;
  IDeviceInfo deviceInfo;
  IFileDescriptorInfo[] fileDescriptorInfos;
  @nullable IRelroLibInfo relroInfo;
  int libraryProcessType;
}

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
  int cpuCount;
  long cpuFeatures;
  String[] commandLine;
  IFileDescriptorInfo[] fileDescriptorInfos;
  IRelroLibInfo relroInfo;
  IApkInfo apkInfo;
  IAndroidInfo androidInfo;
  IDeviceInfo deviceInfo;
  int channel;
}

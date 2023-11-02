// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_SECCOMP_SUPPORT_DETECTOR_H_
#define CHROME_BROWSER_ANDROID_SECCOMP_SUPPORT_DETECTOR_H_

// Reports via UMA the Android kernel version and level of seccomp-bpf support.
// May block.
void ReportSeccompSupport();

#endif  // CHROME_BROWSER_ANDROID_SECCOMP_SUPPORT_DETECTOR_H_

// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROCESS_SINGLETON_INTERNAL_H_
#define CHROME_BROWSER_PROCESS_SINGLETON_INTERNAL_H_

#include "chrome/browser/process_singleton.h"

namespace internal {

void SendRemoteProcessInteractionResultHistogram(
    ProcessSingleton::RemoteProcessInteractionResult result);

void SendRemoteHungProcessTerminateReasonHistogram(
    ProcessSingleton::RemoteHungProcessTerminateReason reason);

}  // namespace internal

#endif  // CHROME_BROWSER_PROCESS_SINGLETON_INTERNAL_H_

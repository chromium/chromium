// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/shared_clipboard/feature_flags.h"

#include "base/metrics/field_trial_params.h"

const base::Feature kSharedClipboardUI{"SharedClipboardUI",
                                       base::FEATURE_DISABLED_BY_DEFAULT};

#if defined(OS_WIN) || defined(OS_MAC) || defined(OS_LINUX) || \
    defined(OS_CHROMEOS)
const base::Feature kRemoteCopyReceiver{"RemoteCopyReceiver",
                                        base::FEATURE_ENABLED_BY_DEFAULT};

const base::FeatureParam<std::string> kRemoteCopyAllowedOrigins = {
    &kRemoteCopyReceiver, "RemoteCopyAllowedOrigins",
    "https://googleusercontent.com"};

const base::Feature kRemoteCopyImageNotification{
    "RemoteCopyImageNotification", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kRemoteCopyPersistentNotification{
    "RemoteCopyPersistentNotification", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kRemoteCopyProgressNotification{
    "RemoteCopyProgressNotification", base::FEATURE_DISABLED_BY_DEFAULT};
#endif  // defined(OS_WIN) || defined(OS_MAC) || defined(OS_LINUX) ||
        // defined(OS_CHROMEOS)

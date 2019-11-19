// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/webrtc/webrtc_flags.h"

// TODO(crbug.com/1021131) - Enable by default for M80 experimentation.
const base::Feature kSharingPeerConnectionReceiver{
    "SharingPeerConnectionReceiver", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kSharingPeerConnectionSender{
    "SharingPeerConnectionSender", base::FEATURE_DISABLED_BY_DEFAULT};

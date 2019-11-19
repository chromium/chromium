// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARING_WEBRTC_WEBRTC_FLAGS_H_
#define CHROME_BROWSER_SHARING_WEBRTC_WEBRTC_FLAGS_H_

#include "base/feature_list.h"

// Feature flag to enable receiving PeerConnection requests.
extern const base::Feature kSharingPeerConnectionReceiver;

// Feature flag to enable sending SharingMessage using PeerConnection.
extern const base::Feature kSharingPeerConnectionSender;

#endif  // CHROME_BROWSER_SHARING_WEBRTC_WEBRTC_FLAGS_H_

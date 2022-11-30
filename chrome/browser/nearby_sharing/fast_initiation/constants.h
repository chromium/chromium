// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEARBY_SHARING_FAST_INITIATION_CONSTANTS_H_
#define CHROME_BROWSER_NEARBY_SHARING_FAST_INITIATION_CONSTANTS_H_

// Fast Initiation Advertisements use the service UUID 0xfe2c. The service data
// will be 0xfc128e along with 2 additional bytes of metadata at the end. These
// are used by Nearby Share when broadcasting or scanning for Fast Initiation
// advertisements.
constexpr uint8_t kFastInitiationServiceId[] = {0xfe, 0x2c};
constexpr uint8_t kFastInitiationModelId[] = {0xfc, 0x12, 0x8e};

#endif  // CHROME_BROWSER_NEARBY_SHARING_FAST_INITIATION_CONSTANTS_H_

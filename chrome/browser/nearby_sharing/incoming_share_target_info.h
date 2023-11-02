// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEARBY_SHARING_INCOMING_SHARE_TARGET_INFO_H_
#define CHROME_BROWSER_NEARBY_SHARING_INCOMING_SHARE_TARGET_INFO_H_

#include "chrome/browser/nearby_sharing/share_target_info.h"

class IncomingShareTargetInfo : public ShareTargetInfo {
 public:
  IncomingShareTargetInfo();
  IncomingShareTargetInfo(IncomingShareTargetInfo&&);
  IncomingShareTargetInfo& operator=(IncomingShareTargetInfo&&);
  ~IncomingShareTargetInfo() override;
};

#endif  // CHROME_BROWSER_NEARBY_SHARING_INCOMING_SHARE_TARGET_INFO_H_

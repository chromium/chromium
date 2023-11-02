// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARE_CORE_SHARE_TARGETS_OBSERVER_H_
#define CHROME_BROWSER_SHARE_CORE_SHARE_TARGETS_OBSERVER_H_

#include "chrome/browser/share/proto/share_target.pb.h"

namespace sharing {

class ShareTargetsObserver {
 public:
  ShareTargetsObserver() = default;
  virtual ~ShareTargetsObserver() = default;

  ShareTargetsObserver(const ShareTargetsObserver& other) = delete;
  ShareTargetsObserver& operator=(const ShareTargetsObserver& other) = delete;

  // Called when the data model is updated
  virtual void OnShareTargetsUpdated(
      std::unique_ptr<mojom::ShareTargets> ShareTarget) {}
};

}  // namespace sharing

#endif  // CHROME_BROWSER_SHARE_CORE_SHARE_TARGETS_OBSERVER_H_

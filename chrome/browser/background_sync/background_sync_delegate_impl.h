// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BACKGROUND_SYNC_BACKGROUND_SYNC_DELEGATE_IMPL_H_
#define CHROME_BROWSER_BACKGROUND_SYNC_BACKGROUND_SYNC_DELEGATE_IMPL_H_

#include "components/background_sync/background_sync_delegate.h"

class Profile;

namespace ukm {
class UkmBackgroundRecorderService;
}  // namespace ukm

namespace url {
class Origin;
}  // namespace url

// Chrome's customization of the logic in components/background_sync
class BackgroundSyncDelegateImpl
    : public background_sync::BackgroundSyncDelegate {
 public:
  explicit BackgroundSyncDelegateImpl(Profile* profile);
  ~BackgroundSyncDelegateImpl() override;

  void GetUkmSourceId(const url::Origin& origin,
                      base::OnceCallback<void(base::Optional<ukm::SourceId>)>
                          callback) override;

 private:
  ukm::UkmBackgroundRecorderService* ukm_background_service_;
};

#endif  // CHROME_BROWSER_BACKGROUND_SYNC_BACKGROUND_SYNC_DELEGATE_IMPL_H_

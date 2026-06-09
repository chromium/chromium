// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TIPS_CORE_TIPS_SERVICE_H_
#define CHROME_BROWSER_TIPS_CORE_TIPS_SERVICE_H_

#include "components/keyed_service/core/keyed_service.h"

namespace tips {

// Service to handle Tips notifications.
class TipsService : public KeyedService {
 public:
  TipsService();
  ~TipsService() override;

  TipsService(const TipsService&) = delete;
  TipsService& operator=(const TipsService&) = delete;
};

}  // namespace tips

#endif  // CHROME_BROWSER_TIPS_CORE_TIPS_SERVICE_H_

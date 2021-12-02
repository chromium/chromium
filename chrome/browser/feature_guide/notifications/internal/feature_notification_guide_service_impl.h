// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FEATURE_GUIDE_NOTIFICATIONS_INTERNAL_FEATURE_NOTIFICATION_GUIDE_SERVICE_IMPL_H_
#define CHROME_BROWSER_FEATURE_GUIDE_NOTIFICATIONS_INTERNAL_FEATURE_NOTIFICATION_GUIDE_SERVICE_IMPL_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/feature_guide/notifications/feature_notification_guide_service.h"

namespace feature_guide {

class FeatureNotificationGuideServiceImpl
    : public FeatureNotificationGuideService {
 public:
  FeatureNotificationGuideServiceImpl();
  ~FeatureNotificationGuideServiceImpl() override;

 private:
  base::WeakPtrFactory<FeatureNotificationGuideServiceImpl> weak_ptr_factory_{
      this};
};

}  // namespace feature_guide

#endif  // CHROME_BROWSER_FEATURE_GUIDE_NOTIFICATIONS_INTERNAL_FEATURE_NOTIFICATION_GUIDE_SERVICE_IMPL_H_

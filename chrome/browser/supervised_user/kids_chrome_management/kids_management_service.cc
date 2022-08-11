// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/kids_chrome_management/kids_management_service.h"

#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/browser_context.h"

namespace chrome::kids {
// Builds the service instance and its local dependencies.
// The profile dependency is needed to verify the dynamic child account status.
KeyedService* KidsManagementServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* browser_context) const {
  return new KidsManagementService();
}
}  // namespace chrome::kids

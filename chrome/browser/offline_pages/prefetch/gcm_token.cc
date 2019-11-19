// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/offline_pages/prefetch/gcm_token.h"

#include <map>

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/gcm/instance_id/instance_id_profile_service_factory.h"
#include "components/gcm_driver/instance_id/instance_id.h"
#include "components/gcm_driver/instance_id/instance_id_driver.h"
#include "components/gcm_driver/instance_id/instance_id_profile_service.h"
#include "components/offline_pages/core/offline_page_feature.h"

using instance_id::InstanceID;
using instance_id::InstanceIDProfileService;
using instance_id::InstanceIDProfileServiceFactory;

namespace {

const char kScopeGCM[] = "GCM";
const char kProdSenderId[] = "864229763856";
}  // namespace

namespace offline_pages {

void GetGCMToken(content::BrowserContext* context,
                 const std::string& app_id,
                 instance_id::InstanceID::GetTokenCallback callback) {
  DCHECK(IsPrefetchingOfflinePagesEnabled());
  // If the callback is canceled, |context| may not be alive anymore.
  if (!callback.MaybeValid())
    return;

  DCHECK(context);
  InstanceIDProfileService* service =
      InstanceIDProfileServiceFactory::GetForProfile(context);
  DCHECK(service);

  InstanceID* instance_id = service->driver()->GetInstanceID(app_id);
  if (!instance_id) {
    DLOG(ERROR) << "GetInstanceID() returned null";
    return;
  }

  instance_id->GetToken(kProdSenderId, kScopeGCM, /*options=*/{},
                        /*flags=*/{}, std::move(callback));
}

}  // namespace offline_pages

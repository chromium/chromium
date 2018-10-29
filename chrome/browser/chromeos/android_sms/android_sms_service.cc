// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/android_sms/android_sms_service.h"
#include "chrome/browser/chromeos/android_sms/android_sms_urls.h"
#include "chrome/browser/chromeos/android_sms/connection_establisher_impl.h"
#include "chrome/browser/chromeos/multidevice_setup/multidevice_setup_client_factory.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chromeos/services/multidevice_setup/public/cpp/prefs.h"
#include "components/session_manager/core/session_manager.h"
#include "content/public/browser/storage_partition.h"

using chromeos::multidevice_setup::MultiDeviceSetupClient;
using chromeos::multidevice_setup::MultiDeviceSetupClientFactory;

namespace chromeos {

namespace android_sms {

AndroidSmsService::AndroidSmsService(content::BrowserContext* browser_context)
    : browser_context_(browser_context) {
  session_manager::SessionManager::Get()->AddObserver(this);
}

AndroidSmsService::~AndroidSmsService() = default;

void AndroidSmsService::Shutdown() {
  connection_manager_.reset();
  session_manager::SessionManager::Get()->RemoveObserver(this);
}

void AndroidSmsService::OnSessionStateChanged() {
  // At most one ConnectionManager should be created.
  if (connection_manager_)
    return;

  // ConnectionManager should not be created for blocked sessions.
  if (session_manager::SessionManager::Get()->IsUserSessionBlocked())
    return;

  content::StoragePartition* storage_partition =
      content::BrowserContext::GetStoragePartitionForSite(
          browser_context_, GetAndroidMessagesURL());
  content::ServiceWorkerContext* service_worker_context =
      storage_partition->GetServiceWorkerContext();

  MultiDeviceSetupClient* multidevice_setup_client =
      MultiDeviceSetupClientFactory::GetForProfile(
          Profile::FromBrowserContext(browser_context_));
  DCHECK(multidevice_setup_client);

  connection_manager_ = std::make_unique<ConnectionManager>(
      service_worker_context, std::make_unique<ConnectionEstablisherImpl>(),
      multidevice_setup_client);
}

}  // namespace android_sms

}  // namespace chromeos

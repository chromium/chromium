// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/multi_capture_service_ash.h"

#include "ash/multi_capture/multi_capture_service_client.h"
#include "ash/shell.h"
#include "base/check_is_test.h"
#include "chrome/browser/ash/policy/multi_screen_capture/multi_screen_capture_policy_service.h"
#include "chrome/browser/ash/policy/multi_screen_capture/multi_screen_capture_policy_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "components/user_manager/user_manager.h"
#include "url/gurl.h"

namespace crosapi {

MultiCaptureServiceAsh::MultiCaptureServiceAsh() {
  if (!ash::Shell::HasInstance()) {
    CHECK_IS_TEST();
  }
}
MultiCaptureServiceAsh::~MultiCaptureServiceAsh() = default;

void MultiCaptureServiceAsh::BindReceiver(
    mojo::PendingReceiver<mojom::MultiCaptureService> receiver) {
  multi_capture_service_receiver_set_.Add(this, std::move(receiver));
}

void MultiCaptureServiceAsh::MultiCaptureStarted(const std::string& label,
                                                 const std::string& host) {
  // TODO(crbug.com/40249909): Origin cannot be used in a crosapi interface as
  // it is not stable. Currently, only the host of the origin is used. Pass the
  // complete origin when the `Origin` interface becomes stable.
  GetMultiCaptureClient()->MultiCaptureStarted(
      label, url::Origin::CreateFromNormalizedTuple(/*scheme=*/"https", host,
                                                    /*port=*/443));
}

void MultiCaptureServiceAsh::MultiCaptureStartedFromApp(
    const std::string& label,
    const std::string& app_id,
    const std::string& app_name) {
  GetMultiCaptureClient()->MultiCaptureStartedFromApp(label, app_id, app_name);
}

void MultiCaptureServiceAsh::MultiCaptureStopped(const std::string& label) {
  GetMultiCaptureClient()->MultiCaptureStopped(label);
}

ash::MultiCaptureServiceClient*
MultiCaptureServiceAsh::GetMultiCaptureClient() {
  auto* multi_capture_client =
      ash::Shell::Get()->multi_capture_service_client();
  CHECK(multi_capture_client);
  return multi_capture_client;
}

void MultiCaptureServiceAsh::IsMultiCaptureAllowed(
    const GURL& url,
    IsMultiCaptureAllowedCallback callback) {
  // This function is only called from the primary user on the Lacros side.
  content::BrowserContext* context =
      ash::BrowserContextHelper::Get()->GetBrowserContextByUser(
          user_manager::UserManager::Get()->GetPrimaryUser());
  if (!context) {
    std::move(callback).Run(false);
    return;
  }

  policy::MultiScreenCapturePolicyService* multi_capture_policy_service =
      policy::MultiScreenCapturePolicyServiceFactory::GetForBrowserContext(
          context);
  if (!multi_capture_policy_service) {
    std::move(callback).Run(false);
    return;
  }

  std::move(callback).Run(
      multi_capture_policy_service->IsMultiScreenCaptureAllowed(url));
}

void MultiCaptureServiceAsh::IsMultiCaptureAllowedForAnyOriginOnMainProfile(
    IsMultiCaptureAllowedForAnyOriginOnMainProfileCallback callback) {
  auto* context = ash::BrowserContextHelper::Get()->GetBrowserContextByUser(
      user_manager::UserManager::Get()->GetPrimaryUser());
  policy::MultiScreenCapturePolicyService* multi_capture_policy_service =
      policy::MultiScreenCapturePolicyServiceFactory::GetForBrowserContext(
          context);
  std::move(callback).Run(multi_capture_policy_service &&
                          multi_capture_policy_service->GetAllowListSize() > 0);
}

}  // namespace crosapi

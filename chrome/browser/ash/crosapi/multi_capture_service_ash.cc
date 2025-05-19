// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/multi_capture_service_ash.h"

#include "ash/multi_capture/multi_capture_service.h"
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
  NOTREACHED();
}

void MultiCaptureServiceAsh::MultiCaptureStartedFromApp(
    const std::string& label,
    const std::string& app_id,
    const std::string& app_name) {
  NOTREACHED();
}

void MultiCaptureServiceAsh::MultiCaptureStopped(const std::string& label) {
  ash::Shell::Get()->multi_capture_service()->NotifyMultiCaptureStopped(label);
}

void MultiCaptureServiceAsh::IsMultiCaptureAllowed(
    const GURL& url,
    IsMultiCaptureAllowedCallback callback) {
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

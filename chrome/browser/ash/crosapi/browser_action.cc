// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/browser_action.h"

#include <optional>
#include <string_view>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/app_restore/full_restore_service.h"
#include "chrome/browser/ash/app_restore/full_restore_service_factory.h"
#include "chrome/browser/ash/crosapi/crosapi_ash.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/ash/crosapi/desk_template_ash.h"
#include "chrome/browser/ash/floating_workspace/floating_workspace_util.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chromeos/components/kiosk/kiosk_utils.h"
#include "components/app_restore/features.h"
#include "components/user_manager/user_manager.h"
#include "ui/base/mojom/window_show_state.mojom.h"

namespace crosapi {

void BrowserAction::Cancel(crosapi::mojom::CreationResult reason) {
  DCHECK_NE(reason, mojom::CreationResult::kSuccess);
}

void BrowserAction::OnPerformed(BrowserManagerCallback on_performed,
                                mojom::CreationResult result) {
  const bool retry = result == mojom::CreationResult::kBrowserShutdown;
  std::move(on_performed).Run(retry);
}

namespace {
crosapi::mojom::OpenUrlParams_SwitchToTabPathBehavior ConvertPathBehavior(
    NavigateParams::PathBehavior path_behavior) {
  switch (path_behavior) {
    case NavigateParams::RESPECT:
      return crosapi::mojom::OpenUrlParams_SwitchToTabPathBehavior::kRespect;
    case NavigateParams::IGNORE_AND_NAVIGATE:
      return crosapi::mojom::OpenUrlParams_SwitchToTabPathBehavior::kIgnore;
  }
}
}  // namespace

class OpenUrlAction final : public BrowserAction {
 public:
  OpenUrlAction(
      const GURL& url,
      crosapi::mojom::OpenUrlParams::WindowOpenDisposition disposition,
      crosapi::mojom::OpenUrlFrom from,
      NavigateParams::PathBehavior path_behavior)
      : BrowserAction(true),
        url_(url),
        disposition_(disposition),
        from_(from),
        path_behavior_(path_behavior),
        weak_ptr_factory_(this) {}

  void Perform(const VersionedBrowserService& service,
               BrowserManagerCallback on_performed) override {
    CHECK_GE(service.interface_version,
             mojom::BrowserService::kOpenUrlMinVersion);
    auto params = crosapi::mojom::OpenUrlParams::New();
    params->disposition = disposition_;
    params->from = from_;
    params->path_behavior = ConvertPathBehavior(path_behavior_);
    service.service->OpenUrl(url_, std::move(params),
                             base::BindOnce(&OpenUrlAction::OnPerformed,
                                            weak_ptr_factory_.GetWeakPtr(),
                                            std::move(on_performed)));
  }

 private:
  const GURL url_;
  const crosapi::mojom::OpenUrlParams::WindowOpenDisposition disposition_;
  const crosapi::mojom::OpenUrlFrom from_;
  const NavigateParams::PathBehavior path_behavior_;
  base::WeakPtrFactory<OpenUrlAction> weak_ptr_factory_;
};

// static
std::unique_ptr<BrowserAction> BrowserAction::OpenUrl(
    const GURL& url,
    crosapi::mojom::OpenUrlParams::WindowOpenDisposition disposition,
    crosapi::mojom::OpenUrlFrom from,
    NavigateParams::PathBehavior path_behavior) {
  return std::make_unique<OpenUrlAction>(url, disposition, from, path_behavior);
}

}  // namespace crosapi

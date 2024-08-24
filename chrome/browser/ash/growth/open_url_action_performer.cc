// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/growth/open_url_action_performer.h"

#include <memory>

#include "ash/public/cpp/new_window_delegate.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chromeos/ash/components/growth/campaigns_logger.h"
#include "chromeos/ash/components/growth/growth_metrics.h"
#include "url/gurl.h"

namespace {

constexpr char kUrl[] = "url";
constexpr char kDisposition[] = "dispositon";

// These values are deserialized from Growth Campaign, so entries should not
// be renumbered and numeric values should never be reused.
enum class Disposition {
  kNewForegroundTab,
  kNewWindow,
  kOffTheRecord,
  kSwitchToTab,
};

struct OpenUrlParam {
  GURL url;
  ash::NewWindowDelegate::Disposition disposition;
};

ash::NewWindowDelegate::Disposition ConvertDisposition(
    Disposition disposition) {
  switch (disposition) {
    case Disposition::kNewForegroundTab:
      return ash::NewWindowDelegate::Disposition::kNewForegroundTab;
    case Disposition::kNewWindow:
      return ash::NewWindowDelegate::Disposition::kNewWindow;
    case Disposition::kOffTheRecord:
      return ash::NewWindowDelegate::Disposition::kOffTheRecord;
    case Disposition::kSwitchToTab:
      return ash::NewWindowDelegate::Disposition::kSwitchToTab;
  }
}

std::unique_ptr<OpenUrlParam> ParseOpenUrlActionPerformerParams(
    const base::Value::Dict* params) {
  if (!params) {
    CAMPAIGNS_LOG(ERROR) << "Empty parameter to OpenUrlActionPerformer.";
    return nullptr;
  }

  auto* url = params->FindString(kUrl);
  if (!url) {
    CAMPAIGNS_LOG(ERROR) << kUrl << " parameter not found.";
    return nullptr;
  }

  auto disposition = params->FindInt(kDisposition);

  auto open_url_param = std::make_unique<OpenUrlParam>();
  open_url_param->url = GURL(*url);
  open_url_param->disposition =
      ConvertDisposition(static_cast<Disposition>(disposition.value_or(0)));

  return open_url_param;
}

}  // namespace

OpenUrlActionPerformer::OpenUrlActionPerformer() = default;
OpenUrlActionPerformer::~OpenUrlActionPerformer() = default;

void OpenUrlActionPerformer::Run(int campaign_id,
                                 std::optional<int> group_id,
                                 const base::Value::Dict* params,
                                 growth::ActionPerformer::Callback callback) {
  auto open_url_param = ParseOpenUrlActionPerformerParams(params);
  if (!open_url_param) {
    growth::RecordCampaignsManagerError(
        growth::CampaignsManagerError::kOpenUrlParamsParsingFail);
    std::move(callback).Run(growth::ActionResult::kFailure,
                            growth::ActionResultReason::kParsingActionFailed);
    return;
  }

  ash::NewWindowDelegate::GetPrimary()->OpenUrl(
      open_url_param->url,
      ash::NewWindowDelegate::OpenUrlFrom::kUserInteraction,
      open_url_param->disposition);

  std::move(callback).Run(growth::ActionResult::kSuccess,
                          /*action_result_reason=*/std::nullopt);
}

growth::ActionType OpenUrlActionPerformer::ActionType() const {
  return growth::ActionType::kOpenUrl;
}

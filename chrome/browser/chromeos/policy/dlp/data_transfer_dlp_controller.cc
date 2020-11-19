// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/dlp/data_transfer_dlp_controller.h"

#include <vector>

#include "base/notreached.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/data_transfer_policy/data_transfer_endpoint.h"
#include "url/gurl.h"

namespace policy {

// static
void DataTransferDlpController::Init() {
  if (!HasInstance())
    new DataTransferDlpController();
}

bool DataTransferDlpController::IsDataReadAllowed(
    const ui::DataTransferEndpoint* const data_src,
    const ui::DataTransferEndpoint* const data_dst) {
  if (!data_src || data_src->type() == ui::EndpointType::kClipboardHistory) {
    return true;
  }

  DlpRulesManager::Level level = DlpRulesManager::Level::kAllow;

  if (!data_dst || data_dst->type() == ui::EndpointType::kDefault) {
    // Passing empty URL will return restricted if there's a rule restricting
    // the src against any dst (*), otherwise it will return ALLOW.
    level = DlpRulesManager::Get()->IsRestrictedDestination(
        data_src->origin()->GetURL(), GURL(),
        DlpRulesManager::Restriction::kClipboard);
  } else if (data_dst->IsUrlType()) {
    level = DlpRulesManager::Get()->IsRestrictedDestination(
        data_src->origin()->GetURL(), data_dst->origin()->GetURL(),
        DlpRulesManager::Restriction::kClipboard);
  } else if (data_dst->type() == ui::EndpointType::kGuestOs) {
    level = DlpRulesManager::Get()->IsRestrictedAnyOfComponents(
        data_src->origin()->GetURL(),
        std::vector<DlpRulesManager::Component>{
            DlpRulesManager::Component::kPluginVm,
            DlpRulesManager::Component::kCrostini},
        DlpRulesManager::Restriction::kClipboard);
  } else if (data_dst->type() == ui::EndpointType::kArc) {
    level = DlpRulesManager::Get()->IsRestrictedComponent(
        data_src->origin()->GetURL(), DlpRulesManager::Component::kArc,
        DlpRulesManager::Restriction::kClipboard);
  } else {
    NOTREACHED();
  }

  bool notify_on_paste = !data_dst || data_dst->notify_if_restricted();

  if (level == DlpRulesManager::Level::kBlock && notify_on_paste) {
    helper_.NotifyBlockedPaste(data_src, data_dst);
  }

  return level == DlpRulesManager::Level::kAllow;
}

DataTransferDlpController::DataTransferDlpController() = default;

DataTransferDlpController::~DataTransferDlpController() = default;

}  // namespace policy

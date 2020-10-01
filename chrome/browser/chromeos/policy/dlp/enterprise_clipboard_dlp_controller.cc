// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/dlp/enterprise_clipboard_dlp_controller.h"

#include <vector>

#include "ash/public/cpp/toast_data.h"
#include "ash/public/cpp/toast_manager.h"
#include "base/notreached.h"
#include "base/optional.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/crostini/crostini_util.h"
#include "chrome/browser/chromeos/plugin_vm/plugin_vm_util.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/clipboard_data_endpoint.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

namespace policy {

namespace {

const char kToastId[] = "clipboard_dlp_block";
constexpr int kToastDurationMs = 2500;

}  // namespace

// static
void EnterpriseClipboardDlpController::Init() {
  new EnterpriseClipboardDlpController();
}

bool EnterpriseClipboardDlpController::IsDataReadAllowed(
    const ui::ClipboardDataEndpoint* const data_src,
    const ui::ClipboardDataEndpoint* const data_dst) const {
  if (!data_src) {
    return true;
  }

  DlpRulesManager::Level level = DlpRulesManager::Level::kAllow;

  if (!data_dst) {
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

  if (level == DlpRulesManager::Level::kBlock) {
    ShowBlockToast(GetToastText(data_src, data_dst));
  }

  return level == DlpRulesManager::Level::kAllow;
}

EnterpriseClipboardDlpController::EnterpriseClipboardDlpController() = default;

EnterpriseClipboardDlpController::~EnterpriseClipboardDlpController() = default;

void EnterpriseClipboardDlpController::ShowBlockToast(
    const base::string16& text) const {
  ash::ToastData toast(kToastId, text, kToastDurationMs, base::nullopt);
  toast.is_managed = true;

  ash::ToastManager::Get()->Show(toast);
}

base::string16 EnterpriseClipboardDlpController::GetToastText(
    const ui::ClipboardDataEndpoint* const data_src,
    const ui::ClipboardDataEndpoint* const data_dst) const {
  DCHECK(data_src);
  DCHECK(data_src->origin());
  base::string16 host_name = base::UTF8ToUTF16(data_src->origin()->host());

  if (data_dst && data_dst->type() == ui::EndpointType::kGuestOs) {
    ProfileManager* profile_manager = g_browser_process->profile_manager();
    Profile* profile =
        profile_manager ? profile_manager->GetActiveUserProfile() : nullptr;

    bool is_crostini_running = crostini::IsCrostiniRunning(profile);
    bool is_plugin_vm_running = plugin_vm::IsPluginVmRunning(profile);

    if (is_crostini_running && is_plugin_vm_running) {
      return l10n_util::GetStringFUTF16(
          IDS_POLICY_DLP_CLIPBOARD_BLOCKED_ON_COPY_TWO_VMS, host_name,
          l10n_util::GetStringUTF16(IDS_CROSTINI_LINUX),
          l10n_util::GetStringUTF16(IDS_PLUGIN_VM_APP_NAME));
    } else if (is_crostini_running) {
      return l10n_util::GetStringFUTF16(
          IDS_POLICY_DLP_CLIPBOARD_BLOCKED_ON_COPY_VM, host_name,
          l10n_util::GetStringUTF16(IDS_CROSTINI_LINUX));
    } else if (is_plugin_vm_running) {
      return l10n_util::GetStringFUTF16(
          IDS_POLICY_DLP_CLIPBOARD_BLOCKED_ON_COPY_VM, host_name,
          l10n_util::GetStringUTF16(IDS_PLUGIN_VM_APP_NAME));
    } else {
      NOTREACHED();
    }
  }

  if (data_dst && data_dst->type() == ui::EndpointType::kArc) {
    return l10n_util::GetStringFUTF16(
        IDS_POLICY_DLP_CLIPBOARD_BLOCKED_ON_COPY_VM, host_name,
        l10n_util::GetStringUTF16(IDS_POLICY_DLP_ANDROID_APPS));
  }

  return l10n_util::GetStringFUTF16(IDS_POLICY_DLP_CLIPBOARD_BLOCKED_ON_PASTE,
                                    host_name);
}

}  // namespace policy

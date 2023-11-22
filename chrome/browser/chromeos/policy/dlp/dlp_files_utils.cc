// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/dlp/dlp_files_utils.h"

#include "chrome/browser/chromeos/policy/dlp/dlp_policy_constants.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager_factory.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/public/cpp/new_window_delegate.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace policy::dlp {

::dlp::DlpComponent MapPolicyComponentToProto(
    data_controls::Component component) {
  switch (component) {
    case data_controls::Component::kUnknownComponent:
      return ::dlp::DlpComponent::UNKNOWN_COMPONENT;
    case data_controls::Component::kArc:
      return ::dlp::DlpComponent::ARC;
    case data_controls::Component::kCrostini:
      return ::dlp::DlpComponent::CROSTINI;
    case data_controls::Component::kPluginVm:
      return ::dlp::DlpComponent::PLUGIN_VM;
    case data_controls::Component::kUsb:
      return ::dlp::DlpComponent::USB;
    case data_controls::Component::kDrive:
      return ::dlp::DlpComponent::GOOGLE_DRIVE;
    case data_controls::Component::kOneDrive:
      return ::dlp::DlpComponent::MICROSOFT_ONEDRIVE;
  }
}

bool IsFilesTransferBlocked(const std::vector<std::string>& sources,
                            data_controls::Component component) {
  // Primary profile restrictions are enforced across all profiles.
  policy::DlpRulesManager* rules_manager =
      policy::DlpRulesManagerFactory::GetForPrimaryProfile();
  if (!rules_manager) {
    return false;
  }

  for (const auto& src : sources) {
    // Non managed files have an empty source URL.
    if (src.empty()) {
      continue;
    }

    std::string out_src_pattern;
    policy::DlpRulesManager::RuleMetadata out_rule_metadata;
    if (rules_manager->IsRestrictedComponent(
            GURL(src), component,
            data_controls::DlpRulesManagerBase::Restriction::kFiles,
            &out_src_pattern,
            &out_rule_metadata) == policy::DlpRulesManager::Level::kBlock) {
      return true;
    }
  }

  return false;
}

void OpenLearnMore(const GURL& url) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  ash::NewWindowDelegate::GetPrimary()->OpenUrl(
      url, ash::NewWindowDelegate::OpenUrlFrom::kUserInteraction,
      ash::NewWindowDelegate::Disposition::kNewForegroundTab);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

}  // namespace policy::dlp

// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/dlp_ash.h"

#include "base/logging.h"
#include "chrome/browser/ash/crosapi/window_util.h"
#include "chrome/browser/ash/policy/dlp/dlp_content_manager_ash.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_content_restriction_set.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager.h"

namespace crosapi {

namespace {

policy::DlpRulesManager::Level ConvertMojoToDlpRulesManagerLevel(
    crosapi::mojom::DlpRestrictionLevel level) {
  switch (level) {
    case crosapi::mojom::DlpRestrictionLevel::kReport:
      return policy::DlpRulesManager::Level::kReport;
    case crosapi::mojom::DlpRestrictionLevel::kWarn:
      return policy::DlpRulesManager::Level::kWarn;
    case crosapi::mojom::DlpRestrictionLevel::kBlock:
      return policy::DlpRulesManager::Level::kBlock;
    case crosapi::mojom::DlpRestrictionLevel::kAllow:
      return policy::DlpRulesManager::Level::kAllow;
  }
}

policy::DlpContentRestrictionSet ConvertMojoToDlpContentRestrictionSet(
    const mojom::DlpRestrictionSetPtr& restrictions) {
  policy::DlpContentRestrictionSet result;
  result.SetRestriction(
      policy::DlpContentRestriction::kScreenshot,
      ConvertMojoToDlpRulesManagerLevel(restrictions->screenshot->level),
      restrictions->screenshot->url);
  result.SetRestriction(
      policy::DlpContentRestriction::kPrivacyScreen,
      ConvertMojoToDlpRulesManagerLevel(restrictions->privacy_screen->level),
      restrictions->privacy_screen->url);
  result.SetRestriction(
      policy::DlpContentRestriction::kPrint,
      ConvertMojoToDlpRulesManagerLevel(restrictions->print->level),
      restrictions->print->url);
  result.SetRestriction(
      policy::DlpContentRestriction::kScreenShare,
      ConvertMojoToDlpRulesManagerLevel(restrictions->screen_share->level),
      restrictions->screen_share->url);
  return result;
}

}  // namespace

DlpAsh::DlpAsh() = default;

DlpAsh::~DlpAsh() = default;

void DlpAsh::BindReceiver(mojo::PendingReceiver<mojom::Dlp> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void DlpAsh::DlpRestrictionsUpdated(const std::string& window_id,
                                    mojom::DlpRestrictionSetPtr restrictions) {
  aura::Window* window = GetShellSurfaceWindow(window_id);
  if (!window) {
    LOG(WARNING) << "Didn't find Lacros window with id: " << window_id;
    return;
  }
  policy::DlpContentManagerAsh* dlp_content_manager =
      policy::DlpContentManagerAsh::Get();
  DCHECK(dlp_content_manager);
  dlp_content_manager->OnWindowRestrictionChanged(
      window, ConvertMojoToDlpContentRestrictionSet(restrictions));
}

}  // namespace crosapi

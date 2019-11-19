// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/warning_badge_service.h"

#include <stddef.h>

#include <memory>

#include "base/macros.h"
#include "base/stl_util.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/extensions/warning_badge_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/global_error/global_error.h"
#include "chrome/browser/ui/global_error/global_error_service.h"
#include "chrome/browser/ui/global_error/global_error_service_factory.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/browser_thread.h"
#include "ui/base/l10n/l10n_util.h"

namespace extensions {

namespace {
// Non-modal GlobalError implementation that warns the user if extensions
// created warnings or errors. If the user clicks on the wrench menu, the user
// is redirected to chrome://extensions to inspect the errors.
class ErrorBadge : public GlobalError {
 public:
  explicit ErrorBadge(WarningBadgeService* badge_service);
  ~ErrorBadge() override;

  // Implementation for GlobalError:
  bool HasMenuItem() override;
  int MenuItemCommandID() override;
  base::string16 MenuItemLabel() override;
  void ExecuteMenuItem(Browser* browser) override;

  bool HasBubbleView() override;
  bool HasShownBubbleView() override;
  void ShowBubbleView(Browser* browser) override;
  GlobalErrorBubbleViewBase* GetBubbleView() override;

  static int GetMenuItemCommandID();

 private:
  WarningBadgeService* badge_service_;

  DISALLOW_COPY_AND_ASSIGN(ErrorBadge);
};

ErrorBadge::ErrorBadge(WarningBadgeService* badge_service)
    : badge_service_(badge_service) {
}

ErrorBadge::~ErrorBadge() {
}

bool ErrorBadge::HasMenuItem() {
  return true;
}

int ErrorBadge::MenuItemCommandID() {
  return GetMenuItemCommandID();
}

base::string16 ErrorBadge::MenuItemLabel() {
  return l10n_util::GetStringUTF16(IDS_EXTENSION_WARNINGS_WRENCH_MENU_ITEM);
}

void ErrorBadge::ExecuteMenuItem(Browser* browser) {
  // Suppress all current warnings in the extension service from triggering
  // a badge on the wrench menu in the future of this session.
  badge_service_->SuppressCurrentWarnings();

  chrome::ExecuteCommand(browser, IDC_MANAGE_EXTENSIONS);
}

bool ErrorBadge::HasBubbleView() {
  return false;
}

bool ErrorBadge::HasShownBubbleView() {
  return false;
}

void ErrorBadge::ShowBubbleView(Browser* browser) {
  NOTREACHED();
}

GlobalErrorBubbleViewBase* ErrorBadge::GetBubbleView() {
  return NULL;
}

// static
int ErrorBadge::GetMenuItemCommandID() {
  return IDC_EXTENSION_ERRORS;
}

}  // namespace

WarningBadgeService::WarningBadgeService(Profile* profile)
    : profile_(profile), warning_service_observer_(this) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  warning_service_observer_.Add(WarningService::Get(profile_));
}

WarningBadgeService::~WarningBadgeService() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

// static
WarningBadgeService* WarningBadgeService::Get(
    content::BrowserContext* context) {
  return WarningBadgeServiceFactory::GetForBrowserContext(context);
}

void WarningBadgeService::SuppressCurrentWarnings() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  size_t old_size = suppressed_warnings_.size();

  const WarningSet& warnings = GetCurrentWarnings();
  suppressed_warnings_.insert(warnings.begin(), warnings.end());

  if (old_size != suppressed_warnings_.size())
    UpdateBadgeStatus();
}

const WarningSet& WarningBadgeService::GetCurrentWarnings() const {
  return WarningService::Get(profile_)->warnings();
}

void WarningBadgeService::ExtensionWarningsChanged(
    const ExtensionIdSet& affected_extensions) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  UpdateBadgeStatus();
}

void WarningBadgeService::UpdateBadgeStatus() {
  const std::set<Warning>& warnings = GetCurrentWarnings();
  bool non_suppressed_warnings_exist = false;
  for (auto i = warnings.begin(); i != warnings.end(); ++i) {
    if (!base::Contains(suppressed_warnings_, *i)) {
      non_suppressed_warnings_exist = true;
      break;
    }
  }
  ShowBadge(non_suppressed_warnings_exist);
}

void WarningBadgeService::ShowBadge(bool show) {
  GlobalErrorService* service =
      GlobalErrorServiceFactory::GetForProfile(profile_);
  GlobalError* error = service->GetGlobalErrorByMenuItemCommandID(
      ErrorBadge::GetMenuItemCommandID());

  // Activate or hide the warning badge in case the current state is incorrect.
  if (error && !show)
    service->RemoveGlobalError(error);
  else if (!error && show)
    service->AddGlobalError(std::make_unique<ErrorBadge>(this));
}

}  // namespace extensions

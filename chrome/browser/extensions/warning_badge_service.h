// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_WARNING_BADGE_SERVICE_H_
#define CHROME_BROWSER_EXTENSIONS_WARNING_BADGE_SERVICE_H_

#include "components/keyed_service/core/keyed_service.h"
#include "extensions/browser/warning_service.h"
#include "extensions/browser/warning_set.h"
#include "extensions/common/extension_id.h"

class Profile;

namespace extensions {

// A service that is responsible for showing an extension warning badge on the
// wrench menu.
class WarningBadgeService : public KeyedService,
                            public WarningService::Observer {
 public:
  explicit WarningBadgeService(Profile* profile);
  WarningBadgeService(const WarningBadgeService&) = delete;
  WarningBadgeService& operator=(const WarningBadgeService&) = delete;
  ~WarningBadgeService() override;

  static WarningBadgeService* Get(content::BrowserContext* context);

  // Black lists all currently active extension warnings, so that they do not
  // trigger a warning badge again for the life-time of the browsing session.
  void SuppressCurrentWarnings();

 protected:
  // Virtual for testing.
  virtual const WarningSet& GetCurrentWarnings() const;

 private:
  // Implementation of WarningService::Observer.
  void ExtensionWarningsChanged(
      const ExtensionIdSet& affected_extensions) override;

  void UpdateBadgeStatus();
  virtual void ShowBadge(bool show);

  Profile* profile_;

  ScopedObserver<WarningService, WarningService::Observer>
      warning_service_observer_;

  // Warnings that do not trigger a badge on the wrench menu.
  WarningSet suppressed_warnings_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_WARNING_BADGE_SERVICE_H_

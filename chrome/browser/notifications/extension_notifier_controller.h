// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_EXTENSION_NOTIFIER_CONTROLLER_H_
#define CHROME_BROWSER_NOTIFICATIONS_EXTENSION_NOTIFIER_CONTROLLER_H_

#include "chrome/browser/notifications/notifier_controller.h"
#include "chrome/browser/ui/app_icon_loader_delegate.h"

class AppIconLoader;

// Controls extensions and apps. Each extension gets its own row in the settings
// ui.
class ExtensionNotifierController : public NotifierController,
                                    public AppIconLoaderDelegate {
 public:
  explicit ExtensionNotifierController(Observer* observer);
  ~ExtensionNotifierController() override;

  // NotifierController:
  std::vector<ash::NotifierMetadata> GetNotifierList(Profile* profile) override;
  void SetNotifierEnabled(Profile* profile,
                          const message_center::NotifierId& notifier_id,
                          bool enabled) override;

 private:
  // Overridden from AppIconLoaderDelegate.
  void OnAppImageUpdated(const std::string& id,
                         const gfx::ImageSkia& image) override;

  std::unique_ptr<AppIconLoader> app_icon_loader_;

  // Lifetime of parent must be longer than the source.
  Observer* observer_;
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_EXTENSION_NOTIFIER_CONTROLLER_H_

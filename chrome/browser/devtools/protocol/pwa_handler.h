// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_PROTOCOL_PWA_HANDLER_H_
#define CHROME_BROWSER_DEVTOOLS_PROTOCOL_PWA_HANDLER_H_

#include <memory>
#include <string>

#include "chrome/browser/devtools/protocol/pwa.h"

class Profile;

class PWAHandler final : public protocol::PWA::Backend {
 public:
  explicit PWAHandler(protocol::UberDispatcher* dispatcher,
                      const std::string& target_id);

  PWAHandler(const PWAHandler&) = delete;
  PWAHandler& operator=(const PWAHandler&) = delete;

  ~PWAHandler() override;

 private:
  void GetOsAppState(const std::string& in_manifest_id,
                     std::unique_ptr<GetOsAppStateCallback> callback) override;

  Profile* GetProfile() const;

  const std::string target_id_;
};

#endif  // CHROME_BROWSER_DEVTOOLS_PROTOCOL_PWA_HANDLER_H_

// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_ARC_SESSION_ARC_SERVICE_MANAGER_H_
#define ASH_COMPONENTS_ARC_SESSION_ARC_SERVICE_MANAGER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/threading/thread_checker.h"
#include "components/account_id/account_id.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace arc {

class ArcBridgeService;

// Holds ARC related global information. Specifically, it owns ArcBridgeService
// instance.
// TODO(hidehiko): Consider to migrate into another global ARC class, such as
// ArcSessionManager or ArcServiceLauncher.
class ArcServiceManager {
 public:
  ArcServiceManager();

  ArcServiceManager(const ArcServiceManager&) = delete;
  ArcServiceManager& operator=(const ArcServiceManager&) = delete;

  ~ArcServiceManager();

  // Returns the current BrowserContext which ARC is allowed.
  // This is workaround to split the dependency from chrome/.
  // TODO(hidehiko): Remove this when we move IsArcAllowedForProfile() to
  // components/arc.
  content::BrowserContext* browser_context() { return browser_context_; }

  // TODO(hidehiko): Remove this when we move IsArcAllowedForProfile() to
  // components/arc. See browser_context() for details.
  void set_browser_context(content::BrowserContext* browser_context) {
    browser_context_ = browser_context;
  }

  // Returns the current AccountID which ARC is allowed.
  // This is workaround to split the dependency from chrome/.
  // TODO(hidehiko): Remove this when we move IsArcAllowedForProfile() to
  // components/arc.
  const AccountId& account_id() const { return account_id_; }

  // TODO(hidehiko): Remove this when we move IsArcAllowedForProfile() to
  // components/arc.
  void set_account_id(const AccountId& account_id) { account_id_ = account_id; }

  // |arc_bridge_service| can only be accessed on the thread that this
  // class was created on.
  ArcBridgeService* arc_bridge_service();

  // Gets the global instance of the ARC Service Manager. This can only be
  // called on the thread that this class was created on.
  static ArcServiceManager* Get();

 private:
  THREAD_CHECKER(thread_checker_);

  std::unique_ptr<ArcBridgeService> arc_bridge_service_;

  // This holds the unowned pointer to the BrowserContext (practically Profile)
  // which is allowed to use ARC.
  // This is set just before BrowserContextKeyedService classes are
  // instantiated.
  // So, in BrowserContextKeyedServiceFactory::BuildServiceInstanceFor(),
  // given BrowserContext pointer can be compared to this to check if it is
  // allowed to use ARC.
  // TODO(hidehiko): Remove this when we move IsArcAllowedForProfile() to
  // components/arc. See browser_context() for details.
  raw_ptr<content::BrowserContext> browser_context_ = nullptr;

  // This holds the AccountId corresponding to the |browser_context_|.
  // TODO(hidehiko): Remove this when we move IsArcAllowedForProfile() to
  // components/arc. See browser_context() for details.
  AccountId account_id_;
};

}  // namespace arc

#endif  // ASH_COMPONENTS_ARC_SESSION_ARC_SERVICE_MANAGER_H_

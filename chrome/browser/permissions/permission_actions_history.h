// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERMISSIONS_PERMISSION_ACTIONS_HISTORY_H_
#define CHROME_BROWSER_PERMISSIONS_PERMISSION_ACTIONS_HISTORY_H_

#include "base/memory/singleton.h"
#include "base/time/time.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/permissions/permission_request_enums.h"
#include "components/permissions/permission_util.h"

namespace base {
class Time;
}  // namespace base
namespace permissions {
enum class PermissionAction;
enum class RequestType;
}  // namespace permissions
class Profile;
class PrefService;

// Helper class with utility functions to interact with permission actions
// history.
class PermissionActionsHistory : public KeyedService {
 public:
  struct Entry {
    permissions::PermissionAction action;
    base::Time time;

    bool operator==(Entry const& that) const {
      return this->action == that.action && this->time == that.time;
    }
    bool operator!=(Entry const& that) const { return !(*this == that); }
  };

  static PermissionActionsHistory* GetForProfile(Profile* profile);

  // Get the history of recorded actions that happened after a particular time.
  // Optionally a permission request type can be specified which will only
  // return actions of that type.
  std::vector<Entry> GetHistory(const base::Time& begin);
  std::vector<Entry> GetHistory(const base::Time& begin,
                                permissions::RequestType type);

  // Record that a particular action has occurred at this time.
  // `base::Time::Now()` is used to retrieve the current time.
  void RecordAction(permissions::PermissionAction action,
                    permissions::RequestType type);

  // Delete logs of past user interactions. To be called when clearing
  // browsing data.
  void ClearHistory(const base::Time& delete_begin,
                    const base::Time& delete_end);

 private:
 class Factory : public BrowserContextKeyedServiceFactory {
   public:
    static PermissionActionsHistory* GetForProfile(Profile* profile);

    static PermissionActionsHistory::Factory* GetInstance();

   private:
    friend struct base::DefaultSingletonTraits<Factory>;

    Factory();
    ~Factory() override;

    // BrowserContextKeyedServiceFactory
    KeyedService* BuildServiceInstanceFor(
        content::BrowserContext* context) const override;
    content::BrowserContext* GetBrowserContextToUse(
        content::BrowserContext* context) const override;
  };

  explicit PermissionActionsHistory(Profile* profile);

  std::vector<Entry> GetHistoryInternal(const base::Time& begin,
                                        const std::string& key);



  PrefService* pref_service_;
};

#endif  // CHROME_BROWSER_PERMISSIONS_PERMISSION_ACTIONS_HISTORY_H_

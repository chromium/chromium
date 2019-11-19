// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTENT_SETTINGS_CONTENT_SETTINGS_SUPERVISED_PROVIDER_H_
#define CHROME_BROWSER_CONTENT_SETTINGS_CONTENT_SETTINGS_SUPERVISED_PROVIDER_H_

// A content setting provider that is set by the custodian of a supervised user.

#include "base/callback_list.h"
#include "base/macros.h"
#include "base/synchronization/lock.h"
#include "components/content_settings/core/browser/content_settings_global_value_map.h"
#include "components/content_settings/core/browser/content_settings_observable_provider.h"

class SupervisedUserSettingsService;

namespace content_settings {

// SupervisedProvider that provides content-settings managed by the custodian
// of a supervised user.
class SupervisedProvider : public ObservableProvider {
 public:
  explicit SupervisedProvider(
      SupervisedUserSettingsService* supervised_user_settings_service);
  ~SupervisedProvider() override;

  // ProviderInterface implementations.
  std::unique_ptr<RuleIterator> GetRuleIterator(
      ContentSettingsType content_type,
      const ResourceIdentifier& resource_identifier,
      bool incognito) const override;

  bool SetWebsiteSetting(const ContentSettingsPattern& primary_pattern,
                         const ContentSettingsPattern& secondary_pattern,
                         ContentSettingsType content_type,
                         const ResourceIdentifier& resource_identifier,
                         std::unique_ptr<base::Value>&& value) override;

  void ClearAllContentSettingsRules(ContentSettingsType content_type) override;

  void ShutdownOnUIThread() override;

 private:
  // Callback on receiving settings from the supervised user settings service.
  void OnSupervisedSettingsAvailable(const base::DictionaryValue* settings);

  GlobalValueMap value_map_;

  // Used around accesses to the |value_map_| object to guarantee
  // thread safety.
  mutable base::Lock lock_;

  std::unique_ptr<
      base::CallbackList<void(const base::DictionaryValue*)>::Subscription>
      user_settings_subscription_;

  DISALLOW_COPY_AND_ASSIGN(SupervisedProvider);
};

}  // namespace content_settings

#endif  // CHROME_BROWSER_CONTENT_SETTINGS_CONTENT_SETTINGS_SUPERVISED_PROVIDER_H_

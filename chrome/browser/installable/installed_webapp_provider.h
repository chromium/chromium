// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_INSTALLABLE_INSTALLED_WEBAPP_PROVIDER_H_
#define CHROME_BROWSER_INSTALLABLE_INSTALLED_WEBAPP_PROVIDER_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "components/content_settings/core/browser/content_settings_observable_provider.h"
#include "components/content_settings/core/common/content_settings.h"
#include "url/gurl.h"

class InstalledWebappProvider : public content_settings::ObservableProvider {
 public:
  // Although not used in the interface of this class, RuleList is the type for
  // the underlying data that this Provider holds.
  using RuleList = std::vector<std::pair<GURL, ContentSetting>>;

  InstalledWebappProvider();
  ~InstalledWebappProvider() override;

  // ProviderInterface implementations.
  std::unique_ptr<content_settings::RuleIterator> GetRuleIterator(
      ContentSettingsType content_type,
      const content_settings::ResourceIdentifier& resource_identifier,
      bool incognito) const override;

  bool SetWebsiteSetting(
      const ContentSettingsPattern& primary_pattern,
      const ContentSettingsPattern& secondary_pattern,
      ContentSettingsType content_type,
      const content_settings::ResourceIdentifier& resource_identifier,
      std::unique_ptr<base::Value>&& value) override;

  void ClearAllContentSettingsRules(ContentSettingsType content_type) override;
  void ShutdownOnUIThread() override;

  void Notify();

 private:
  DISALLOW_COPY_AND_ASSIGN(InstalledWebappProvider);
};

#endif  // CHROME_BROWSER_INSTALLABLE_INSTALLED_WEBAPP_PROVIDER_H_

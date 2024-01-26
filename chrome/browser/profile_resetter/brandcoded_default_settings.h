// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILE_RESETTER_BRANDCODED_DEFAULT_SETTINGS_H_
#define CHROME_BROWSER_PROFILE_RESETTER_BRANDCODED_DEFAULT_SETTINGS_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/values.h"

// BrandcodedDefaultSettings provides a set of default settings
// for ProfileResetter. They are specific to Chrome distribution channels.
class BrandcodedDefaultSettings {
 public:
  BrandcodedDefaultSettings();
  // Constructs BrandcodedDefaultSettings directly from preferences.
  explicit BrandcodedDefaultSettings(const std::string& prefs);

  BrandcodedDefaultSettings(const BrandcodedDefaultSettings&) = delete;
  BrandcodedDefaultSettings& operator=(const BrandcodedDefaultSettings&) =
      delete;

  ~BrandcodedDefaultSettings();

  // The following methods return non-zero value if the default value was
  // provided for given setting.
  // After the call return_value contains a list of default engines.
  // |return_value[0]| is default one.
  std::optional<base::Value::List> GetSearchProviderOverrides() const;

  bool GetHomepage(std::string* homepage) const;
  std::optional<bool> GetHomepageIsNewTab() const;
  std::optional<bool> GetShowHomeButton() const;

  // |extension_ids| is a list of extension ids.
  bool GetExtensions(std::vector<std::string>* extension_ids) const;

  bool GetRestoreOnStartup(int* restore_on_startup) const;
  std::optional<base::Value::List> GetUrlsToRestoreOnStartup() const;

 private:
  std::optional<base::Value::List> ExtractList(const char* pref_name) const;

  base::Value::Dict master_dictionary_;
};

#endif  // CHROME_BROWSER_PROFILE_RESETTER_BRANDCODED_DEFAULT_SETTINGS_H_

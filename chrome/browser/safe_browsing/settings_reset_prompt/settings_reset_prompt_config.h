// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_SETTINGS_RESET_PROMPT_SETTINGS_RESET_PROMPT_CONFIG_H_
#define CHROME_BROWSER_SAFE_BROWSING_SETTINGS_RESET_PROMPT_SETTINGS_RESET_PROMPT_CONFIG_H_

#include <cstddef>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "base/feature_list.h"
#include "base/time/time.h"

class GURL;

namespace safe_browsing {

BASE_DECLARE_FEATURE(kSettingsResetPrompt);

// Encapsulates the state of the reset prompt experiment as well as
// associated data.
class SettingsResetPromptConfig {
 public:
  // Factory method for creating instances of SettingsResetPromptConfig.
  // Returns nullptr if |IsPromptEnabled()| is false or if something is wrong
  // with the config parameters.
  static std::unique_ptr<SettingsResetPromptConfig> Create();

  SettingsResetPromptConfig(const SettingsResetPromptConfig&) = delete;
  SettingsResetPromptConfig& operator=(const SettingsResetPromptConfig&) =
      delete;

  virtual ~SettingsResetPromptConfig();

  // Returns a non-negative integer ID if |url| should trigger a
  // settings reset prompt and a negative integer otherwise. The IDs
  // identify the domains or entities that we want to prompt the user
  // for and can be used for metrics reporting.
  virtual int UrlToResetDomainId(const GURL& url) const;

  // The delay before showing the reset prompt after Chrome startup.
  base::TimeDelta delay_before_prompt() const;
  // Integer that identifies the current prompt wave. This number will increase
  // with each new prompt wave.
  int prompt_wave() const;
  // The minimum time that must pass since the last time the prompt was shown
  // before a new prompt can be shown. Applies only to prompts shown during the
  // same prompt wave.
  base::TimeDelta time_between_prompts() const;

 protected:
  SettingsResetPromptConfig();

 private:
  using SHA256Hash = std::vector<uint8_t>;
  struct SHA256HashHasher {
    size_t operator()(const SHA256Hash& key) const;
  };
  enum ConfigError : int;

  bool Init();
  ConfigError ParseDomainHashes(const std::string& domain_hashes_json);

  // Map of 32 byte SHA256 hashes to integer domain IDs.
  std::unordered_map<SHA256Hash, int, SHA256HashHasher> domain_hashes_;

  // Other feature parameters.
  //
  // If you add any required feature parameters, make sure to update the field
  // trial testing configuration for the "SettingsResetPrompt" feature in
  // src/testing/variations/fieldtrial_testing_config.json
  base::TimeDelta delay_before_prompt_;
  int prompt_wave_ = 0;
  base::TimeDelta time_between_prompts_;
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_SETTINGS_RESET_PROMPT_SETTINGS_RESET_PROMPT_CONFIG_H_

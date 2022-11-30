// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_SETTINGS_RESET_PROMPT_SETTINGS_RESET_PROMPT_TEST_UTILS_H_
#define CHROME_BROWSER_SAFE_BROWSING_SETTINGS_RESET_PROMPT_SETTINGS_RESET_PROMPT_TEST_UTILS_H_

#include <memory>
#include <string>
#include <unordered_set>

#include "base/callback_forward.h"
#include "chrome/browser/profile_resetter/profile_resetter.h"
#include "chrome/browser/safe_browsing/settings_reset_prompt/settings_reset_prompt_config.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

class BrandcodedDefaultSettings;
class Profile;

namespace safe_browsing {

class SettingsResetPromptModel;

class MockSettingsResetPromptConfig : public SettingsResetPromptConfig {
 public:
  MockSettingsResetPromptConfig();
  ~MockSettingsResetPromptConfig() override;

  MOCK_CONST_METHOD1(UrlToResetDomainId, int(const GURL& URL));
};

class MockProfileResetter : public ProfileResetter {
 public:
  explicit MockProfileResetter(Profile* profile);
  ~MockProfileResetter() override;

  // Calls |MockReset()| defined below. Set any expectations on |MockReset|.
  // After the call to |MockReset()|, will call |callback.Run()| to simulate the
  // real |ProfileResetter|'s behaviour.
  void Reset(ProfileResetter::ResettableFlags resettable_flags,
             std::unique_ptr<BrandcodedDefaultSettings> main_settings,
             base::OnceClosure callback) override;

  MOCK_METHOD3(MockReset,
               void(ProfileResetter::ResettableFlags resettable_flags,
                    BrandcodedDefaultSettings* main_settings,
                    base::OnceClosure callback));
  MOCK_CONST_METHOD0(IsActive, bool());
};

// Returns a |SettingsResetPromptModel| with mock objects passed to the model's
// constructor as follows:
// - A |NiceMock<MockSettingsResetPromptConfig>| that will return positive
// domain IDs for each URL in |reset_urls| and negative IDs otherwise.
// - A |NiceMock<MockProfileResetter>| if |profile_resetter| is nullptr.
std::unique_ptr<SettingsResetPromptModel> CreateModelForTesting(
    Profile* profile,
    const std::unordered_set<std::string>& reset_urls,
    std::unique_ptr<ProfileResetter> profile_resetter);

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_SETTINGS_RESET_PROMPT_SETTINGS_RESET_PROMPT_TEST_UTILS_H_

// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/settings_reset_prompt/settings_reset_prompt_test_utils.h"

#include <utility>

#include "base/functional/callback.h"
#include "chrome/browser/profile_resetter/brandcoded_default_settings.h"
#include "chrome/browser/profile_resetter/resettable_settings_snapshot.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/settings_reset_prompt/settings_reset_prompt_model.h"

namespace safe_browsing {

using testing::_;
using testing::NiceMock;
using testing::Return;

MockSettingsResetPromptConfig::MockSettingsResetPromptConfig() {
  // Define a default return value of -1 for |UrlToResetDomainId()|.
  EXPECT_CALL(*this, UrlToResetDomainId(_)).WillRepeatedly(Return(-1));
}

MockSettingsResetPromptConfig::~MockSettingsResetPromptConfig() {}

MockProfileResetter::MockProfileResetter(Profile* profile)
    : ProfileResetter(profile) {}

MockProfileResetter::~MockProfileResetter() {}

void MockProfileResetter::Reset(
    ProfileResetter::ResettableFlags resettable_flags,
    std::unique_ptr<BrandcodedDefaultSettings> main_settings,
    base::OnceClosure callback) {
  MockReset(resettable_flags, main_settings.get(), base::OnceClosure());
  std::move(callback).Run();
}

std::unique_ptr<SettingsResetPromptModel> CreateModelForTesting(
    Profile* profile,
    const std::unordered_set<std::string>& reset_urls,
    std::unique_ptr<ProfileResetter> profile_resetter) {
  auto config = std::make_unique<NiceMock<MockSettingsResetPromptConfig>>();

  int id = 1;
  for (const std::string& reset_url : reset_urls) {
    EXPECT_CALL(*config, UrlToResetDomainId(GURL(reset_url)))
        .WillRepeatedly(Return(id));
    ++id;
  }

  return std::make_unique<SettingsResetPromptModel>(
      profile, std::move(config),
      profile_resetter
          ? std::move(profile_resetter)
          : std::make_unique<NiceMock<MockProfileResetter>>(profile));
}

}  // namespace safe_browsing

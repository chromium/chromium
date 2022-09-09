// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/warning_badge_service.h"

#include "base/memory/raw_ptr.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/global_error/global_error_service.h"
#include "chrome/browser/ui/global_error/global_error_service_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/browser/warning_service.h"
#include "extensions/browser/warning_set.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace {

class TestExtensionWarningSet : public WarningService {
 public:
  explicit TestExtensionWarningSet(Profile* profile)
      : WarningService(profile) {}
  ~TestExtensionWarningSet() override {}

  void AddWarning(const Warning& warning) {
    WarningSet warnings;
    warnings.insert(warning);
    AddWarnings(warnings);
  }
};

class TestWarningBadgeService : public WarningBadgeService {
 public:
  TestWarningBadgeService(Profile* profile, WarningService* warning_service)
      : WarningBadgeService(profile), warning_service_(warning_service) {}
  ~TestWarningBadgeService() override {}

  const std::set<Warning>& GetCurrentWarnings() const override {
    return warning_service_->warnings();
  }

 private:
  raw_ptr<WarningService> warning_service_;
};

bool HasBadge(Profile* profile) {
  GlobalErrorService* service =
      GlobalErrorServiceFactory::GetForProfile(profile);
  return service->GetGlobalErrorByMenuItemCommandID(IDC_EXTENSION_ERRORS) !=
         nullptr;
}

const char ext1_id[] = "extension1";
const char ext2_id[] = "extension2";

}  // namespace

// Check that no badge appears if it has been suppressed for a specific
// warning.
TEST(WarningBadgeServiceTest, SuppressBadgeForCurrentWarnings) {
  content::BrowserTaskEnvironment task_environment;
  TestingProfile profile;
  TestExtensionWarningSet warnings(&profile);
  TestWarningBadgeService badge_service(&profile, &warnings);
  warnings.AddObserver(&badge_service);

  // Insert first warning.
  warnings.AddWarning(Warning::CreateNetworkDelayWarning(ext1_id));
  EXPECT_TRUE(HasBadge(&profile));

  // Suppress first warning.
  badge_service.SuppressCurrentWarnings();
  EXPECT_FALSE(HasBadge(&profile));

  // Simulate deinstallation of extension.
  std::set<Warning::WarningType> to_clear =
      warnings.GetWarningTypesAffectingExtension(ext1_id);
  warnings.ClearWarnings(to_clear);
  EXPECT_FALSE(HasBadge(&profile));

  // Set first warning again and verify that not badge is shown this time.
  warnings.AddWarning(Warning::CreateNetworkDelayWarning(ext1_id));
  EXPECT_FALSE(HasBadge(&profile));

  // Set second warning and verify that it shows a badge.
  warnings.AddWarning(Warning::CreateRepeatedCacheFlushesWarning(ext2_id));
  EXPECT_TRUE(HasBadge(&profile));

  warnings.RemoveObserver(&badge_service);
}

}  // namespace extensions

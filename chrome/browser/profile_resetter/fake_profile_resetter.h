// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILE_RESETTER_FAKE_PROFILE_RESETTER_H_
#define CHROME_BROWSER_PROFILE_RESETTER_FAKE_PROFILE_RESETTER_H_

#include "chrome/browser/profile_resetter/profile_resetter.h"
#include "chrome/test/base/testing_profile.h"

// Fake version of ProfileResetter to be used in tests.
class FakeProfileResetter : public ProfileResetter {
 public:
  explicit FakeProfileResetter(TestingProfile* profile);

  bool IsActive() const override;

  void ResetSettings(
      ResettableFlags resettable_flags,
      std::unique_ptr<BrandcodedDefaultSettings> initial_settings,
      base::OnceClosure callback) override;

  size_t Resets() const;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  void ResetDnsConfigurations() override;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

 private:
  size_t reset_count_ = 0;
};
#endif  // CHROME_BROWSER_PROFILE_RESETTER_FAKE_PROFILE_RESETTER_H_

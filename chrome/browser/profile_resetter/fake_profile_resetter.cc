// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profile_resetter/fake_profile_resetter.h"

FakeProfileResetter::FakeProfileResetter(TestingProfile* profile)
    : ProfileResetter(profile) {}

bool FakeProfileResetter::IsActive() const {
  return false;
}

void FakeProfileResetter::ResetSettings(
    ResettableFlags resettable_flags,
    std::unique_ptr<BrandcodedDefaultSettings> initial_settings,
    base::OnceClosure callback) {
  ++reset_count_;
  std::move(callback).Run();
}

size_t FakeProfileResetter::Resets() const {
  return reset_count_;
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
void FakeProfileResetter::ResetDnsConfigurations() {}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

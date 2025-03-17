// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profile_resetter/profile_resetter_test_base.h"

#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "chrome/browser/profile_resetter/brandcoded_default_settings.h"
#include "components/keyed_service/core/service_access_type.h"

ProfileResetterMockObject::ProfileResetterMockObject() = default;

ProfileResetterMockObject::~ProfileResetterMockObject() = default;

void ProfileResetterMockObject::RunLoop() {
  EXPECT_CALL(*this, Callback());
  runner_ = new content::MessageLoopRunner;
  runner_->Run();
  runner_.reset();
}

void ProfileResetterMockObject::StopLoop() {
  DCHECK(runner_.get());
  Callback();
  runner_->Quit();
}

ProfileResetterTestBase::ProfileResetterTestBase() = default;

ProfileResetterTestBase::~ProfileResetterTestBase() = default;

void ProfileResetterTestBase::ResetAndWait(
    ProfileResetter::ResettableFlags resettable_flags) {
  std::unique_ptr<BrandcodedDefaultSettings> master_settings(
      new BrandcodedDefaultSettings);
  resetter_->ResetSettings(resettable_flags, std::move(master_settings),
                           base::BindOnce(&ProfileResetterMockObject::StopLoop,
                                          base::Unretained(&mock_object_)));
  mock_object_.RunLoop();
}

void ProfileResetterTestBase::ResetAndWait(
    ProfileResetter::ResettableFlags resettable_flags,
    const std::string& prefs) {
  std::unique_ptr<BrandcodedDefaultSettings> master_settings(
      new BrandcodedDefaultSettings(prefs));
  resetter_->ResetSettings(resettable_flags, std::move(master_settings),
                           base::BindOnce(&ProfileResetterMockObject::StopLoop,
                                          base::Unretained(&mock_object_)));
  mock_object_.RunLoop();
}

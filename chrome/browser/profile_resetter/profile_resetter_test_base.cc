// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profile_resetter/profile_resetter_test_base.h"

#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/profile_resetter/brandcoded_default_settings.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engine_choice/search_engine_choice_service_factory.h"
#include "chrome/browser/search_engines/ui_thread_search_terms_data.h"
#include "chrome/browser/webdata_services/web_data_service_factory.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/search_engines/template_url_service.h"
#include "components/search_engines/template_url_service_client.h"
#include "content/public/browser/browser_context.h"

ProfileResetterMockObject::ProfileResetterMockObject() {}

ProfileResetterMockObject::~ProfileResetterMockObject() {}

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

ProfileResetterTestBase::ProfileResetterTestBase() {}

ProfileResetterTestBase::~ProfileResetterTestBase() {}

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

std::unique_ptr<KeyedService> CreateTemplateURLServiceForTesting(
    content::BrowserContext* context) {
  Profile* profile = Profile::FromBrowserContext(context);
  return std::make_unique<TemplateURLService>(
      *profile->GetPrefs(),
      *search_engines::SearchEngineChoiceServiceFactory::GetForProfile(profile),
      std::make_unique<UIThreadSearchTermsData>(),
      WebDataServiceFactory::GetKeywordWebDataForProfile(
          profile, ServiceAccessType::EXPLICIT_ACCESS),
      nullptr /* TemplateURLServiceClient */, base::RepeatingClosure()
#if BUILDFLAG(IS_CHROMEOS_LACROS)
                                                  ,
      profile->IsMainProfile()
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
  );
}

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search_engines/template_url_service_factory_test_util.h"

#include "base/bind.h"
#include "base/run_loop.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/search_engines/template_url_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/test_utils.h"

TemplateURLServiceFactoryTestUtil::TemplateURLServiceFactoryTestUtil(
    TestingProfile* profile)
    : profile_(profile) {
  profile_->CreateWebDataService();

  TemplateURLServiceFactory::GetInstance()->SetTestingFactoryAndUse(
      profile_,
      base::BindRepeating(&TemplateURLServiceFactory::BuildInstanceFor));
}

TemplateURLServiceFactoryTestUtil::~TemplateURLServiceFactoryTestUtil() {
  // Flush the message loop to make application verifiers happy.
  content::RunAllTasksUntilIdle();
}

void TemplateURLServiceFactoryTestUtil::VerifyLoad() {
  model()->Load();
  content::RunAllTasksUntilIdle();
}

TemplateURLService* TemplateURLServiceFactoryTestUtil::model() const {
  return TemplateURLServiceFactory::GetForProfile(profile_);
}

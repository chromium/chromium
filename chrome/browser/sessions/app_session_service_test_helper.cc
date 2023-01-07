// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sessions/app_session_service_test_helper.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/app_session_service.h"
#include "chrome/browser/sessions/app_session_service_factory.h"
#include "chrome/browser/sessions/session_service_base.h"
#include "chrome/browser/sessions/session_service_base_test_helper.h"

AppSessionServiceTestHelper::AppSessionServiceTestHelper()
    : AppSessionServiceTestHelper(static_cast<AppSessionService*>(nullptr)) {}

AppSessionServiceTestHelper::AppSessionServiceTestHelper(Profile* profile)
    : AppSessionServiceTestHelper(
          AppSessionServiceFactory::GetForProfileForSessionRestore(profile)) {}

AppSessionServiceTestHelper::AppSessionServiceTestHelper(
    AppSessionService* service)
    : SessionServiceBaseTestHelper(service), app_session_service_(service) {}

void AppSessionServiceTestHelper::SetService(AppSessionService* service) {
  SetServiceBase(service);
  app_session_service_ = service;
}

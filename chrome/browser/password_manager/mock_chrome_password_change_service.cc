// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/mock_chrome_password_change_service.h"

#include "components/password_manager/core/browser/password_feature_manager.h"

MockChromePasswordChangeService::MockChromePasswordChangeService()
    : ChromePasswordChangeService(/*pref_service=*/nullptr,
                                  /*affiliation_service=*/nullptr,
                                  /*optimization_keyed_service=*/nullptr,
                                  /*settings_service=*/nullptr,
                                  /*feature_manager=*/nullptr,
                                  /*log_router=*/nullptr) {}
MockChromePasswordChangeService::~MockChromePasswordChangeService() = default;

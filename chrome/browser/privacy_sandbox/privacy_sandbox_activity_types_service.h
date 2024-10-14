// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRIVACY_SANDBOX_PRIVACY_SANDBOX_ACTIVITY_TYPES_SERVICE_H_
#define CHROME_BROWSER_PRIVACY_SANDBOX_PRIVACY_SANDBOX_ACTIVITY_TYPES_SERVICE_H_

#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_service.h"

namespace privacy_sandbox {

class PrivacySandboxActivityTypesService : public KeyedService {
 public:
  explicit PrivacySandboxActivityTypesService(PrefService* pref_service);
  ~PrivacySandboxActivityTypesService() override;

  // KeyedService:
  void Shutdown() override;

 private:
  raw_ptr<PrefService> pref_service_;
};

}  // namespace privacy_sandbox
#endif  // CHROME_BROWSER_PRIVACY_SANDBOX_PRIVACY_SANDBOX_ACTIVITY_TYPES_SERVICE_H_

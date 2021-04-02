// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_INVESTIGATOR_DEPENDENCY_PROVIDER_H_
#define CHROME_BROWSER_SIGNIN_INVESTIGATOR_DEPENDENCY_PROVIDER_H_

#include "chrome/browser/profiles/profile.h"
#include "components/prefs/pref_service.h"
#include "components/signin/core/browser/signin_investigator.h"

// This version should work for anything with a profile object, like desktop and
// Android.
class InvestigatorDependencyProvider
    : public SigninInvestigator::DependencyProvider {
 public:
  explicit InvestigatorDependencyProvider(Profile* profile);
  ~InvestigatorDependencyProvider() override;
  PrefService* GetPrefs() override;

 private:
  // Non-owning pointer.
  Profile* profile_;
  DISALLOW_COPY_AND_ASSIGN(InvestigatorDependencyProvider);
};

#endif  // CHROME_BROWSER_SIGNIN_INVESTIGATOR_DEPENDENCY_PROVIDER_H_

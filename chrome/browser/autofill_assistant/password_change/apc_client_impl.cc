// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill_assistant/password_change/apc_client_impl.h"

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/autofill_assistant/password_change/apc_onboarding_coordinator.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/autofill_assistant/password_change/assistant_display_delegate.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

ApcClientImpl::ApcClientImpl(content::WebContents* web_contents)
    : content::WebContentsUserData<ApcClientImpl>(*web_contents) {}

ApcClientImpl::~ApcClientImpl() = default;

bool ApcClientImpl::Start(const GURL& url,
                          const std::string& username,
                          bool skip_login) {
  // Ensure that only one run is ongoing.
  if (is_running_)
    return false;
  is_running_ = true;

  // The coordinator takes care of checking whether a user has previously given
  // consent and, if not, prompts the user to give consent now.
  onboarding_coordinator_ = CreateOnboardingCoordinator();
  onboarding_coordinator_->PerformOnboarding(base::BindOnce(
      &ApcClientImpl::OnOnboardingComplete, base::Unretained(this)));

  return true;
}

void ApcClientImpl::Stop() {
  onboarding_coordinator_.reset();

  is_running_ = false;
}

bool ApcClientImpl::IsRunning() const {
  return is_running_;
}

// `success` indicates whether onboarding was successful, i.e. whether consent
// has been given.
void ApcClientImpl::OnOnboardingComplete(bool success) {
  if (!success) {
    Stop();
    return;
  }

  // TODO(crbug.com/1324089): Start execution. For now, immediately mark the run
  // as complete.
  OnRunComplete();
}

void ApcClientImpl::OnRunComplete() {
  Stop();
}

std::unique_ptr<ApcOnboardingCoordinator>
ApcClientImpl::CreateOnboardingCoordinator() {
  return ApcOnboardingCoordinator::Create(&GetWebContents());
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(ApcClientImpl);

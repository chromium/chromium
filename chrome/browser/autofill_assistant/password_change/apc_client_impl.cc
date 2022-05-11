// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill_assistant/password_change/apc_client_impl.h"

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/autofill_assistant/password_change/apc_onboarding_coordinator_impl.h"
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

  // TODO(crbug.com/1322419): Obtain the side panel.
  // TODO(crbug.com/1322387): Perform onboarding
  return true;
}

void ApcClientImpl::Stop() {
  is_running_ = false;
}

bool ApcClientImpl::IsRunning() const {
  return is_running_;
}

void ApcClientImpl::OnOnboardingComplete(bool success) {}

void ApcClientImpl::OnRunComplete() {}

WEB_CONTENTS_USER_DATA_KEY_IMPL(ApcClientImpl);

ApcClient* ApcClient::GetOrCreateForWebContents(
    content::WebContents* web_contents) {
  ApcClientImpl::CreateForWebContents(web_contents);
  return ApcClientImpl::FromWebContents(web_contents);
}

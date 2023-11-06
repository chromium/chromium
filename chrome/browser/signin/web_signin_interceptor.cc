// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/web_signin_interceptor.h"

#include "components/signin/public/identity_manager/account_info.h"

ScopedWebSigninInterceptionBubbleHandle::
    ~ScopedWebSigninInterceptionBubbleHandle() = default;

bool SigninInterceptionHeuristicOutcomeIsSuccess(
    SigninInterceptionHeuristicOutcome outcome) {
  return outcome == SigninInterceptionHeuristicOutcome::kInterceptEnterprise ||
         outcome == SigninInterceptionHeuristicOutcome::kInterceptMultiUser ||
         outcome ==
             SigninInterceptionHeuristicOutcome::kInterceptProfileSwitch ||
         outcome ==
             SigninInterceptionHeuristicOutcome::kInterceptEnterpriseForced ||
         outcome == SigninInterceptionHeuristicOutcome::
                        kInterceptEnterpriseForcedProfileSwitch ||
         outcome == SigninInterceptionHeuristicOutcome::kInterceptChromeSignin;
}

WebSigninInterceptor::Delegate::BubbleParameters::BubbleParameters(
    SigninInterceptionType interception_type,
    AccountInfo intercepted_account,
    AccountInfo primary_account,
    SkColor profile_highlight_color,
    bool show_link_data_option,
    bool show_managed_disclaimer)
    : interception_type(interception_type),
      intercepted_account(intercepted_account),
      primary_account(primary_account),
      profile_highlight_color(profile_highlight_color),
      show_link_data_option(show_link_data_option),
      show_managed_disclaimer(show_managed_disclaimer) {}

WebSigninInterceptor::Delegate::BubbleParameters::BubbleParameters(
    const BubbleParameters& copy) = default;

WebSigninInterceptor::Delegate::BubbleParameters&
WebSigninInterceptor::Delegate::BubbleParameters::operator=(
    const BubbleParameters&) = default;

WebSigninInterceptor::Delegate::BubbleParameters::~BubbleParameters() = default;

WebSigninInterceptor::WebSigninInterceptor() = default;
WebSigninInterceptor::~WebSigninInterceptor() = default;

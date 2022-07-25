// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/fast_checkout/fast_checkout_client_impl.h"
#include "base/feature_list.h"
#include "chrome/browser/autofill_assistant/common_dependencies_chrome.h"
#include "chrome/browser/fast_checkout/fast_checkout_external_action_delegate.h"
#include "chrome/browser/fast_checkout/fast_checkout_features.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill_assistant/browser/public/autofill_assistant_factory.h"
#include "components/autofill_assistant/browser/public/public_script_parameters.h"
#include "content/public/browser/web_contents_user_data.h"

namespace {
constexpr char kIntentValue[] = "CHROME_FAST_CHECKOUT";
constexpr char kTrue[] = "true";
// TODO(crbug.com/1338521): Define and specify proper caller(s) and source(s).
constexpr char kCaller[] = "7";  // run was started from within Chromium
constexpr char kSource[] = "1";  // run was started organically
}  // namespace

FastCheckoutClientImpl::FastCheckoutClientImpl(
    content::WebContents* web_contents)
    : content::WebContentsUserData<FastCheckoutClientImpl>(*web_contents) {}

FastCheckoutClientImpl::~FastCheckoutClientImpl() = default;

bool FastCheckoutClientImpl::Start(const GURL& url) {
  if (!base::FeatureList::IsEnabled(features::kFastCheckout))
    return false;

  if (is_running_)
    return false;

  is_running_ = true;
  url_ = url;

  base::flat_map<std::string, std::string> params_map{
      {autofill_assistant::public_script_parameters::kIntentParameterName,
       kIntentValue},
      {autofill_assistant::public_script_parameters::
           kOriginalDeeplinkParameterName,
       url_.spec()},
      {autofill_assistant::public_script_parameters::kEnabledParameterName,
       kTrue},
      {autofill_assistant::public_script_parameters::
           kStartImmediatelyParameterName,
       kTrue},
      {autofill_assistant::public_script_parameters::kCallerParameterName,
       kCaller},
      {autofill_assistant::public_script_parameters::kSourceParameterName,
       kSource},
  };

  fast_checkout_external_action_delegate_ =
      CreateFastCheckoutExternalActionDelegate();
  external_script_controller_ = CreateHeadlessScriptController();

  external_script_controller_->StartScript(
      params_map,
      base::BindOnce(&FastCheckoutClientImpl::OnRunComplete,
                     base::Unretained(this)),
      /*use_autofill_assistant_onboarding=*/true,
      base::BindOnce(&FastCheckoutClientImpl::OnOnboardingCompletedSuccessfully,
                     base::Unretained(this)));

  return true;
}

void FastCheckoutClientImpl::OnOnboardingCompletedSuccessfully() {
  fast_checkout_controller_ = CreateFastCheckoutController();
  fast_checkout_controller_->Show();
}

void FastCheckoutClientImpl::OnRunComplete(
    autofill_assistant::HeadlessScriptController::ScriptResult result) {
  // TODO(crbug.com/1338522): Handle failed result.
  Stop();
}

void FastCheckoutClientImpl::Stop() {
  external_script_controller_.reset();
  fast_checkout_controller_.reset();
  is_running_ = false;
}

bool FastCheckoutClientImpl::IsRunning() const {
  return is_running_;
}

std::unique_ptr<FastCheckoutExternalActionDelegate>
FastCheckoutClientImpl::CreateFastCheckoutExternalActionDelegate() {
  return std::make_unique<FastCheckoutExternalActionDelegate>();
}

std::unique_ptr<FastCheckoutController>
FastCheckoutClientImpl::CreateFastCheckoutController() {
  return std::make_unique<FastCheckoutControllerImpl>(&GetWebContents(), this);
}

std::unique_ptr<autofill_assistant::HeadlessScriptController>
FastCheckoutClientImpl::CreateHeadlessScriptController() {
  std::unique_ptr<autofill_assistant::AutofillAssistant> autofill_assistant =
      autofill_assistant::AutofillAssistantFactory::CreateForBrowserContext(
          GetWebContents().GetBrowserContext(),
          std::make_unique<autofill_assistant::CommonDependenciesChrome>());
  return autofill_assistant->CreateHeadlessScriptController(
      &GetWebContents(), fast_checkout_external_action_delegate_.get());
}

void FastCheckoutClientImpl::OnOptionsSelected(
    std::unique_ptr<autofill::AutofillProfile> selected_profile,
    std::unique_ptr<autofill::CreditCard> selected_credit_card) {
  fast_checkout_external_action_delegate_->SetOptionsSelected(
      *selected_profile, *selected_credit_card);
}

void FastCheckoutClientImpl::OnDismiss() {
  Stop();
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(FastCheckoutClientImpl);

// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_change/otp_detection_helper.h"

#include "base/task/single_thread_task_runner.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/core/browser/foundations/autofill_manager.h"
#include "components/password_manager/core/browser/one_time_passwords/otp_form_manager.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "content/public/browser/web_contents.h"

namespace {

bool IsFieldStillPresent(autofill::FieldGlobalId field_id,
                         content::WebContents* web_contents) {
  autofill::ContentAutofillDriver* driver =
      autofill::ContentAutofillDriver::GetForRenderFrameHost(
          web_contents->GetPrimaryMainFrame());
  if (!driver) {
    return false;
  }
  return driver->GetAutofillManager().FindCachedFormById(field_id);
}

}  // namespace

OtpDetectionHelper::OtpDetectionHelper(
    content::WebContents* web_contents,
    password_manager::PasswordManagerClient* client,
    OtpChallengeResolvedCallback callback)
    : web_contents_(web_contents),
      client_(client),
      callback_(std::move(callback)) {
  CHECK(IsOtpPresent(web_contents, client));
  for (const auto& [form_id, otp_form_manager] :
       client_->GetOtpManager()->form_managers()) {
    if (IsFieldStillPresent(otp_form_manager->otp_field_ids().back(),
                            web_contents)) {
      // It's enough to keep track of a single OTP field inside a form.
      otp_fields_.push_back(otp_form_manager->otp_field_ids().back());
    }
  }

  // Start observing `web_contents_` for any navigation, which is used as a
  // signal to check if OTP disappeared.
  Observe(web_contents_);
  otp_observation_.Observe(client_->GetOtpManager());
}

OtpDetectionHelper::~OtpDetectionHelper() = default;

// static
bool OtpDetectionHelper::IsOtpPresent(
    content::WebContents* web_contents,
    password_manager::PasswordManagerClient* client) {
  if (!client || !client->GetOtpManager()) {
    return false;
  }

  password_manager::OtpManager* otp_manager = client->GetOtpManager();

  for (const auto& [form_id, otp_form_manager] : otp_manager->form_managers()) {
    if (IsFieldStillPresent(otp_form_manager->otp_field_ids().back(),
                            web_contents)) {
      return true;
    }
  }
  return false;
}

void OtpDetectionHelper::OnOtpFieldDetected(
    password_manager::OtpFormManager* form_manager) {
  if (IsFieldStillPresent(form_manager->otp_field_ids().back(),
                          web_contents_)) {
    otp_fields_.push_back(form_manager->otp_field_ids().back());
  }
}

void OtpDetectionHelper::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  // Erase fields which aren't present on a page anymore.
  std::erase_if(otp_fields_, [&](autofill::FieldGlobalId id) {
    return !IsFieldStillPresent(id, web_contents_);
  });

  // If no otp fields are visible on a page run callback.
  if (otp_fields_.empty()) {
    CHECK(callback_);
    std::move(callback_).Run();
  }
}

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/personal_context/first_run/chrome_personal_context_first_run_client.h"

#include "base/functional/callback.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/personal_context/first_run/android/personal_context_first_run_bottom_sheet_bridge.h"
#endif

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/views/personal_context/personal_context_notice_dialog_controller.h"
#endif  // !BUILDFLAG(IS_ANDROID)

#include "components/personal_context/first_run/personal_context_first_run_types.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"

#if !BUILDFLAG(IS_ANDROID)
namespace {
personal_context::NoticeResult ToNoticeResult(
    personal_context::NoticeDialogResult result) {
  switch (result) {
    case personal_context::NoticeDialogResult::kAcknowledged:
      return personal_context::NoticeResult::kAcknowledged;
    case personal_context::NoticeDialogResult::kDismissed:
      return personal_context::NoticeResult::kNotAcknowledged;
  }
}
}  // namespace
#endif

ChromePersonalContextFirstRunClient::ChromePersonalContextFirstRunClient() =
    default;

ChromePersonalContextFirstRunClient::~ChromePersonalContextFirstRunClient() =
    default;

void ChromePersonalContextFirstRunClient::ShowNotice(
    content::WebContents* web_contents,
    personal_context::FirstRunInvocationSource invocation_source,
    base::OnceCallback<void(personal_context::NoticeResult)> callback) {
#if !BUILDFLAG(IS_ANDROID)
  content::BrowserContext* context = web_contents->GetBrowserContext();
  auto* controller = static_cast<
      personal_context::notice::PersonalContextNoticeDialogController*>(
      context->GetUserData(
          personal_context::notice::PersonalContextNoticeDialogController::
              kUserDataKey));

  if (!controller) {
    auto new_controller = std::make_unique<
        personal_context::notice::PersonalContextNoticeDialogController>(
        context);
    controller = new_controller.get();
    context->SetUserData(
        personal_context::notice::PersonalContextNoticeDialogController::
            kUserDataKey,
        std::move(new_controller));
  }

  controller->ShowDialog(
      web_contents, base::BindOnce(&ToNoticeResult).Then(std::move(callback)));
#else
  android_bridge_ = std::make_unique<
      personal_context::PersonalContextFirstRunBottomSheetBridge>(
      web_contents,
      base::BindOnce(&ChromePersonalContextFirstRunClient::OnNoticeResult,
                     base::Unretained(this), std::move(callback)));
  android_bridge_->Show();
#endif
}

#if BUILDFLAG(IS_ANDROID)
void ChromePersonalContextFirstRunClient::OnNoticeResult(
    base::OnceCallback<void(personal_context::NoticeResult)> callback,
    personal_context::NoticeResult result) {
  std::move(callback).Run(result);
  android_bridge_.reset();
}
#endif

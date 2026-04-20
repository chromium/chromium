// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility_annotator/first_run/chrome_accessibility_annotator_first_run_client.h"

#include "base/functional/callback.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/accessibility_annotator/first_run/android/accessibility_annotator_first_run_bottom_sheet_bridge.h"
#endif

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/views/accessibility_annotator/accessibility_annotator_info_dialog_controller.h"
#endif  // !BUILDFLAG(IS_ANDROID)

#include "components/accessibility_annotator/first_run/accessibility_annotator_first_run_types.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"

#if !BUILDFLAG(IS_ANDROID)
namespace {
accessibility_annotator::InfoResult ToInfoResult(
    accessibility_annotator::InfoDialogResult result) {
  switch (result) {
    case accessibility_annotator::InfoDialogResult::kAcknowledged:
      return accessibility_annotator::InfoResult::kAcknowledged;
    case accessibility_annotator::InfoDialogResult::kDismissed:
      return accessibility_annotator::InfoResult::kNotAcknowledged;
  }
}
}  // namespace
#endif

ChromeAccessibilityAnnotatorFirstRunClient::
    ChromeAccessibilityAnnotatorFirstRunClient() = default;

ChromeAccessibilityAnnotatorFirstRunClient::
    ~ChromeAccessibilityAnnotatorFirstRunClient() = default;

void ChromeAccessibilityAnnotatorFirstRunClient::ShowRemoteAnnotatorInfo(
    content::WebContents* web_contents,
    accessibility_annotator::FirstRunInvocationSource invocation_source,
    base::OnceCallback<void(accessibility_annotator::InfoResult)> callback) {
#if !BUILDFLAG(IS_ANDROID)
  content::BrowserContext* context = web_contents->GetBrowserContext();
  auto* controller =
      static_cast<accessibility_annotator::info::
                      AccessibilityAnnotatorInfoDialogController*>(
          context->GetUserData(
              accessibility_annotator::info::
                  AccessibilityAnnotatorInfoDialogController::kUserDataKey));

  if (!controller) {
    auto new_controller =
        std::make_unique<accessibility_annotator::info::
                             AccessibilityAnnotatorInfoDialogController>(
            context);
    controller = new_controller.get();
    context->SetUserData(
        accessibility_annotator::info::
            AccessibilityAnnotatorInfoDialogController::kUserDataKey,
        std::move(new_controller));
  }

  controller->ShowDialog(
      web_contents, base::BindOnce(&ToInfoResult).Then(std::move(callback)));
#else
  android_bridge_ = std::make_unique<
      accessibility_annotator::AccessibilityAnnotatorFirstRunBottomSheetBridge>(
      web_contents,
      base::BindOnce(&ChromeAccessibilityAnnotatorFirstRunClient::
                         OnRemoteAnnotatorInfoResult,
                     base::Unretained(this), std::move(callback)));
  android_bridge_->Show();
#endif
}

#if BUILDFLAG(IS_ANDROID)
void ChromeAccessibilityAnnotatorFirstRunClient::OnRemoteAnnotatorInfoResult(
    base::OnceCallback<void(accessibility_annotator::InfoResult)> callback,
    accessibility_annotator::InfoResult result) {
  std::move(callback).Run(result);
  android_bridge_.reset();
}
#endif

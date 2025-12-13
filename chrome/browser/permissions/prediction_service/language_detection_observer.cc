// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
#include "chrome/browser/permissions/prediction_service/language_detection_observer.h"

#include "chrome/browser/translate/chrome_translate_client.h"
#include "components/permissions/permission_uma_util.h"
#include "components/translate/core/browser/language_state.h"

using ::permissions::LanguageDetectionStatus;
constexpr auto& RecordLanguageDetectionStatus =
    ::permissions::PermissionUmaUtil::RecordLanguageDetectionStatus;

namespace permissions {

LanguageDetectionObserver::LanguageDetectionObserver() = default;

LanguageDetectionObserver::~LanguageDetectionObserver() = default;

void LanguageDetectionObserver::Init(
    content::WebContents* web_contents,
    base::OnceCallback<void()> on_english_detected,
    base::OnceCallback<void()> on_fallback) {
  VLOG(1) << "[PermissionsAIv4] LanguageDetectionObserver::Init";
  web_contents_ = web_contents;
  on_english_detected_callback_ = std::move(on_english_detected);
  fallback_callback_ = std::move(on_fallback);
  std::string_view source_language =
      chrome_translate_client()->GetLanguageState().source_language();

  if (source_language.starts_with("en")) {
    VLOG(1) << "[PermissionsAIv4] LanguageDetectionObserver::Init "
               "immediately available English";
    RecordLanguageDetectionStatus(
        LanguageDetectionStatus::kImmediatelyAvailableEnglish);
    std::move(on_english_detected_callback_).Run();
  } else if (source_language.empty()) {
    VLOG(1) << "[PermissionsAIv4] LanguageDetectionObserver::Init "
               "start language detection";
    chrome_translate_client()
        ->GetTranslateDriver()
        ->AddLanguageDetectionObserver(this);
    // We start a timer here, in case language detection takes more than
    // |kLanguageDetectionTimeout| seconds. It will call the fallback callback
    // if the language detection doesn't converge during the timeout interval.
    timeout_timer_.Start(FROM_HERE, base::Seconds(kLanguageDetectionTimeout),
                         base::BindOnce(&LanguageDetectionObserver::OnTimeout,
                                        weak_ptr_factory_.GetWeakPtr()));
  } else {
    VLOG(1) << "[PermissionsAIv4] LanguageDetectionObserver::Init "
               "immediately available NOT English";
    RecordLanguageDetectionStatus(
        LanguageDetectionStatus::kImmediatelyAvailableNotEnglish);
    std::move(fallback_callback_).Run();
  }
}

void LanguageDetectionObserver::Reset() {
  RemoveAsObserver();
  timeout_timer_.Stop();
  web_contents_ = nullptr;
}
ChromeTranslateClient* LanguageDetectionObserver::chrome_translate_client() {
  return ChromeTranslateClient::FromWebContents(web_contents_);
}

void LanguageDetectionObserver::RemoveAsObserver() {
  if (web_contents_) {
    chrome_translate_client()
        ->GetTranslateDriver()
        ->RemoveLanguageDetectionObserver(this);
  }
}

void LanguageDetectionObserver::OnTimeout() {
  VLOG(1) << "[PermissionsAIv4] LanguageDetectionObserver::OnTimeout";
  RecordLanguageDetectionStatus(LanguageDetectionStatus::kNoResultDueToTimeout);
  if (on_english_detected_callback_) {
    std::move(fallback_callback_).Run();
  }
  RemoveAsObserver();
}

bool LanguageDetectionObserver::WaitingForLanguageDetection() {
  return timeout_timer_.IsRunning();
}

void LanguageDetectionObserver::OnLanguageDetermined(
    const translate::LanguageDetectionDetails& details) {
  if (details.adopted_language.starts_with("en") &&
      on_english_detected_callback_) {
    VLOG(1)
        << "[PermissionsAIv4] LanguageDetectionObserver::OnLanguageDetermined "
           "English";
    RecordLanguageDetectionStatus(
        LanguageDetectionStatus::kDelayedDetectedEnglish);
    std::move(on_english_detected_callback_).Run();
  } else if (on_english_detected_callback_) {
    VLOG(1)
        << "[PermissionsAIv4] LanguageDetectionObserver::OnLanguageDetermined "
           "NOT English";
    RecordLanguageDetectionStatus(
        LanguageDetectionStatus::kDelayedDetectedNotEnglish);
    std::move(fallback_callback_).Run();
  }
  timeout_timer_.Stop();
  RemoveAsObserver();
}
}  // namespace permissions

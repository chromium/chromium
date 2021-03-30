// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/translate/chrome_translate_client.h"

#include <memory>
#include <vector>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/notreached.h"
#include "base/path_service.h"
#include "base/strings/string_split.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/language/language_model_manager_factory.h"
#include "chrome/browser/language/url_language_histogram_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/browser/translate/translate_accept_languages_factory.h"
#include "chrome/browser/translate/translate_model_service_factory.h"
#include "chrome/browser/translate/translate_ranker_factory.h"
#include "chrome/browser/translate/translate_service.h"
#include "chrome/browser/ui/translate/translate_bubble_factory.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/theme_resources.h"
#include "components/autofill_assistant/browser/public/runtime_manager.h"
#include "components/language/core/browser/language_model_manager.h"
#include "components/language/core/browser/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/translate/content/browser/translate_model_service.h"
#include "components/translate/core/browser/language_state.h"
#include "components/translate/core/browser/page_translated_details.h"
#include "components/translate/core/browser/translate_accept_languages.h"
#include "components/translate/core/browser/translate_browser_metrics.h"
#include "components/translate/core/browser/translate_download_manager.h"
#include "components/translate/core/browser/translate_infobar_delegate.h"
#include "components/translate/core/browser/translate_manager.h"
#include "components/translate/core/browser/translate_metrics_logger.h"
#include "components/translate/core/browser/translate_prefs.h"
#include "components/translate/core/common/language_detection_details.h"
#include "components/translate/core/common/translate_util.h"
#include "components/variations/service/variations_service.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "third_party/metrics_proto/translate_event.pb.h"
#include "ui/base/ui_base_features.h"
#include "url/gurl.h"

#if defined(OS_ANDROID)
#include "chrome/browser/android/android_theme_resources.h"
#else
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#endif

namespace {
using base::FeatureList;
using metrics::TranslateEventProto;

#if !defined(OS_ANDROID)
TranslateEventProto::EventType BubbleResultToTranslateEvent(
    ShowTranslateBubbleResult result) {
  switch (result) {
    case ShowTranslateBubbleResult::BROWSER_WINDOW_NOT_VALID:
      return TranslateEventProto::BROWSER_WINDOW_IS_INVALID;
    case ShowTranslateBubbleResult::BROWSER_WINDOW_MINIMIZED:
      return TranslateEventProto::BROWSER_WINDOW_IS_MINIMIZED;
    case ShowTranslateBubbleResult::BROWSER_WINDOW_NOT_ACTIVE:
      return TranslateEventProto::BROWSER_WINDOW_NOT_ACTIVE;
    case ShowTranslateBubbleResult::WEB_CONTENTS_NOT_ACTIVE:
      return TranslateEventProto::WEB_CONTENTS_NOT_ACTIVE;
    case ShowTranslateBubbleResult::EDITABLE_FIELD_IS_ACTIVE:
      return TranslateEventProto::EDITABLE_FIELD_IS_ACTIVE;
    default:
      NOTREACHED();
      return metrics::TranslateEventProto::UNKNOWN;
  }
}
#endif

}  // namespace

ChromeTranslateClient::ChromeTranslateClient(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {
  if (translate::IsSubFrameTranslationEnabled()) {
    per_frame_translate_driver_ =
        std::make_unique<translate::PerFrameContentTranslateDriver>(
            &web_contents->GetController(),
            UrlLanguageHistogramFactory::GetForBrowserContext(
                web_contents->GetBrowserContext()));
  } else {
    translate_driver_ = std::make_unique<translate::ContentTranslateDriver>(
        &web_contents->GetController(),
        UrlLanguageHistogramFactory::GetForBrowserContext(
            web_contents->GetBrowserContext()),
        TranslateModelServiceFactory::GetOrBuildForKey(
            Profile::FromBrowserContext(web_contents->GetBrowserContext())
                ->GetProfileKey()));
  }
  translate_manager_ = std::make_unique<translate::TranslateManager>(
      this,
      translate::TranslateRankerFactory::GetForBrowserContext(
          web_contents->GetBrowserContext()),
      LanguageModelManagerFactory::GetForBrowserContext(
          web_contents->GetBrowserContext())
          ->GetPrimaryModel());
  if (translate_driver_) {
    translate_driver_->AddLanguageDetectionObserver(this);
    translate_driver_->set_translate_manager(translate_manager_.get());
  }
  if (per_frame_translate_driver_) {
    per_frame_translate_driver_->AddLanguageDetectionObserver(this);
    per_frame_translate_driver_->set_translate_manager(
        translate_manager_.get());
  }

  auto* assistant_runtime_manager =
      autofill_assistant::RuntimeManager::GetOrCreateForWebContents(
          web_contents);
  assistant_runtime_manager->AddObserver(this);
}

ChromeTranslateClient::~ChromeTranslateClient() {
  if (translate_driver_) {
    translate_driver_->RemoveLanguageDetectionObserver(this);
    translate_driver_->set_translate_manager(nullptr);
  }
  if (per_frame_translate_driver_) {
    per_frame_translate_driver_->RemoveLanguageDetectionObserver(this);
    per_frame_translate_driver_->set_translate_manager(nullptr);
  }
}

const translate::LanguageState& ChromeTranslateClient::GetLanguageState() {
  return *translate_manager_->GetLanguageState();
}

translate::ContentTranslateDriver* ChromeTranslateClient::translate_driver() {
  if (translate_driver_) {
    DCHECK(!translate::IsSubFrameTranslationEnabled());
    return translate_driver_.get();
  }

  return per_frame_translate_driver();
}

translate::PerFrameContentTranslateDriver*
ChromeTranslateClient::per_frame_translate_driver() {
  DCHECK(translate::IsSubFrameTranslationEnabled());
  return per_frame_translate_driver_.get();
}

// static
std::unique_ptr<translate::TranslatePrefs>
ChromeTranslateClient::CreateTranslatePrefs(PrefService* prefs) {
  std::unique_ptr<translate::TranslatePrefs> translate_prefs(
      new translate::TranslatePrefs(prefs));

  // We need to obtain the country here, since it comes from VariationsService.
  // components/ does not have access to that.
  DCHECK(g_browser_process);
  variations::VariationsService* variations_service =
      g_browser_process->variations_service();
  if (variations_service) {
    translate_prefs->SetCountry(
        variations_service->GetStoredPermanentCountry());
  }

  return translate_prefs;
}

// static
translate::TranslateAcceptLanguages*
ChromeTranslateClient::GetTranslateAcceptLanguages(
    content::BrowserContext* browser_context) {
  return TranslateAcceptLanguagesFactory::GetForBrowserContext(browser_context);
}

// static
translate::TranslateManager* ChromeTranslateClient::GetManagerFromWebContents(
    content::WebContents* web_contents) {
  ChromeTranslateClient* chrome_translate_client =
      FromWebContents(web_contents);
  if (!chrome_translate_client)
    return NULL;
  return chrome_translate_client->GetTranslateManager();
}

void ChromeTranslateClient::GetTranslateLanguages(
    content::WebContents* web_contents,
    std::string* source,
    std::string* target) {
  DCHECK(source != NULL);
  DCHECK(target != NULL);

  *source = translate::TranslateDownloadManager::GetLanguageCode(
      GetLanguageState().original_language());

  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  std::unique_ptr<translate::TranslatePrefs> translate_prefs =
      CreateTranslatePrefs(profile->GetPrefs());
  if (!profile->IsOffTheRecord()) {
    std::string auto_translate_language =
        translate::TranslateManager::GetAutoTargetLanguage(
            *source, translate_prefs.get());
    if (!auto_translate_language.empty()) {
      *target = auto_translate_language;
      return;
    }
  }

  *target = translate::TranslateManager::GetTargetLanguage(
      translate_prefs.get(), LanguageModelManagerFactory::GetInstance()
                                 ->GetForBrowserContext(profile)
                                 ->GetPrimaryModel());
}

translate::TranslateManager* ChromeTranslateClient::GetTranslateManager() {
  return translate_manager_.get();
}

bool ChromeTranslateClient::ShowTranslateUI(
    translate::TranslateStep step,
    const std::string& source_language,
    const std::string& target_language,
    translate::TranslateErrors::Type error_type,
    bool triggered_from_menu) {
  DCHECK(web_contents());
  DCHECK(translate_manager_);

  if (error_type != translate::TranslateErrors::NONE)
    step = translate::TRANSLATE_STEP_TRANSLATE_ERROR;

// Translate uses a bubble UI on desktop and an infobar on Android (here)
// and iOS (in ios/chrome/browser/translate/chrome_ios_translate_client.mm).
#if defined(OS_ANDROID)
  // Infobar UI.
  DCHECK(!TranslateService::IsTranslateBubbleEnabled());
  translate::TranslateInfoBarDelegate::Create(
      step != translate::TRANSLATE_STEP_BEFORE_TRANSLATE,
      translate_manager_->GetWeakPtr(),
      InfoBarService::FromWebContents(web_contents()),
      web_contents()->GetBrowserContext()->IsOffTheRecord(), step,
      source_language, target_language, error_type, triggered_from_menu);

  translate_manager_->GetActiveTranslateMetricsLogger()->LogUIChange(true);
#else
  DCHECK(TranslateService::IsTranslateBubbleEnabled());
  // Bubble UI.
  if (step == translate::TRANSLATE_STEP_BEFORE_TRANSLATE &&
      translate_manager_->ShouldSuppressBubbleUI(triggered_from_menu,
                                                 source_language)) {
    return false;
  }

  ShowTranslateBubbleResult result = ShowBubble(
      step, source_language, target_language, error_type, triggered_from_menu);
  if (result != ShowTranslateBubbleResult::SUCCESS &&
      step == translate::TRANSLATE_STEP_BEFORE_TRANSLATE) {
    translate_manager_->RecordTranslateEvent(
        BubbleResultToTranslateEvent(result));
  }
#endif

  return true;
}

translate::TranslateDriver* ChromeTranslateClient::GetTranslateDriver() {
  return translate_driver();
}

PrefService* ChromeTranslateClient::GetPrefs() {
  DCHECK(web_contents());
  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  return profile->GetPrefs();
}

std::unique_ptr<translate::TranslatePrefs>
ChromeTranslateClient::GetTranslatePrefs() {
  return CreateTranslatePrefs(GetPrefs());
}

translate::TranslateAcceptLanguages*
ChromeTranslateClient::GetTranslateAcceptLanguages() {
  DCHECK(web_contents());
  return GetTranslateAcceptLanguages(web_contents()->GetBrowserContext());
}

#if defined(OS_ANDROID)
int ChromeTranslateClient::GetInfobarIconID() const {
  return IDR_ANDROID_INFOBAR_TRANSLATE;
}

void ChromeTranslateClient::ManualTranslateWhenReady() {
  if (GetLanguageState().original_language().empty()) {
    manual_translate_on_ready_ = true;
  } else {
    translate::TranslateManager* manager = GetTranslateManager();
    manager->InitiateManualTranslation(/*auto_translate=*/true,
                                       /*triggered_from_menu=*/true);
  }
}
#endif

void ChromeTranslateClient::SetPredefinedTargetLanguage(
    const std::string& translate_language_code) {
  translate::TranslateManager* manager = GetTranslateManager();
  manager->SetPredefinedTargetLanguage(translate_language_code);
}

bool ChromeTranslateClient::IsTranslatableURL(const GURL& url) {
  return TranslateService::IsTranslatableURL(url);
}

bool ChromeTranslateClient::IsAutofillAssistantRunning() const {
  auto* assistant_runtime_manager =
      autofill_assistant::RuntimeManager::GetForWebContents(web_contents());
  return assistant_runtime_manager && assistant_runtime_manager->GetState() ==
                                          autofill_assistant::UIState::kShown;
}

void ChromeTranslateClient::OnStateChanged(autofill_assistant::UIState state) {
  if (state == autofill_assistant::UIState::kNotShown) {
    GetTranslateManager()->OnAutofillAssistantFinished();
  }
}

void ChromeTranslateClient::ShowReportLanguageDetectionErrorUI(
    const GURL& report_url) {
#if defined(OS_ANDROID)
  // Android does not support reporting language detection errors.
  NOTREACHED();
#else
  // We'll open the URL in a new tab so that the user can tell us more.
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents());
  if (!browser) {
    NOTREACHED();
    return;
  }

  chrome::AddSelectedTabWithURL(browser, report_url,
                                ui::PAGE_TRANSITION_AUTO_BOOKMARK);
#endif  // defined(OS_ANDROID)
}

void ChromeTranslateClient::WebContentsDestroyed() {
  // Translation process can be interrupted.
  // Destroying the TranslateManager now guarantees that it never has to deal
  // with NULL WebContents.
  translate_manager_.reset();

  auto* assistant_runtime_manager =
      autofill_assistant::RuntimeManager::GetForWebContents(web_contents());
  if (assistant_runtime_manager) {
    assistant_runtime_manager->RemoveObserver(this);
  }
}

// TranslateDriver::LanguageDetectionObserver implementation.

void ChromeTranslateClient::OnLanguageDetermined(
    const translate::LanguageDetectionDetails& details) {
  translate::TranslateBrowserMetrics::ReportLanguageDetectionContentLength(
      details.contents.size());

  if (!web_contents()->GetBrowserContext()->IsOffTheRecord() &&
      IsTranslatableURL(details.url)) {
    GetTranslateManager()->NotifyLanguageDetected(details);
  }

#if defined(OS_ANDROID)
  // See ChromeTranslateClient::ManualTranslateOnReady
  if (manual_translate_on_ready_) {
    GetTranslateManager()->InitiateManualTranslation(true);
    manual_translate_on_ready_ = false;
  }
#endif
}

// The bubble is implemented only on the desktop platforms.
#if !defined(OS_ANDROID)
ShowTranslateBubbleResult ChromeTranslateClient::ShowBubble(
    translate::TranslateStep step,
    const std::string& source_language,
    const std::string& target_language,
    translate::TranslateErrors::Type error_type,
    bool is_user_gesture) {
  DCHECK(translate_manager_);
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents());

  // |browser| might be NULL when testing. In this case, Show(...) should be
  // called because the implementation for testing is used.
  if (!browser) {
    return TranslateBubbleFactory::Show(NULL, web_contents(), step,
                                        source_language, target_language,
                                        error_type, is_user_gesture);
  }

  if (web_contents() != browser->tab_strip_model()->GetActiveWebContents())
    return ShowTranslateBubbleResult::WEB_CONTENTS_NOT_ACTIVE;

  // This ShowBubble function is also used for updating the existing bubble.
  // However, with the bubble shown, any browser windows are NOT activated
  // because the bubble takes the focus from the other widgets including the
  // browser windows. So it is checked that |browser| is the last activated
  // browser, not is now activated.
  if (browser != chrome::FindLastActive())
    return ShowTranslateBubbleResult::BROWSER_WINDOW_NOT_ACTIVE;

  // During auto-translating, the bubble should not be shown.
  if (!is_user_gesture && (step == translate::TRANSLATE_STEP_TRANSLATING ||
                           step == translate::TRANSLATE_STEP_AFTER_TRANSLATE)) {
    if (GetLanguageState().InTranslateNavigation())
      return ShowTranslateBubbleResult::SUCCESS;
  }

  return TranslateBubbleFactory::Show(browser->window(), web_contents(), step,
                                      source_language, target_language,
                                      error_type, is_user_gesture);
}
#endif

WEB_CONTENTS_USER_DATA_KEY_IMPL(ChromeTranslateClient)

// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/translate/chrome_translate_client.h"

#include <memory>
#include <vector>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/notreached.h"
#include "base/path_service.h"
#include "base/strings/string_split.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/language/accept_languages_service_factory.h"
#include "chrome/browser/language/language_model_manager_factory.h"
#include "chrome/browser/language/url_language_histogram_factory.h"
#include "chrome/browser/language_detection/language_detection_model_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/browser/translate/translate_ranker_factory.h"
#include "chrome/browser/translate/translate_service.h"
#include "chrome/browser/ui/translate/translate_bubble_factory.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/theme_resources.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/language/core/browser/accept_languages_service.h"
#include "components/language/core/browser/language_model_manager.h"
#include "components/language/core/browser/pref_names.h"
#include "components/language_detection/content/browser/content_language_detection_driver.h"
#include "components/language_detection/core/browser/language_detection_model_service.h"
#include "components/prefs/pref_service.h"
#include "components/translate/content/browser/content_translate_driver.h"
#include "components/translate/core/browser/language_state.h"
#include "components/translate/core/browser/page_translated_details.h"
#include "components/translate/core/browser/translate_browser_metrics.h"
#include "components/translate/core/browser/translate_download_manager.h"
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

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/android/android_theme_resources.h"
#include "chrome/browser/translate/android/auto_translate_snackbar_controller.h"
#include "components/translate/content/android/translate_message.h"
#include "content/public/browser/page.h"
#include "content/public/browser/visibility.h"
#else
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#endif

namespace {
using metrics::TranslateEventProto;

#if !BUILDFLAG(IS_ANDROID)
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
      NOTREACHED_IN_MIGRATION();
      return metrics::TranslateEventProto::UNKNOWN;
  }
}
#endif

#if BUILDFLAG(IS_ANDROID)
// helper function for use in ChromeTranslateClient::ShowTranslateUI.
bool IsAutomaticTranslationType(translate::TranslationType type) {
  return type == translate::TranslationType::kAutomaticTranslationByHref ||
         type == translate::TranslationType::kAutomaticTranslationByLink ||
         type == translate::TranslationType::kAutomaticTranslationByPref ||
         type == translate::TranslationType::
                     kAutomaticTranslationToPredefinedTarget;
}
#endif

}  // namespace

ChromeTranslateClient::ChromeTranslateClient(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<ChromeTranslateClient>(*web_contents),
      translate_driver_(new translate::ContentTranslateDriver(
          *web_contents,
          UrlLanguageHistogramFactory::GetForBrowserContext(
              web_contents->GetBrowserContext()))),
      language_detection_driver_(
          new language_detection::ContentLanguageDetectionDriver(
              LanguageDetectionModelServiceFactory::GetForProfile(
                  Profile::FromBrowserContext(
                      web_contents->GetBrowserContext())))),
      translate_manager_(new translate::TranslateManager(
          this,
          translate::TranslateRankerFactory::GetForBrowserContext(
              web_contents->GetBrowserContext()),
          LanguageModelManagerFactory::GetForBrowserContext(
              web_contents->GetBrowserContext())
              ->GetPrimaryModel())) {
  translate_driver_->AddLanguageDetectionObserver(this);
  translate_driver_->set_translate_manager(translate_manager_.get());
}

ChromeTranslateClient::~ChromeTranslateClient() {
  translate_driver_->RemoveLanguageDetectionObserver(this);
  translate_driver_->set_translate_manager(nullptr);
}

const translate::LanguageState& ChromeTranslateClient::GetLanguageState() {
  return *translate_manager_->GetLanguageState();
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
language::AcceptLanguagesService*
ChromeTranslateClient::GetAcceptLanguagesService(
    content::BrowserContext* browser_context) {
  return AcceptLanguagesServiceFactory::GetForBrowserContext(browser_context);
}

// static
translate::TranslateManager* ChromeTranslateClient::GetManagerFromWebContents(
    content::WebContents* web_contents) {
  ChromeTranslateClient* chrome_translate_client =
      FromWebContents(web_contents);
  if (!chrome_translate_client) {
    return nullptr;
  }
  return chrome_translate_client->GetTranslateManager();
}

void ChromeTranslateClient::GetTranslateLanguages(
    content::WebContents* web_contents,
    std::string* source,
    std::string* target,
    bool for_display) {
  DCHECK(source != nullptr);
  DCHECK(target != nullptr);

  *source = translate::TranslateDownloadManager::GetLanguageCode(
      GetLanguageState().source_language());

  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  std::unique_ptr<translate::TranslatePrefs> translate_prefs =
      CreateTranslatePrefs(profile->GetPrefs());
  if (for_display) {
    *target = translate_manager_->GetTargetLanguageForDisplay(
        translate_prefs.get(), LanguageModelManagerFactory::GetInstance()
                                   ->GetForBrowserContext(profile)
                                   ->GetPrimaryModel());
  } else {
    *target = translate_manager_->GetTargetLanguage(
        translate_prefs.get(), LanguageModelManagerFactory::GetInstance()
                                   ->GetForBrowserContext(profile)
                                   ->GetPrimaryModel());
  }
}

translate::TranslateManager* ChromeTranslateClient::GetTranslateManager() {
  return translate_manager_.get();
}

bool ChromeTranslateClient::ShowTranslateUI(
    translate::TranslateStep step,
    const std::string& source_language,
    const std::string& target_language,
    translate::TranslateErrors error_type,
    bool triggered_from_menu) {
  DCHECK(web_contents());
  DCHECK(translate_manager_);

  if (error_type != translate::TranslateErrors::NONE) {
    step = translate::TRANSLATE_STEP_TRANSLATE_ERROR;
  }

// Translate uses a bubble UI on desktop and the Message UI on Android (here)
// and iOS (in ios/chrome/browser/translate/chrome_ios_translate_client.mm).
#if BUILDFLAG(IS_ANDROID)
  DCHECK(!TranslateService::IsTranslateBubbleEnabled());
    // Message UI.
    translate::TranslationType translate_type =
        GetLanguageState().translation_type();
    // Use the automatic translation Snackbar if the current translation is an
    // automatic translation and there was no error.
    if (IsAutomaticTranslationType(translate_type) &&
        step != translate::TRANSLATE_STEP_TRANSLATE_ERROR) {
      // The Automatic translation snackbar is only shown after translation
      // has completed. The translating step is a no-op with the Snackbar.
      if (step == translate::TRANSLATE_STEP_AFTER_TRANSLATE) {
        // An automatic translation has completed show the snackbar.
        if (!auto_translate_snackbar_controller_) {
          auto_translate_snackbar_controller_ =
              std::make_unique<translate::AutoTranslateSnackbarController>(
                  web_contents(), translate_manager_->GetWeakPtr());
        }
        auto_translate_snackbar_controller_->ShowSnackbar(target_language);
      }
    } else {
      // Not an automatic translation. Use TranslateMessage instead.
      if (!translate_message_) {
        translate_message_ = std::make_unique<translate::TranslateMessage>(
            web_contents(), translate_manager_->GetWeakPtr(),
            base::BindRepeating([]() {}));
      }
      translate_message_->ShowTranslateStep(step, source_language,
                                            target_language);
    }
  translate_manager_->GetActiveTranslateMetricsLogger()->LogUIChange(true);
#else
  DCHECK(TranslateService::IsTranslateBubbleEnabled());
  // Bubble UI.
  if (step == translate::TRANSLATE_STEP_BEFORE_TRANSLATE &&
      translate_manager_->ShouldSuppressBubbleUI(target_language)) {
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
  return translate_driver_.get();
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

language::AcceptLanguagesService*
ChromeTranslateClient::GetAcceptLanguagesService() {
  DCHECK(web_contents());
  return GetAcceptLanguagesService(web_contents()->GetBrowserContext());
}

#if BUILDFLAG(IS_ANDROID)
void ChromeTranslateClient::ManualTranslateWhenReady() {
  if (GetLanguageState().source_language().empty()) {
    manual_translate_on_ready_ = true;
  } else {
    translate::TranslateManager* manager = GetTranslateManager();
    manager->ShowTranslateUI(/*auto_translate=*/true,
                             /*triggered_from_menu=*/true);
  }
}
#endif

void ChromeTranslateClient::SetPredefinedTargetLanguage(
    const std::string& translate_language_code,
    bool should_auto_translate) {
  translate::TranslateManager* manager = GetTranslateManager();
  manager->SetPredefinedTargetLanguage(translate_language_code,
                                       should_auto_translate);
}

bool ChromeTranslateClient::IsTranslatableURL(const GURL& url) {
  return TranslateService::IsTranslatableURL(url);
}

// content::WebContentsObserver implementation.
void ChromeTranslateClient::WebContentsDestroyed() {
  // Translation process can be interrupted.
  // Destroying the TranslateManager now guarantees that it never has to deal
  // with NULL WebContents.
  if (translate_manager_) {
    if (translate_driver_) {
      translate_driver_->set_translate_manager(nullptr);
    }
    translate_manager_.reset();
  }
}

#if BUILDFLAG(IS_ANDROID)
void ChromeTranslateClient::PrimaryPageChanged(content::Page& page) {
  if (auto_translate_snackbar_controller_ &&
      auto_translate_snackbar_controller_->IsShowing()) {
    auto_translate_snackbar_controller_->NativeDismissSnackbar();
  }
}

void ChromeTranslateClient::OnVisibilityChanged(
    content::Visibility visibility) {
  if (auto_translate_snackbar_controller_ &&
      auto_translate_snackbar_controller_->IsShowing() &&
      visibility == content::Visibility::HIDDEN) {
    auto_translate_snackbar_controller_->NativeDismissSnackbar();
  }
}
#endif  // IS_ANDROID

// TranslateDriver::LanguageDetectionObserver implementation.
void ChromeTranslateClient::OnLanguageDetermined(
    const translate::LanguageDetectionDetails& details) {
  if (details.has_run_lang_detection) {
    translate::TranslateBrowserMetrics::ReportLanguageDetectionContentLength(
        details.contents.size());
  }

  if (!web_contents()->GetBrowserContext()->IsOffTheRecord() &&
      IsTranslatableURL(details.url)) {
    GetTranslateManager()->NotifyLanguageDetected(details);
  }

#if BUILDFLAG(IS_ANDROID)
  // See ChromeTranslateClient::ManualTranslateOnReady
  if (manual_translate_on_ready_) {
    GetTranslateManager()->ShowTranslateUI(/*auto_translate=*/true);
    manual_translate_on_ready_ = false;
  }
#endif
}

// The bubble is implemented only on the desktop platforms.
#if !BUILDFLAG(IS_ANDROID)
ShowTranslateBubbleResult ChromeTranslateClient::ShowBubble(
    translate::TranslateStep step,
    const std::string& source_language,
    const std::string& target_language,
    translate::TranslateErrors error_type,
    bool is_user_gesture) {
  DCHECK(translate_manager_);
  Browser* browser = chrome::FindBrowserWithTab(web_contents());

  // |browser| might be NULL when testing. In this case, Show(...) should be
  // called because the implementation for testing is used.
  if (!browser) {
    return TranslateBubbleFactory::Show(nullptr, web_contents(), step,
                                        source_language, target_language,
                                        error_type, is_user_gesture);
  }

  if (web_contents() != browser->tab_strip_model()->GetActiveWebContents()) {
    return ShowTranslateBubbleResult::WEB_CONTENTS_NOT_ACTIVE;
  }

  // This ShowBubble function is also used for updating the existing bubble.
  // However, with the bubble shown, any browser windows are NOT activated
  // because the bubble takes the focus from the other widgets including the
  // browser windows. So it is checked that |browser| is the last activated
  // browser, not is now activated.
  if (browser != chrome::FindLastActive()) {
    return ShowTranslateBubbleResult::BROWSER_WINDOW_NOT_ACTIVE;
  }

  // During auto-translating, the bubble should not be shown.
  if (!is_user_gesture && (step == translate::TRANSLATE_STEP_TRANSLATING ||
                           step == translate::TRANSLATE_STEP_AFTER_TRANSLATE)) {
    if (GetLanguageState().InTranslateNavigation()) {
      return ShowTranslateBubbleResult::SUCCESS;
    }
  }

  return TranslateBubbleFactory::Show(browser->window(), web_contents(), step,
                                      source_language, target_language,
                                      error_type, is_user_gesture);
}
#endif

WEB_CONTENTS_USER_DATA_KEY_IMPL(ChromeTranslateClient);

// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TRANSLATE_CHROME_TRANSLATE_CLIENT_H_
#define CHROME_BROWSER_TRANSLATE_CHROME_TRANSLATE_CLIENT_H_

#include <memory>
#include <string>

#include "base/feature_list.h"
#include "base/gtest_prod_util.h"
#include "build/build_config.h"
#include "components/language/core/browser/accept_languages_service.h"
#include "components/language/core/browser/url_language_histogram.h"
#include "components/language_detection/content/browser/content_language_detection_driver.h"
#include "components/translate/content/browser/content_translate_driver.h"
#include "components/translate/core/browser/translate_client.h"
#include "components/translate/core/browser/translate_step.h"
#include "components/translate/core/common/translate_errors.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {
class BrowserContext;
class Page;
enum class Visibility;
class WebContents;
}  // namespace content

class PrefService;

namespace language {
class AcceptLanguagesService;
}

namespace translate {
class AutoTranslateSnackbarController;
class LanguageState;
class TranslatePrefs;
class TranslateManager;
class TranslateMessage;

struct LanguageDetectionDetails;
}  // namespace translate

enum class ShowTranslateBubbleResult;

class ChromeTranslateClient
    : public translate::TranslateClient,
      public translate::TranslateDriver::LanguageDetectionObserver,
      public content::WebContentsObserver,
      public content::WebContentsUserData<ChromeTranslateClient> {
 public:
  ChromeTranslateClient(const ChromeTranslateClient&) = delete;
  ChromeTranslateClient& operator=(const ChromeTranslateClient&) = delete;

  ~ChromeTranslateClient() override;

  // Gets the LanguageState associated with the page.
  const translate::LanguageState& GetLanguageState();

  // Returns the ContentTranslateDriver instance associated with this
  // WebContents.
  translate::ContentTranslateDriver* translate_driver() {
    return translate_driver_.get();
  }

  // Returns the ContentLanguageDetectionDriver instance associated with this
  // WebContents.
  language_detection::ContentLanguageDetectionDriver*
  language_detection_driver() {
    return language_detection_driver_.get();
  }

  // Helper method to return a new TranslatePrefs instance.
  static std::unique_ptr<translate::TranslatePrefs> CreateTranslatePrefs(
      PrefService* prefs);

  // Helper method to return the AcceptLanguagesService instance associated
  // with |browser_context|.
  static language::AcceptLanguagesService* GetAcceptLanguagesService(
      content::BrowserContext* browser_context);

  // Helper method to return the TranslateManager instance associated with
  // |web_contents|, or NULL if there is no such associated instance.
  static translate::TranslateManager* GetManagerFromWebContents(
      content::WebContents* web_contents);

  // Gets |source| and |target| languages. |source| is the original source
  // language of a page. |target| is |TranslateManager::GetTargetLanguage|,
  // or, if |for_display| is true and the page was translated - the current page
  // language.
  void GetTranslateLanguages(content::WebContents* web_contents,
                             std::string* source,
                             std::string* target,
                             bool for_display = true);

  // Gets the associated TranslateManager.
  translate::TranslateManager* GetTranslateManager();

  // TranslateClient implementation.
  translate::TranslateDriver* GetTranslateDriver() override;
  PrefService* GetPrefs() override;
  std::unique_ptr<translate::TranslatePrefs> GetTranslatePrefs() override;
  language::AcceptLanguagesService* GetAcceptLanguagesService() override;
#if BUILDFLAG(IS_ANDROID)
  // Trigger a manual translation when the necessary state (e.g. source
  // language) is ready.
  void ManualTranslateWhenReady();
#endif
  void SetPredefinedTargetLanguage(const std::string& translate_language_code,
                                   bool should_auto_translate);

  bool ShowTranslateUI(translate::TranslateStep step,
                       const std::string& source_language,
                       const std::string& target_language,
                       translate::TranslateErrors error_type,
                       bool triggered_from_menu) override;
  bool IsTranslatableURL(const GURL& url) override;

  // TranslateDriver::LanguageDetectionObserver implementation.
  void OnLanguageDetermined(
      const translate::LanguageDetectionDetails& details) override;

 private:
  explicit ChromeTranslateClient(content::WebContents* web_contents);
  friend class content::WebContentsUserData<ChromeTranslateClient>;
  FRIEND_TEST_ALL_PREFIXES(ChromeTranslateClientTest,
                           LanguageEventShouldRecord);
  FRIEND_TEST_ALL_PREFIXES(ChromeTranslateClientTest,
                           LanguageEventShouldNotRecord);
  FRIEND_TEST_ALL_PREFIXES(ChromeTranslateClientTest,
                           TranslationEventShouldRecord);
  FRIEND_TEST_ALL_PREFIXES(ChromeTranslateClientTest,
                           TranslationEventShouldNotRecord);

  // content::WebContentsObserver implementation.
  void WebContentsDestroyed() override;

#if !BUILDFLAG(IS_ANDROID)
  // Shows the Full Page Translate bubble.
  ShowTranslateBubbleResult ShowBubble(translate::TranslateStep step,
                                       const std::string& source_language,
                                       const std::string& target_language,
                                       translate::TranslateErrors error_type,
                                       bool is_user_gesture);
#endif

  std::unique_ptr<translate::ContentTranslateDriver> translate_driver_;
  std::unique_ptr<language_detection::ContentLanguageDetectionDriver>
      language_detection_driver_;
  std::unique_ptr<translate::TranslateManager> translate_manager_;

#if BUILDFLAG(IS_ANDROID)
  // Whether to trigger a manual translation when ready.
  // See ChromeTranslateClient::ManualTranslateOnReady
  bool manual_translate_on_ready_ = false;

  std::unique_ptr<translate::TranslateMessage> translate_message_;
  std::unique_ptr<translate::AutoTranslateSnackbarController>
      auto_translate_snackbar_controller_;

  // content::WebContentsObserver implementation on Android only. Used for the
  // auto-translate Snackbar.
  void PrimaryPageChanged(content::Page& page) override;
  void OnVisibilityChanged(content::Visibility visibility) override;
#endif

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_TRANSLATE_CHROME_TRANSLATE_CLIENT_H_

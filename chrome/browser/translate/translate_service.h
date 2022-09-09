// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TRANSLATE_TRANSLATE_SERVICE_H_
#define CHROME_BROWSER_TRANSLATE_TRANSLATE_SERVICE_H_

#include "components/web_resource/resource_request_allowed_notifier.h"

class GURL;
class PrefService;

namespace language {
class LanguageModel;
}  // namespace language

// Singleton managing the resources required for Translate.
class TranslateService
    : public web_resource::ResourceRequestAllowedNotifier::Observer {
 public:
  // Must be called before the Translate feature can be used.
  static void Initialize();

  // Must be called to shut down the Translate feature.
  static void Shutdown();

  // Initializes the TranslateService in a way that it can be initialized
  // multiple times in a unit test suite (once for each test). Should be paired
  // with ShutdownForTesting at the end of the test.
  static void InitializeForTesting(network::mojom::ConnectionType type);

  // Shuts down the TranslateService at the end of a test in a way that the next
  // test can initialize and use the service.
  static void ShutdownForTesting();

  // Returns true if the Full Page Translate bubble is enabled.
  static bool IsTranslateBubbleEnabled();

  // Returns the language to translate to. For more details, see
  // TranslateManager::GetTargetLanguage.
  static std::string GetTargetLanguage(PrefService* prefs,
                                       language::LanguageModel* language_model);

  // Returns true if the URL can be translated.
  static bool IsTranslatableURL(const GURL& url);

  // Returns true if the service is available and enabled by user preferences.
  static bool IsAvailable(PrefService* prefs);

 private:
  TranslateService();
  ~TranslateService() override;

  // ResourceRequestAllowedNotifier::Observer methods.
  void OnResourceRequestsAllowed() override;

  // Helper class to know if it's allowed to make network resource requests.
  web_resource::ResourceRequestAllowedNotifier
      resource_request_allowed_notifier_;
};

#endif  // CHROME_BROWSER_TRANSLATE_TRANSLATE_SERVICE_H_

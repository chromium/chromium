// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILE_MANAGER_CLOUD_UPLOAD_PROMPT_PREFS_HANDLER_H_
#define CHROME_BROWSER_ASH_FILE_MANAGER_CLOUD_UPLOAD_PROMPT_PREFS_HANDLER_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "components/keyed_service/core/keyed_service.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace chromeos::cloud_upload {

// This factory reacts to profile creation and instantiates profile-keyed
// services that set up handlers for prefs related to Cloud Upload move
// confirmation prompts.
class CloudUploadPromptPrefsHandlerFactory : public ProfileKeyedServiceFactory {
 public:
  CloudUploadPromptPrefsHandlerFactory(
      const CloudUploadPromptPrefsHandlerFactory&) = delete;
  CloudUploadPromptPrefsHandlerFactory& operator=(
      const CloudUploadPromptPrefsHandlerFactory&) = delete;

  static CloudUploadPromptPrefsHandlerFactory* GetInstance();

 private:
  friend base::NoDestructor<CloudUploadPromptPrefsHandlerFactory>;

  CloudUploadPromptPrefsHandlerFactory();
  ~CloudUploadPromptPrefsHandlerFactory() override;

  // BrowserContextKeyedServiceFactory:
  void RegisterProfilePrefs(
      user_prefs::PrefRegistrySyncable* registry) override;
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
};

}  // namespace chromeos::cloud_upload

#endif  // CHROME_BROWSER_ASH_FILE_MANAGER_CLOUD_UPLOAD_PROMPT_PREFS_HANDLER_H_

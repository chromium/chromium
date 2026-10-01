// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_manager/cloud_upload_prompt_prefs_handler_factory.h"

#include <memory>

#include "chrome/browser/ash/file_manager/cloud_upload_prompt_prefs_handler.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/sync_service_factory.h"

namespace chromeos::cloud_upload {

// static
CloudUploadPromptPrefsHandlerFactory*
CloudUploadPromptPrefsHandlerFactory::GetInstance() {
  static base::NoDestructor<CloudUploadPromptPrefsHandlerFactory> instance;
  return instance.get();
}

CloudUploadPromptPrefsHandlerFactory::CloudUploadPromptPrefsHandlerFactory()
    : ProfileKeyedServiceFactory("CloudUploadPromptPrefsHandlerFactory") {
  DependsOn(SyncServiceFactory::GetInstance());
}

CloudUploadPromptPrefsHandlerFactory::~CloudUploadPromptPrefsHandlerFactory() =
    default;

void CloudUploadPromptPrefsHandlerFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  RegisterCloudUploadPromptProfilePrefs(registry);
}

std::unique_ptr<KeyedService>
CloudUploadPromptPrefsHandlerFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  // Check this before asking for the SyncService: this factory creates its
  // service with the browser context, and looking the SyncService up
  // unconditionally would build one for every profile.
  if (!ShouldCreateCloudUploadPromptPrefsHandler(profile)) {
    return nullptr;
  }
  return CreateCloudUploadPromptPrefsHandler(
      profile, SyncServiceFactory::GetForProfile(profile));
}

bool CloudUploadPromptPrefsHandlerFactory::ServiceIsCreatedWithBrowserContext()
    const {
  return true;
}

}  // namespace chromeos::cloud_upload

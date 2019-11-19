// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/external_policy_loader.h"

#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chrome/browser/extensions/external_provider_impl.h"
#include "chrome/browser/extensions/forced_extensions/installation_reporter.h"
#include "chrome/browser/profiles/profile.h"

namespace extensions {

ExternalPolicyLoader::ExternalPolicyLoader(Profile* profile,
                                           ExtensionManagement* settings,
                                           InstallationType type)
    : profile_(profile), settings_(settings), type_(type) {
  settings_->AddObserver(this);
}

ExternalPolicyLoader::~ExternalPolicyLoader() {
  settings_->RemoveObserver(this);
}

void ExternalPolicyLoader::OnExtensionManagementSettingsChanged() {
  StartLoading();
}

// static
void ExternalPolicyLoader::AddExtension(base::DictionaryValue* dict,
                                        const std::string& extension_id,
                                        const std::string& update_url) {
  dict->SetString(base::StringPrintf("%s.%s", extension_id.c_str(),
                                     ExternalProviderImpl::kExternalUpdateUrl),
                  update_url);
}

void ExternalPolicyLoader::StartLoading() {
  std::unique_ptr<base::DictionaryValue> prefs;
  switch (type_) {
    case FORCED: {
      InstallationReporter* installation_reporter =
          InstallationReporter::Get(profile_);
      prefs = settings_->GetForceInstallList();
      for (const auto& it : prefs->DictItems()) {
        installation_reporter->ReportInstallationStage(
            it.first, InstallationReporter::Stage::SEEN_BY_POLICY_LOADER);
      }
      break;
    }
    case RECOMMENDED:
      prefs = settings_->GetRecommendedInstallList();
      break;
  }
  LoadFinished(std::move(prefs));
}

}  // namespace extensions

// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_web_apps/apps/personalization_app/personalization_app_manager.h"

#include <memory>

#include "ash/webui/personalization_app/search/search_handler.h"
#include "chrome/browser/ash/system_web_apps/apps/personalization_app/enterprise_policy_delegate_impl.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/ash/components/local_search_service/public/cpp/local_search_service_proxy.h"
#include "content/public/browser/browser_context.h"

namespace ash::personalization_app {

namespace {

class PersonalizationAppManagerImpl : public PersonalizationAppManager {
 public:
  PersonalizationAppManagerImpl(content::BrowserContext* context,
                                local_search_service::LocalSearchServiceProxy&
                                    local_search_service_proxy) {
    Profile* profile = Profile::FromBrowserContext(context);
    CHECK(profile, base::NotFatalUntil::M160);
    search_handler_ = std::make_unique<SearchHandler>(
        local_search_service_proxy, profile->GetPrefs(),
        std::make_unique<EnterprisePolicyDelegateImpl>(context));
  }

  ~PersonalizationAppManagerImpl() override = default;

  SearchHandler* search_handler() override { return search_handler_.get(); }

 private:
  // KeyedService:
  void Shutdown() override { search_handler_.reset(); }

  // Handles running search queries for Personalization App features. Only set
  // if |PersonalizationHub| feature is enabled.
  std::unique_ptr<SearchHandler> search_handler_;
};

}  // namespace

// static
std::unique_ptr<PersonalizationAppManager> PersonalizationAppManager::Create(
    content::BrowserContext* context,
    local_search_service::LocalSearchServiceProxy& local_search_service_proxy) {
  return std::make_unique<PersonalizationAppManagerImpl>(
      context, local_search_service_proxy);
}

}  // namespace ash::personalization_app

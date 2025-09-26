// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/data_protection/data_protection_url_lookup_service.h"

#include "base/feature_list.h"
#include "base/no_destructor.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/enterprise/data_protection/data_protection_features.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/sessions/content/session_tab_helper.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"

namespace enterprise_data_protection {

void OnRealTimeLookupComplete(
    LookupCallback callback,
    const std::string& identifier,
    bool is_success,
    bool is_cached,
    std::unique_ptr<safe_browsing::RTLookupResponse> rt_lookup_response) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!is_success) {
    rt_lookup_response.reset();
  }

  std::move(callback).Run(std::move(rt_lookup_response));
}

DataProtectionUrlLookupService::DataProtectionUrlLookupService() = default;
DataProtectionUrlLookupService::~DataProtectionUrlLookupService() = default;

void DataProtectionUrlLookupService::DoLookup(
    safe_browsing::RealTimeUrlLookupServiceBase* lookup_service,
    const GURL& url,
    const std::string& identifier,
    LookupCallback callback,
    content::WebContents* web_contents) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(web_contents);
  DCHECK(callback);

  lookup_service->StartMaybeCachedLookup(
      url,
      base::BindOnce(&OnRealTimeLookupComplete, std::move(callback),
                     identifier),
      base::SequencedTaskRunner::GetCurrentDefault(),
      sessions::SessionTabHelper::IdForTab(web_contents),
      /*referring_app_info=*/std::nullopt, /*use_cache=*/
      !base::FeatureList::IsEnabled(kEnableSinglePageAppDataProtection));
}

// ====================================================
// DataProtectionUrlLookupServiceFactory implementation
// ====================================================

DataProtectionUrlLookupServiceFactory::DataProtectionUrlLookupServiceFactory()
    : ProfileKeyedServiceFactory("DataProtectionUrlLookupService",
                                 ProfileSelections::BuildForRegularProfile()) {}

DataProtectionUrlLookupServiceFactory::
    ~DataProtectionUrlLookupServiceFactory() = default;

// static
DataProtectionUrlLookupServiceFactory*
DataProtectionUrlLookupServiceFactory::GetInstance() {
  static base::NoDestructor<DataProtectionUrlLookupServiceFactory> instance;
  return instance.get();
}

// static
DataProtectionUrlLookupService*
DataProtectionUrlLookupServiceFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<DataProtectionUrlLookupService*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

std::unique_ptr<KeyedService>
DataProtectionUrlLookupServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<DataProtectionUrlLookupService>();
}

}  // namespace enterprise_data_protection

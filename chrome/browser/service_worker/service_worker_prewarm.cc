// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/service_worker/service_worker_prewarm.h"

#include "base/check_is_test.h"
#include "base/feature_list.h"
#include "base/functional/callback_helpers.h"
#include "base/trace_event/trace_event.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/service_worker_context.h"
#include "content/public/common/origin_util.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace {

// Warm up the ServiceWorker registration for DSE.
BASE_FEATURE(kPrewarmServiceWorkerRegistrationForDSE,
             base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace

namespace chrome_service_worker {

void PrewarmServiceWorkerRegistrationForDSE(
    content::BrowserContext* browser_context,
    content::ServiceWorkerContext& service_worker_context) {
  TRACE_EVENT("ServiceWorker",
              "chrome_service_worker::PrewarmServiceWorkerRegistrationForDSE");

  if (PrewarmServiceWorkerRegistrationForDSECalledCountForTesting()
          .has_value()) {
    CHECK_IS_TEST();
    ++(PrewarmServiceWorkerRegistrationForDSECalledCountForTesting().value());
  }

  if (!base::FeatureList::IsEnabled(kPrewarmServiceWorkerRegistrationForDSE)) {
    return;
  }

  Profile* profile = Profile::FromBrowserContext(browser_context);

  if (!profile) {
    return;
  }

  TemplateURLService* template_url_service =
      TemplateURLServiceFactory::GetForProfile(profile);

  if (!template_url_service) {
    return;
  }

  GURL url =
      template_url_service->GenerateSearchURLForDefaultSearchProvider(u"");

  if (!content::OriginCanAccessServiceWorkers(url)) {
    return;
  }

  const blink::StorageKey key =
      blink::StorageKey::CreateFirstParty(url::Origin::Create(url));

  if (!service_worker_context.MaybeHasRegistrationForStorageKey(key)) {
    return;
  }

  service_worker_context.CheckHasServiceWorker(url, key, base::DoNothing());
}

std::optional<int>&
PrewarmServiceWorkerRegistrationForDSECalledCountForTesting() {
  static std::optional<int> call_count;
  return call_count;
}

}  // namespace chrome_service_worker

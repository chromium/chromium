// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/explore_sites/explore_sites_service_impl.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/post_task.h"
#include "chrome/browser/android/chrome_feature_list.h"
#include "chrome/browser/android/explore_sites/blacklist_site_task.h"
#include "chrome/browser/android/explore_sites/catalog.pb.h"
#include "chrome/browser/android/explore_sites/clear_activities_task.h"
#include "chrome/browser/android/explore_sites/clear_catalog_task.h"
#include "chrome/browser/android/explore_sites/explore_sites_bridge.h"
#include "chrome/browser/android/explore_sites/explore_sites_feature.h"
#include "chrome/browser/android/explore_sites/explore_sites_schema.h"
#include "chrome/browser/android/explore_sites/explore_sites_store.h"
#include "chrome/browser/android/explore_sites/get_catalog_task.h"
#include "chrome/browser/android/explore_sites/get_images_task.h"
#include "chrome/browser/android/explore_sites/get_version_task.h"
#include "chrome/browser/android/explore_sites/image_helper.h"
#include "chrome/browser/android/explore_sites/import_catalog_task.h"
#include "chrome/browser/android/explore_sites/increment_shown_count_task.h"
#include "chrome/browser/android/explore_sites/record_site_click_task.h"
#include "chrome/browser/browser_process.h"
#include "components/offline_pages/task/task.h"
#include "components/variations/service/variations_service.h"
#include "content/public/browser/browser_thread.h"
#include "sql/database.h"
#include "sql/meta_table.h"
#include "sql/statement.h"

using chrome::android::explore_sites::ExploreSitesVariation;
using chrome::android::explore_sites::GetExploreSitesVariation;
using offline_pages::Task;

namespace {
void ReportCatalogError(explore_sites::ExploreSitesCatalogError error) {
  UMA_HISTOGRAM_ENUMERATION("ExploreSites.CatalogError", error);
}

void ReportCatalogRequestResult(
    explore_sites::ExploreSitesCatalogUpdateRequestResult result) {
  UMA_HISTOGRAM_ENUMERATION("ExploreSites.CatalogRequestResult", result);
}
}  // namespace

namespace explore_sites {

ExploreSitesServiceImpl::ExploreSitesServiceImpl(
    std::unique_ptr<ExploreSitesStore> store,
    std::unique_ptr<URLLoaderFactoryGetter> url_loader_factory_getter,
    std::unique_ptr<HistoryStatisticsReporter> history_statistics_reporter)
    : task_queue_(this),
      explore_sites_store_(std::move(store)),
      url_loader_factory_getter_(std::move(url_loader_factory_getter)),
      history_statistics_reporter_(std::move(history_statistics_reporter)) {
  if (IsExploreSitesEnabled()) {
    ExploreSitesBridge::ScheduleDailyTask();
  }
  // Collect history statistics unconditionally, to have baseline as well.
  history_statistics_reporter_->ScheduleReportStatistics();
}

ExploreSitesServiceImpl::~ExploreSitesServiceImpl() {}

// static
bool ExploreSitesServiceImpl::IsExploreSitesEnabled() {
  ExploreSitesVariation variation = GetExploreSitesVariation();
  return variation == ExploreSitesVariation::ENABLED ||
         variation == ExploreSitesVariation::PERSONALIZED ||
         variation == ExploreSitesVariation::MOST_LIKELY;
}

void ExploreSitesServiceImpl::GetCatalog(CatalogCallback callback) {
  if (!IsExploreSitesEnabled())
    return;

  // TODO(https://crbug.com/910255): Ensure the catalog swap doesn't happen
  // during a session.
  task_queue_.AddTask(std::make_unique<GetCatalogTask>(
      explore_sites_store_.get(), /*update_current*/ true,
      std::move(callback)));
}

void ExploreSitesServiceImpl::GetCategoryImage(int category_id,
                                               int pixel_size,
                                               BitmapCallback callback) {
  task_queue_.AddTask(std::make_unique<GetImagesTask>(
      explore_sites_store_.get(), category_id, kFaviconsPerCategoryImage,
      base::BindOnce(&ExploreSitesServiceImpl::ComposeCategoryImage,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     pixel_size)));
}

void ExploreSitesServiceImpl::GetSummaryImage(int pixel_size,
                                              BitmapCallback callback) {
  task_queue_.AddTask(std::make_unique<GetImagesTask>(
      explore_sites_store_.get(), GetImagesTask::DataType::kSummary,
      kFaviconsPerCategoryImage,
      base::BindOnce(&ExploreSitesServiceImpl::ComposeCategoryImage,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     pixel_size)));
}

void ExploreSitesServiceImpl::GetSiteImage(int site_id,
                                           BitmapCallback callback) {
  task_queue_.AddTask(std::make_unique<GetImagesTask>(
      explore_sites_store_.get(), site_id,
      base::BindOnce(&ExploreSitesServiceImpl::ComposeSiteImage,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback))));
}

void ExploreSitesServiceImpl::UpdateCatalogFromNetwork(
    bool is_immediate_fetch,
    const std::string& accept_languages,
    BooleanCallback callback) {
  if (!IsExploreSitesEnabled())
    return;

  update_catalog_callbacks_.push_back(std::move(callback));

  // If we are already fetching, don't interrupt a fetch in progress unless
  // immediate fetch is requested while currently running fetch is background
  // fetch. This is because we want to get back the result asap for immediate
  // fetch.
  if (explore_sites_fetcher_ != nullptr) {
    if (is_immediate_fetch)
      explore_sites_fetcher_->RestartAsImmediateFetchIfNotYet();
    return;
  }
  // We want to create the fetcher, but need to grab the current version from
  // the DB first.
  task_queue_.AddTask(std::make_unique<GetVersionTask>(
      explore_sites_store_.get(),
      base::BindOnce(&ExploreSitesServiceImpl::GotVersionToStartFetch,
                     weak_ptr_factory_.GetWeakPtr(), is_immediate_fetch,
                     accept_languages)));
}

void ExploreSitesServiceImpl::RecordClick(const std::string& url,
                                          int category_type) {
  // Record the activity in the activity table.
  task_queue_.AddTask(std::make_unique<RecordSiteClickTask>(
      explore_sites_store_.get(), url, category_type));
}

void ExploreSitesServiceImpl::BlacklistSite(const std::string& url) {
  // Add the url to the blacklist table in the database.
  task_queue_.AddTask(
      std::make_unique<BlacklistSiteTask>(explore_sites_store_.get(), url));

  // TODO(https://crbug.com/893845): Remove cached category icon if affected.
}

void ExploreSitesServiceImpl::ClearActivities(base::Time begin,
                                              base::Time end,
                                              base::OnceClosure callback) {
  task_queue_.AddTask(std::make_unique<ClearActivitiesTask>(
      explore_sites_store_.get(), begin, end,
      base::BindOnce(
          [](base::OnceClosure callback, bool) { std::move(callback).Run(); },
          std::move(callback))));
}

void ExploreSitesServiceImpl::IncrementNtpShownCount(int category_id) {
  task_queue_.AddTask(std::make_unique<IncrementShownCountTask>(
      explore_sites_store_.get(), category_id));
}

void ExploreSitesServiceImpl::ClearCachedCatalogsForDebugging() {
  task_queue_.AddTask(std::make_unique<ClearCatalogTask>(
      explore_sites_store_.get(), base::BindOnce([](bool result) {})));
}

void ExploreSitesServiceImpl::OverrideCountryCodeForDebugging(
    const std::string& country_code) {
  country_override_ = std::make_unique<std::string>(country_code);
}

std::string ExploreSitesServiceImpl::GetCountryCode() {
  if (country_override_)
    return *country_override_;

  std::string manually_set_variation_country =
      base::GetFieldTrialParamValueByFeature(chrome::android::kExploreSites,
                                             "country_override");
  if (!manually_set_variation_country.empty())
    return manually_set_variation_country;

  variations::VariationsService* variations_service =
      g_browser_process->variations_service();
  if (variations_service) {
    std::string country = variations_service->GetStoredPermanentCountry();
    if (!country.empty())
      return country;
    country = variations_service->GetLatestCountry();
    if (!country.empty())
      return country;
  }

  return "DEFAULT";
}

// Validate the catalog.  Note this does not take ownership of the pointer.
// Since we can't modify the collection while iterating over it, we instead copy
// the valid contents into a new validated catalog.
std::unique_ptr<Catalog> ValidateCatalog(std::unique_ptr<Catalog> catalog) {
  std::unique_ptr<Catalog> validated_catalog = std::make_unique<Catalog>();

  if (catalog == nullptr)
    return validated_catalog;

  // Check each category.
  for (auto& category : *catalog->mutable_categories()) {
    bool remove_category = false;
    // Validate that the category type is within the known range.
    if (!Category::CategoryType_IsValid(category.type())) {
      remove_category = true;
      ReportCatalogError(ExploreSitesCatalogError::kCategoryWithUnknownType);
    }
    // Validate that the category has a title.
    if (category.localized_title().empty()) {
      remove_category = true;
      ReportCatalogError(ExploreSitesCatalogError::kCategoryMissingTitle);
    }

    if (remove_category)
      continue;

    Category* new_category = nullptr;

    // Check the individual sites in this category.
    for (auto& site : *category.mutable_sites()) {
      // Ensure the URL parses and is in a valid format.
      GURL url(site.site_url());
      if (!url.is_valid()) {
        ReportCatalogError(ExploreSitesCatalogError::kSiteWithBadUrl);
        continue;
      }
      if (site.title().empty()) {
        ReportCatalogError(ExploreSitesCatalogError::kSiteMissingTitle);
        continue;
      }
      if (site.icon().empty()) {
        // We should report missing icons, but we will still include the site if
        // only the icon is missing.
        ReportCatalogError(ExploreSitesCatalogError::kSiteMissingIcon);
      }

      // If we have at least one valid site, we can safely create a category.
      if (new_category == nullptr) {
        // Create a new (empty) category.  We will fill it if we find at least
        // one good site.
        new_category = validated_catalog->add_categories();
      }

      // Add the site into the category we are working on.
      Site* new_site = new_category->add_sites();
      new_site->Swap(&site);

      // We want to use a canonicalized URL in the database so that blacklisting
      // will always work.  Typically this will cause a trailing slash to be
      // added if it's missing.
      new_site->set_site_url(url.spec());
    }

    // Collect UMA if the last site was removed from the category, or there were
    // none to start with.
    if (new_category == nullptr) {
      ReportCatalogError(ExploreSitesCatalogError::kCategoryWithNoSites);
      continue;
    }

    // Now that sites have been copied in, copy over the other fields from the
    // original category.
    category.clear_sites();
    new_category->MergeFrom(category);
  }

  return validated_catalog;
}

void ExploreSitesServiceImpl::OnCatalogFetchedForTest(
    ExploreSitesRequestStatus status,
    std::unique_ptr<std::string> serialized_protobuf) {
  OnCatalogFetched(status, std::move(serialized_protobuf));
}

void ExploreSitesServiceImpl::GotVersionToStartFetch(
    bool is_immediate_fetch,
    const std::string& accept_languages,
    std::string catalog_version) {
  if (explore_sites_fetcher_) {
    // There was a race.
    return;
  }

  // Create a fetcher and start fetching the protobuf (async).
  explore_sites_fetcher_ = ExploreSitesFetcher::CreateForGetCatalog(
      is_immediate_fetch, catalog_version, accept_languages, GetCountryCode(),
      url_loader_factory_getter_->GetFactory(),
      base::BindOnce(&ExploreSitesServiceImpl::OnCatalogFetched,
                     weak_ptr_factory_.GetWeakPtr()));
  explore_sites_fetcher_->Start();
}

void ExploreSitesServiceImpl::OnCatalogFetched(
    ExploreSitesRequestStatus status,
    std::unique_ptr<std::string> serialized_protobuf) {
  UMA_HISTOGRAM_ENUMERATION("ExploreSites.RequestStatus", status);

  explore_sites_fetcher_.reset(nullptr);

  if (serialized_protobuf == nullptr) {
    DVLOG(1) << "Empty catalog response received from network.";
    if (status == ExploreSitesRequestStatus::kSuccess) {
      ReportCatalogRequestResult(
          ExploreSitesCatalogUpdateRequestResult::kExistingCatalogIsCurrent);
    } else {
      ReportCatalogRequestResult(
          ExploreSitesCatalogUpdateRequestResult::kFailure);
    }
    NotifyCatalogUpdated(std::move(update_catalog_callbacks_), false);
    update_catalog_callbacks_.clear();
    return;
  }

  // Convert the protobuf into a catalog object.
  explore_sites::GetCatalogResponse catalog_response;
  if (!catalog_response.ParseFromString(*serialized_protobuf.get())) {
    DVLOG(1) << "Failed to parse catalog";
    NotifyCatalogUpdated(std::move(update_catalog_callbacks_), false);
    update_catalog_callbacks_.clear();
    ReportCatalogError(ExploreSitesCatalogError::kParseFailure);
    return;
  }
  std::string catalog_version = catalog_response.version_token();

  // Check the catalog, canonicalizing any URLs in it.
  if (catalog_response.has_catalog()) {
    std::unique_ptr<Catalog> validated_catalog = ValidateCatalog(
        base::WrapUnique<Catalog>(catalog_response.release_catalog()));

    // Add the catalog to our internal database.
    task_queue_.AddTask(std::make_unique<ImportCatalogTask>(
        explore_sites_store_.get(), catalog_version,
        std::move(validated_catalog),
        base::BindOnce(&ExploreSitesServiceImpl::NotifyCatalogUpdated,
                       weak_ptr_factory_.GetWeakPtr(),
                       std::move(update_catalog_callbacks_))));
  } else {
    NotifyCatalogUpdated(std::move(update_catalog_callbacks_), true);
  }

  ReportCatalogRequestResult(
      ExploreSitesCatalogUpdateRequestResult::kNewCatalog);
}

void ExploreSitesServiceImpl::ComposeSiteImage(BitmapCallback callback,
                                               EncodedImageList images) {
  image_helper_.ComposeSiteImage(std::move(callback), std::move(images));
}

void ExploreSitesServiceImpl::ComposeCategoryImage(BitmapCallback callback,
                                                   int pixel_size,
                                                   EncodedImageList images) {
  image_helper_.ComposeCategoryImage(std::move(callback), pixel_size,
                                     std::move(images));
}

void ExploreSitesServiceImpl::Shutdown() {}

void ExploreSitesServiceImpl::OnTaskQueueIsIdle() {}

void ExploreSitesServiceImpl::NotifyCatalogUpdated(
    std::vector<BooleanCallback> callbacks,
    bool success) {
  for (auto& callback : callbacks)
    std::move(callback).Run(success);
}

}  // namespace explore_sites

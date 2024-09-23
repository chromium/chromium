// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/contacts/nearby_share_contact_downloader_impl.h"

#include <utility>
#include <vector>

#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/nearby_sharing/client/nearby_share_client.h"
#include "chrome/browser/nearby_sharing/common/nearby_share_features.h"
#include "components/cross_device/logging/logging.h"

namespace {

void RecordListContactPeopleResultMetrics(
    ash::nearby::NearbyHttpResult result) {
  base::UmaHistogramEnumeration("Nearby.Share.Contacts.HttpResult", result);
}

void RecordContactDownloadResultMetrics(bool success,
                                        size_t current_page_number,
                                        base::TimeTicks start_timestamp_) {
  base::UmaHistogramBoolean("Nearby.Share.Contacts.DownloadResult", success);
  if (success) {
    base::UmaHistogramCounts100(
        "Nearby.Share.Contacts.DownloadPageCount.Success", current_page_number);
    base::UmaHistogramTimes("Nearby.Share.Contacts.TimeToDownload.Success",
                            base::TimeTicks::Now() - start_timestamp_);
  } else {
    base::UmaHistogramCounts100(
        "Nearby.Share.Contacts.DownloadPageCount.Failure", current_page_number);
    base::UmaHistogramTimes("Nearby.Share.Contacts.TimeToDownload.Failure",
                            base::TimeTicks::Now() - start_timestamp_);
  }
}

void RecordContactDistributionMetrics(
    const std::vector<nearby::sharing::proto::ContactRecord>&
        unfiltered_contacts) {
  size_t num_reachable = 0;
  size_t num_unknown_type = 0;
  size_t num_google_type = 0;
  size_t num_device_type = 0;
  for (const auto& contact : unfiltered_contacts) {
    if (contact.is_reachable())
      ++num_reachable;

    switch (contact.type()) {
      case nearby::sharing::proto::ContactRecord::UNKNOWN:
        ++num_unknown_type;
        break;
      case nearby::sharing::proto::ContactRecord::GOOGLE_CONTACT:
        ++num_google_type;
        break;
      case nearby::sharing::proto::ContactRecord::DEVICE_CONTACT:
        ++num_device_type;
        break;
      case nearby::sharing::proto::
          ContactRecord_Type_ContactRecord_Type_INT_MIN_SENTINEL_DO_NOT_USE_:
      case nearby::sharing::proto::
          ContactRecord_Type_ContactRecord_Type_INT_MAX_SENTINEL_DO_NOT_USE_:
        NOTREACHED_IN_MIGRATION();
    }
  }
  base::UmaHistogramCounts10000("Nearby.Share.Contacts.NumContacts.Unfiltered",
                                unfiltered_contacts.size());
  base::UmaHistogramCounts10000("Nearby.Share.Contacts.NumContacts.Reachable",
                                num_reachable);
  base::UmaHistogramCounts10000("Nearby.Share.Contacts.NumContacts.Unreachable",
                                unfiltered_contacts.size() - num_reachable);
  base::UmaHistogramCounts10000(
      "Nearby.Share.Contacts.NumContacts.UnknownContactType", num_unknown_type);
  base::UmaHistogramCounts10000(
      "Nearby.Share.Contacts.NumContacts.GoogleContactType", num_google_type);
  base::UmaHistogramCounts10000(
      "Nearby.Share.Contacts.NumContacts.DeviceContactType", num_device_type);
  if (!unfiltered_contacts.empty()) {
    base::UmaHistogramPercentage(
        "Nearby.Share.Contacts.PercentReachable",
        std::lround(100.0f * num_reachable / unfiltered_contacts.size()));
    base::UmaHistogramPercentage(
        "Nearby.Share.Contacts.PercentDeviceContactType",
        std::lround(100.0f * num_device_type / unfiltered_contacts.size()));
  }
}

}  // namespace

// static
NearbyShareContactDownloaderImpl::Factory*
    NearbyShareContactDownloaderImpl::Factory::test_factory_ = nullptr;

// static
std::unique_ptr<NearbyShareContactDownloader>
NearbyShareContactDownloaderImpl::Factory::Create(
    const std::string& device_id,
    base::TimeDelta timeout,
    NearbyShareClientFactory* client_factory,
    SuccessCallback success_callback,
    FailureCallback failure_callback) {
  if (test_factory_) {
    return test_factory_->CreateInstance(device_id, timeout, client_factory,
                                         std::move(success_callback),
                                         std::move(failure_callback));
  }

  return base::WrapUnique(new NearbyShareContactDownloaderImpl(
      device_id, timeout, client_factory, std::move(success_callback),
      std::move(failure_callback)));
}

// static
void NearbyShareContactDownloaderImpl::Factory::SetFactoryForTesting(
    Factory* test_factory) {
  test_factory_ = test_factory;
}

NearbyShareContactDownloaderImpl::Factory::~Factory() = default;

NearbyShareContactDownloaderImpl::NearbyShareContactDownloaderImpl(
    const std::string& device_id,
    base::TimeDelta timeout,
    NearbyShareClientFactory* client_factory,
    SuccessCallback success_callback,
    FailureCallback failure_callback)
    : NearbyShareContactDownloader(device_id,
                                   std::move(success_callback),
                                   std::move(failure_callback)),
      timeout_(timeout),
      client_factory_(client_factory) {}

NearbyShareContactDownloaderImpl::~NearbyShareContactDownloaderImpl() = default;

void NearbyShareContactDownloaderImpl::OnRun() {
  CD_LOG(VERBOSE, Feature::NS) << __func__ << ": Starting contacts download.";
  start_timestamp_ = base::TimeTicks::Now();
  CallListContactPeople(/*next_page_token=*/std::nullopt);
}

void NearbyShareContactDownloaderImpl::CallListContactPeople(
    const std::optional<std::string>& next_page_token) {
  ++current_page_number_;
  CD_LOG(VERBOSE, Feature::NS)
      << __func__ << ": Making ListContactPeople RPC call to fetch page number "
      << current_page_number_
      << " with page token: " << next_page_token.value_or("[null]");
  timer_.Start(
      FROM_HERE, timeout_,
      base::BindOnce(
          &NearbyShareContactDownloaderImpl::OnListContactPeopleTimeout,
          base::Unretained(this)));

  nearby::sharing::proto::ListContactPeopleRequest request;
  if (next_page_token)
    request.set_page_token(*next_page_token);

  client_ = client_factory_->CreateInstance();
  client_->ListContactPeople(
      request,
      base::BindOnce(
          &NearbyShareContactDownloaderImpl::OnListContactPeopleSuccess,
          base::Unretained(this)),
      base::BindOnce(
          &NearbyShareContactDownloaderImpl::OnListContactPeopleFailure,
          base::Unretained(this)));
}

void NearbyShareContactDownloaderImpl::OnListContactPeopleSuccess(
    const nearby::sharing::proto::ListContactPeopleResponse& response) {
  timer_.Stop();
  contacts_.insert(contacts_.end(), response.contact_records().begin(),
                   response.contact_records().end());
  std::optional<std::string> next_page_token =
      response.next_page_token().empty()
          ? std::nullopt
          : std::make_optional<std::string>(response.next_page_token());
  client_.reset();
  RecordListContactPeopleResultMetrics(ash::nearby::NearbyHttpResult::kSuccess);

  if (next_page_token) {
    CallListContactPeople(next_page_token);
    return;
  }

  CD_LOG(VERBOSE, Feature::NS) << __func__ << ": Download of "
                               << contacts_.size() << " contacts succeeded.";
  RecordContactDownloadResultMetrics(/*success=*/true, current_page_number_,
                                     start_timestamp_);
  RecordContactDistributionMetrics(contacts_);

  // Remove device contacts if the feature flag is disabled.
  if (!base::FeatureList::IsEnabled(features::kNearbySharingDeviceContacts)) {
    size_t initial_num_contacts = contacts_.size();
    std::erase_if(
        contacts_, [](const nearby::sharing::proto::ContactRecord& contact) {
          return contact.type() ==
                 nearby::sharing::proto::ContactRecord::DEVICE_CONTACT;
        });
    CD_LOG(VERBOSE, Feature::NS)
        << __func__ << ": Removed " << initial_num_contacts - contacts_.size()
        << " device contacts.";
  }

  // Remove unreachable contacts.
  size_t initial_num_contacts = contacts_.size();
  std::erase_if(contacts_,
                [](const nearby::sharing::proto::ContactRecord& contact) {
                  return !contact.is_reachable();
                });
  uint32_t num_unreachable_contacts_filtered_out =
      initial_num_contacts - contacts_.size();
  CD_LOG(VERBOSE, Feature::NS)
      << __func__ << ": Removed " << num_unreachable_contacts_filtered_out
      << " unreachable contacts.";

  Succeed(std::move(contacts_), num_unreachable_contacts_filtered_out);
}

void NearbyShareContactDownloaderImpl::OnListContactPeopleFailure(
    ash::nearby::NearbyHttpError error) {
  timer_.Stop();
  client_.reset();
  RecordListContactPeopleResultMetrics(
      ash::nearby::NearbyHttpErrorToResult(error));
  RecordContactDownloadResultMetrics(/*success=*/false, current_page_number_,
                                     start_timestamp_);

  CD_LOG(ERROR, Feature::NS)
      << __func__ << ": Contact download RPC call failed with error " << error
      << " fetching page number " << current_page_number_;
  Fail();
}

void NearbyShareContactDownloaderImpl::OnListContactPeopleTimeout() {
  client_.reset();
  RecordListContactPeopleResultMetrics(ash::nearby::NearbyHttpResult::kTimeout);
  RecordContactDownloadResultMetrics(/*success=*/false, current_page_number_,
                                     start_timestamp_);

  CD_LOG(ERROR, Feature::NS)
      << __func__ << ": Contact download RPC call timed out.";
  Fail();
}

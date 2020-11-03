// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/contacts/nearby_share_contact_downloader_impl.h"

#include <algorithm>
#include <utility>

#include "base/memory/ptr_util.h"
#include "chrome/browser/nearby_sharing/client/nearby_share_client.h"
#include "chrome/browser/nearby_sharing/common/nearby_share_features.h"
#include "chrome/browser/nearby_sharing/logging/logging.h"

namespace {

void RecordListContactPeopleResultMetrics(NearbyShareHttpResult result,
                                          size_t current_page_number) {
  // TODO(https://crbug.com/1105579): Record a histogram value for each result.
  // TODO(https://crbug.com/1105579): On failure, record a histogram value for
  // the page that the request failed on.
}

void RecordContactDownloadMetrics(
    const std::vector<nearbyshare::proto::ContactRecord>& contacts,
    size_t num_pages) {
  // TODO(https://crbug.com/1105579): Record a histogram for the total number of
  // pages needed, the ratio of contact types, and the ratio of (un)reachable
  // contacts.
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
  if (test_factory_)
    return test_factory_->CreateInstance(device_id, timeout, client_factory,
                                         std::move(success_callback),
                                         std::move(failure_callback));

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
  NS_LOG(VERBOSE) << __func__ << ": Starting contacts download.";
  CallListContactPeople(/*next_page_token=*/base::nullopt);
}

void NearbyShareContactDownloaderImpl::CallListContactPeople(
    const base::Optional<std::string>& next_page_token) {
  ++current_page_number_;
  NS_LOG(VERBOSE) << __func__
                  << ": Making ListContactPeople RPC call to fetch page number "
                  << current_page_number_
                  << " with page token: " << next_page_token.value_or("[null]");
  timer_.Start(
      FROM_HERE, timeout_,
      base::BindOnce(
          &NearbyShareContactDownloaderImpl::OnListContactPeopleTimeout,
          base::Unretained(this)));

  nearbyshare::proto::ListContactPeopleRequest request;
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
    const nearbyshare::proto::ListContactPeopleResponse& response) {
  timer_.Stop();
  contacts_.insert(contacts_.end(), response.contact_records().begin(),
                   response.contact_records().end());
  base::Optional<std::string> next_page_token =
      response.next_page_token().empty()
          ? base::nullopt
          : base::make_optional<std::string>(response.next_page_token());
  client_.reset();
  RecordListContactPeopleResultMetrics(NearbyShareHttpResult::kSuccess,
                                       current_page_number_);

  if (next_page_token) {
    CallListContactPeople(next_page_token);
    return;
  }

  NS_LOG(VERBOSE) << __func__ << ": Download of " << contacts_.size()
                  << " contacts succeeded.";
  RecordContactDownloadMetrics(contacts_, current_page_number_);

  // Remove device contacts if the feature flag is disabled.
  if (!base::FeatureList::IsEnabled(features::kNearbySharingDeviceContacts)) {
    size_t initial_num_contacts = contacts_.size();
    contacts_.erase(
        std::remove_if(
            contacts_.begin(), contacts_.end(),
            [](const nearbyshare::proto::ContactRecord& contact) {
              return contact.type() ==
                     nearbyshare::proto::ContactRecord::DEVICE_CONTACT;
            }),
        contacts_.end());
    NS_LOG(VERBOSE) << __func__ << ": Removed "
                    << initial_num_contacts - contacts_.size()
                    << " device contacts.";
  }

  // Remove unreachable contacts.
  size_t initial_num_contacts = contacts_.size();
  contacts_.erase(
      std::remove_if(contacts_.begin(), contacts_.end(),
                     [](const nearbyshare::proto::ContactRecord& contact) {
                       return !contact.is_reachable();
                     }),
      contacts_.end());
  uint32_t num_unreachable_contacts_filtered_out =
      initial_num_contacts - contacts_.size();
  NS_LOG(VERBOSE) << __func__ << ": Removed "
                  << num_unreachable_contacts_filtered_out
                  << " unreachable contacts.";

  Succeed(std::move(contacts_), num_unreachable_contacts_filtered_out);
}

void NearbyShareContactDownloaderImpl::OnListContactPeopleFailure(
    NearbyShareHttpError error) {
  timer_.Stop();
  client_.reset();
  RecordListContactPeopleResultMetrics(NearbyShareHttpErrorToResult(error),
                                       current_page_number_);

  NS_LOG(ERROR) << __func__ << ": Contact download RPC call failed with error "
                << error << " fetching page number " << current_page_number_;
  Fail();
}

void NearbyShareContactDownloaderImpl::OnListContactPeopleTimeout() {
  client_.reset();
  RecordListContactPeopleResultMetrics(NearbyShareHttpResult::kTimeout,
                                       current_page_number_);

  NS_LOG(ERROR) << __func__ << ": Contact download RPC call timed out.";
  Fail();
}

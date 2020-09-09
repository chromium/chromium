// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/contacts/nearby_share_contact_downloader_impl.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "chrome/browser/nearby_sharing/client/nearby_share_client.h"
#include "chrome/browser/nearby_sharing/logging/logging.h"

namespace {

const char kDeviceIdPrefix[] = "users/me/devices/";

void RecordGetDeviceStateResultMetrics(NearbyShareHttpResult result) {
  // TODO(https://crbug.com/1105579): Record a histogram value for each result.
}

void RecordListContactPeopleResultMetrics(NearbyShareHttpResult result,
                                          size_t current_page_number) {
  // TODO(https://crbug.com/1105579): Record a histogram value for each result.
  // TODO(https://crbug.com/1105579): On failure, record a histogram value for
  // the page that the request failed on.
}

}  // namespace

// static
NearbyShareContactDownloaderImpl::Factory*
    NearbyShareContactDownloaderImpl::Factory::test_factory_ = nullptr;

// static
std::unique_ptr<NearbyShareContactDownloader>
NearbyShareContactDownloaderImpl::Factory::Create(
    bool only_download_if_changed,
    const std::string& device_id,
    base::TimeDelta timeout,
    NearbyShareClientFactory* client_factory,
    SuccessCallback success_callback,
    FailureCallback failure_callback) {
  if (test_factory_)
    return test_factory_->CreateInstance(
        only_download_if_changed, device_id, timeout, client_factory,
        std::move(success_callback), std::move(failure_callback));

  return base::WrapUnique(new NearbyShareContactDownloaderImpl(
      only_download_if_changed, device_id, timeout, client_factory,
      std::move(success_callback), std::move(failure_callback)));
}

// static
void NearbyShareContactDownloaderImpl::Factory::SetFactoryForTesting(
    Factory* test_factory) {
  test_factory_ = test_factory;
}

NearbyShareContactDownloaderImpl::Factory::~Factory() = default;

NearbyShareContactDownloaderImpl::NearbyShareContactDownloaderImpl(
    bool only_download_if_changed,
    const std::string& device_id,
    base::TimeDelta timeout,
    NearbyShareClientFactory* client_factory,
    SuccessCallback success_callback,
    FailureCallback failure_callback)
    : NearbyShareContactDownloader(only_download_if_changed,
                                   device_id,
                                   std::move(success_callback),
                                   std::move(failure_callback)),
      timeout_(timeout),
      client_factory_(client_factory) {}

NearbyShareContactDownloaderImpl::~NearbyShareContactDownloaderImpl() = default;

void NearbyShareContactDownloaderImpl::OnRun() {
  NS_LOG(VERBOSE) << __func__ << ": Starting contacts download.";
  CheckIfContactsChanged();
}

void NearbyShareContactDownloaderImpl::CheckIfContactsChanged() {
  NS_LOG(VERBOSE) << __func__
                  << ": Checking if contacts have changed since last upload.";
  timer_.Start(
      FROM_HERE, timeout_,
      base::BindOnce(&NearbyShareContactDownloaderImpl::OnGetDeviceStateTimeout,
                     base::Unretained(this)));

  nearbyshare::proto::GetDeviceStateRequest request;
  request.set_parent(kDeviceIdPrefix + device_id());

  client_ = client_factory_->CreateInstance();
  client_->GetDeviceState(
      request,
      base::BindOnce(&NearbyShareContactDownloaderImpl::OnGetDeviceStateSuccess,
                     base::Unretained(this)),
      base::BindOnce(&NearbyShareContactDownloaderImpl::OnGetDeviceStateFailure,
                     base::Unretained(this)));
}

void NearbyShareContactDownloaderImpl::OnGetDeviceStateSuccess(
    const nearbyshare::proto::GetDeviceStateResponse& response) {
  timer_.Stop();

  did_contacts_change_since_last_upload_ = response.are_contacts_changed();
  NS_LOG(VERBOSE) << __func__ << ": Did contacts change since last upload? "
                  << (did_contacts_change_since_last_upload_ ? "Yes." : "No.");

  client_.reset();
  RecordGetDeviceStateResultMetrics(NearbyShareHttpResult::kSuccess);

  if (only_download_if_changed() && !did_contacts_change_since_last_upload_) {
    NS_LOG(VERBOSE) << __func__
                    << ": Contacts did not change; no download needed.";
    Succeed(did_contacts_change_since_last_upload_, /*contacts=*/base::nullopt);
    return;
  }

  CallListContactPeople(/*next_page_token=*/base::nullopt);
}

void NearbyShareContactDownloaderImpl::OnGetDeviceStateFailure(
    NearbyShareHttpError error) {
  timer_.Stop();
  client_.reset();
  RecordGetDeviceStateResultMetrics(NearbyShareHttpErrorToResult(error));

  NS_LOG(ERROR) << __func__ << ": GetDeviceState RPC failed with error "
                << error;
  Fail();
}

void NearbyShareContactDownloaderImpl::OnGetDeviceStateTimeout() {
  client_.reset();
  RecordGetDeviceStateResultMetrics(NearbyShareHttpResult::kTimeout);

  NS_LOG(ERROR) << __func__ << ": GetDeviceState RPC timed out.";
  Fail();
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

  NS_LOG(VERBOSE)
      << __func__ << ": Download of " << contacts_.size()
      << " contacts succeeded. Did contacts change since last upload? "
      << (did_contacts_change_since_last_upload_ ? "Yes." : "No.");

  // TODO(https://crbug.com/1105579): Record a histogram for the total number of
  // pages needed.

  Succeed(did_contacts_change_since_last_upload_, std::move(contacts_));
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

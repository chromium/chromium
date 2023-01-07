// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/preloading/prefetch/prefetch_proxy/prefetch_container.h"

#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "chrome/browser/preloading/prefetch/prefetch_proxy/prefetch_proxy_cookie_listener.h"
#include "chrome/browser/preloading/prefetch/prefetch_proxy/prefetch_proxy_network_context.h"
#include "chrome/browser/preloading/prefetch/prefetch_proxy/prefetch_proxy_params.h"
#include "chrome/browser/preloading/prefetch/prefetch_proxy/prefetch_type.h"
#include "chrome/browser/preloading/prefetch/prefetch_proxy/prefetched_mainframe_response_container.h"
#include "chrome/browser/profiles/profile.h"
#include "url/gurl.h"

PrefetchContainer::PrefetchContainer(const GURL& url,
                                     const PrefetchType& prefetch_type,
                                     size_t original_prediction_index)
    : url_(url),
      prefetch_type_(prefetch_type),
      original_prediction_index_(original_prediction_index),
      request_id_(base::UnguessableToken::Create().ToString()) {}

PrefetchContainer::~PrefetchContainer() = default;

void PrefetchContainer::ChangePrefetchType(
    const PrefetchType& new_prefetch_type) {
  // The only supported state change is to upgrade a private cross origin
  // prefetch to allow for subresources to be prefetched. Other state changes
  // would require changing |network_context_| which is not supported yet.
  bool supported_state_change =
      prefetch_type_.IsIsolatedNetworkContextRequired() &&
      prefetch_type_.IsProxyRequired() &&
      !prefetch_type_.AllowedToPrefetchSubresources() &&
      new_prefetch_type.IsIsolatedNetworkContextRequired() &&
      new_prefetch_type.IsProxyRequired() &&
      new_prefetch_type.AllowedToPrefetchSubresources();

  // TODO(crbug.com/1278104): Add support for other prefetch type state changes.
  if (supported_state_change) {
    prefetch_type_ = new_prefetch_type;
  }

  base::UmaHistogramBoolean("PrefetchProxy.WasPrefetchTypeStateChangeValid",
                            supported_state_change);
}

PrefetchProxyPrefetchStatus PrefetchContainer::GetPrefetchStatus() const {
  DCHECK(prefetch_status_);
  return prefetch_status_.value();
}

void PrefetchContainer::RegisterCookieListener(
    base::OnceCallback<void(const GURL&)> on_cookie_change_callback,
    network::mojom::CookieManager* cookie_manager) {
  cookie_listener_ = PrefetchProxyCookieListener::MakeAndRegister(
      url_, std::move(on_cookie_change_callback), cookie_manager);
}

void PrefetchContainer::StopCookieListener() {
  DCHECK(cookie_listener_);
  cookie_listener_->StopListening();
}

bool PrefetchContainer::HaveCookiesChanged() const {
  if (cookie_listener_)
    return cookie_listener_->HaveCookiesChanged();
  return false;
}

bool PrefetchContainer::IsPrefetchedResponseValid(
    base::TimeDelta cacheable_duration) const {
  return prefetch_received_time_.has_value() &&
         base::TimeTicks::Now() <
             prefetch_received_time_.value() + cacheable_duration;
}

void PrefetchContainer::SetPrefetchedResponse(
    std::unique_ptr<PrefetchedMainframeResponseContainer> prefetched_response) {
  DCHECK(!prefetched_response_);
  DCHECK(!is_decoy_);
  prefetch_received_time_ = base::TimeTicks::Now();
  prefetched_response_ = std::move(prefetched_response);
}

std::unique_ptr<PrefetchedMainframeResponseContainer>
PrefetchContainer::ReleasePrefetchedResponse() {
  DCHECK(prefetch_received_time_);
  DCHECK(prefetched_response_);
  DCHECK(IsPrefetchedResponseValid(PrefetchProxyCacheableDuration()));

  prefetch_received_time_.reset();
  return std::move(prefetched_response_);
}

std::unique_ptr<PrefetchedMainframeResponseContainer>
PrefetchContainer::ClonePrefetchedResponse() const {
  DCHECK(prefetch_received_time_);
  DCHECK(prefetched_response_);
  return prefetched_response_->Clone();
}

void PrefetchContainer::SetNoStatePrefetchStatus(
    NoStatePrefetchStatus no_state_prefetch_status) {
  DCHECK(prefetch_type_.AllowedToPrefetchSubresources());

  // The only valid state changes are: kNotStarted to kInProgress, kInProgress
  // to kSucceeded, and kInProgress to kFailed.
  DCHECK((no_state_prefetch_status_ == NoStatePrefetchStatus::kNotStarted &&
          no_state_prefetch_status == NoStatePrefetchStatus::kInProgress) ||
         (no_state_prefetch_status_ == NoStatePrefetchStatus::kInProgress &&
          no_state_prefetch_status == NoStatePrefetchStatus::kSucceeded) ||
         (no_state_prefetch_status_ == NoStatePrefetchStatus::kInProgress &&
          no_state_prefetch_status == NoStatePrefetchStatus::kFailed));

  no_state_prefetch_status_ = no_state_prefetch_status;
}

void PrefetchContainer::CreateNetworkContextForPrefetch(Profile* profile) {
  network_context_ = std::make_unique<PrefetchProxyNetworkContext>(
      profile, prefetch_type_.IsIsolatedNetworkContextRequired(),
      prefetch_type_.IsProxyRequired() &&
          !prefetch_type_.IsProxyBypassedForTesting());
}

std::unique_ptr<PrefetchProxyNetworkContext>
PrefetchContainer::ReleaseNetworkContext() {
  DCHECK(network_context_);
  return std::move(network_context_);
}

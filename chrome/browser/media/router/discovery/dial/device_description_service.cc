// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/discovery/dial/device_description_service.h"
#include "base/bind.h"

#include <map>
#include <utility>
#include <vector>

#if DCHECK_IS_ON()
#include <sstream>
#endif

#include "base/stl_util.h"
#include "chrome/browser/media/router/discovery/dial/device_description_fetcher.h"
#include "chrome/browser/media/router/discovery/dial/safe_dial_device_description_parser.h"
#include "chrome/browser/media/router/media_router_metrics.h"
#include "net/base/ip_address.h"
#include "url/gurl.h"

namespace media_router {

using ParsingError = SafeDialDeviceDescriptionParser::ParsingError;

namespace {

// How long to cache a device description.
constexpr int kDeviceDescriptionCacheTimeHours = 12;

// Time interval to clean up cache entries.
constexpr int kCacheCleanUpTimeoutMins = 30;

// Maximum size on the number of cached entries.
constexpr int kCacheMaxEntries = 256;

#if DCHECK_IS_ON()
std::string CachedDeviceDescriptionToString(
    const media_router::DeviceDescriptionService::CacheEntry& cached_data) {
  std::stringstream ss;
  ss << "CachedDialDeviceDescription [unique_id]: "
     << cached_data.description_data.unique_id
     << " [friendly_name]: " << cached_data.description_data.friendly_name
     << " [model_name]: " << cached_data.description_data.model_name
     << " [app_url]: " << cached_data.description_data.app_url
     << " [expire_time]: " << cached_data.expire_time
     << " [config_id]: " << cached_data.config_id;

  return ss.str();
}
#endif

// Checks mandatory fields. Returns ParsingError::kNone if device description is
// valid; Otherwise returns specific error type.
ParsingError ValidateParsedDeviceDescription(
    const GURL& device_description_url,
    const ParsedDialDeviceDescription& description_data) {
  if (description_data.unique_id.empty()) {
    return ParsingError::kMissingUniqueId;
  }
  if (description_data.friendly_name.empty()) {
    return ParsingError::kMissingFriendlyName;
  }
  if (!description_data.app_url.is_valid()) {
    return ParsingError::kMissingAppUrl;
  }

  // TODO(crbug.com/679432): Get the device IP from the SSDP response.
  net::IPAddress device_ip;
  if (!device_ip.AssignFromIPLiteral(
          device_description_url.HostNoBracketsPiece()) ||
      !DialDeviceData::IsValidDialAppUrl(description_data.app_url, device_ip)) {
    return ParsingError::kInvalidAppUrl;
  }
  return ParsingError::kNone;
}

}  // namespace

DeviceDescriptionService::DeviceDescriptionService(
    const DeviceDescriptionParseSuccessCallback& success_cb,
    const DeviceDescriptionParseErrorCallback& error_cb)
    : success_cb_(success_cb), error_cb_(error_cb) {}

DeviceDescriptionService::~DeviceDescriptionService() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (pending_device_count_ > 0) {
    DLOG(WARNING) << "Fail to finish parsing " << pending_device_count_
                  << " devices.";
  }
}

void DeviceDescriptionService::GetDeviceDescriptions(
    const std::vector<DialDeviceData>& devices) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::map<std::string, std::unique_ptr<DeviceDescriptionFetcher>>
      existing_fetcher_map;
  for (auto& fetcher_it : device_description_fetcher_map_) {
    std::string device_label = fetcher_it.first;
    const auto& device_it =
        std::find_if(devices.begin(), devices.end(),
                     [&device_label](const DialDeviceData& device_data) {
                       return device_data.label() == device_label;
                     });
    if (device_it == devices.end() ||
        device_it->device_description_url() ==
            fetcher_it.second->device_description_url()) {
      existing_fetcher_map.insert(
          std::make_pair(device_label, std::move(fetcher_it.second)));
    }
  }

  // Remove all out dated fetchers.
  device_description_fetcher_map_ = std::move(existing_fetcher_map);

  for (const auto& device_data : devices) {
    auto* cache_entry = CheckAndUpdateCache(device_data);
    if (cache_entry) {
      // Get device description from cache.
      success_cb_.Run(device_data, cache_entry->description_data);
      continue;
    }

    FetchDeviceDescription(device_data);
  }

  // Start a clean up timer.
  if (!clean_up_timer_) {
    clean_up_timer_.reset(new base::RepeatingTimer());
    clean_up_timer_->Start(
        FROM_HERE, base::TimeDelta::FromMinutes(kCacheCleanUpTimeoutMins), this,
        &DeviceDescriptionService::CleanUpCacheEntries);
  }
}

void DeviceDescriptionService::CleanUpCacheEntries() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::Time now = GetNow();

  DVLOG(2) << "Before clean up, cache size: " << description_cache_.size();
  base::EraseIf(description_cache_,
                [&now](const std::pair<std::string, CacheEntry>& cache_pair) {
                  return cache_pair.second.expire_time < now;
                });
  DVLOG(2) << "After clean up, cache size: " << description_cache_.size();

  if (description_cache_.empty() && device_description_fetcher_map_.empty()) {
    DVLOG(2) << "Cache is empty, stop clean up timer...";
    clean_up_timer_.reset();
  }
}

void DeviceDescriptionService::FetchDeviceDescription(
    const DialDeviceData& device_data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Existing Fetcher.
  const auto& it = device_description_fetcher_map_.find(device_data.label());
  if (it != device_description_fetcher_map_.end())
    return;

  auto device_description_fetcher = std::make_unique<DeviceDescriptionFetcher>(
      device_data.device_description_url(),
      base::BindOnce(
          &DeviceDescriptionService::OnDeviceDescriptionFetchComplete,
          base::Unretained(this), device_data),
      base::BindOnce(&DeviceDescriptionService::OnDeviceDescriptionFetchError,
                     base::Unretained(this), device_data));

  device_description_fetcher->Start();
  device_description_fetcher_map_.insert(std::make_pair(
      device_data.label(), std::move(device_description_fetcher)));

  pending_device_count_++;
}

void DeviceDescriptionService::ParseDeviceDescription(
    const DialDeviceData& device_data,
    const DialDeviceDescriptionData& description_data) {
  device_description_parser_.Parse(
      description_data.device_description, description_data.app_url,
      base::BindOnce(&DeviceDescriptionService::OnParsedDeviceDescription,
                     base::Unretained(this), device_data));
}

const DeviceDescriptionService::CacheEntry*
DeviceDescriptionService::CheckAndUpdateCache(
    const DialDeviceData& device_data) {
  const auto& it = description_cache_.find(device_data.label());
  if (it == description_cache_.end())
    return nullptr;

  // If the entry's config_id does not match, or it has expired, remove it.
  if (it->second.config_id != device_data.config_id() ||
      GetNow() >= it->second.expire_time) {
    DVLOG(2) << "Removing invalid entry " << it->first;
    description_cache_.erase(it);
    return nullptr;
  }

  // Entry is valid.
  return &it->second;
}

void DeviceDescriptionService::OnParsedDeviceDescription(
    const DialDeviceData& device_data,
    const ParsedDialDeviceDescription& device_description,
    SafeDialDeviceDescriptionParser::ParsingError parsing_error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  pending_device_count_--;

  if (parsing_error != ParsingError::kNone) {
    MediaRouterMetrics::RecordDialParsingError(parsing_error);
    error_cb_.Run(device_data, "Failed to parse device description XML");
    return;
  }

  ParsingError error = ValidateParsedDeviceDescription(
      device_data.device_description_url(), device_description);

  if (error != ParsingError::kNone) {
    DLOG(WARNING) << "Device description failed to validate. "
                     "MediaRouterDialParsingError code: "
                  << static_cast<int>(error);
    MediaRouterMetrics::RecordDialParsingError(error);
    error_cb_.Run(device_data, "Failed to process fetch result");
    return;
  }

  if (description_cache_.size() >= kCacheMaxEntries) {
    success_cb_.Run(device_data, device_description);
    return;
  }

  CacheEntry cached_description_data;
  cached_description_data.expire_time =
      GetNow() + base::TimeDelta::FromHours(kDeviceDescriptionCacheTimeHours);
  cached_description_data.config_id = device_data.config_id();
  cached_description_data.description_data = device_description;

#if DCHECK_IS_ON()
  DVLOG(2) << "Caching device description for " << device_data.label()
           << "... device description was: "
           << CachedDeviceDescriptionToString(cached_description_data);
#endif

  description_cache_.insert(
      std::make_pair(device_data.label(), cached_description_data));

  success_cb_.Run(device_data, device_description);
}

void DeviceDescriptionService::OnDeviceDescriptionFetchComplete(
    const DialDeviceData& device_data,
    const DialDeviceDescriptionData& description_data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  ParseDeviceDescription(device_data, description_data);
  device_description_fetcher_map_.erase(device_data.label());
}

void DeviceDescriptionService::OnDeviceDescriptionFetchError(
    const DialDeviceData& device_data,
    const std::string& error_message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOG(2) << "OnDeviceDescriptionFetchError [label]: " << device_data.label();

  error_cb_.Run(device_data, error_message);
  device_description_fetcher_map_.erase(device_data.label());
}

base::Time DeviceDescriptionService::GetNow() {
  return base::Time::Now();
}

DeviceDescriptionService::CacheEntry::CacheEntry() : config_id(-1) {}
DeviceDescriptionService::CacheEntry::CacheEntry(const CacheEntry& other) =
    default;
DeviceDescriptionService::CacheEntry::~CacheEntry() = default;

}  // namespace media_router

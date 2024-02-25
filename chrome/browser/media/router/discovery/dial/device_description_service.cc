// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/discovery/dial/device_description_service.h"

#include <map>
#include <memory>
#include <utility>
#include <vector>

#if DCHECK_IS_ON()
#include <sstream>
#endif

#include "base/functional/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/ranges/algorithm.h"
#include "chrome/browser/media/router/discovery/dial/device_description_fetcher.h"
#include "chrome/browser/media/router/discovery/dial/safe_dial_device_description_parser.h"
#include "net/base/ip_address.h"
#include "url/gurl.h"

namespace media_router {

using ParsingResult = SafeDialDeviceDescriptionParser::ParsingResult;

namespace {

// How long to cache a device description.
constexpr int kDeviceDescriptionCacheTimeHours = 12;

// Time interval to clean up cache entries.
constexpr int kCacheCleanUpTimeoutMins = 30;

// Maximum size on the number of cached entries.
constexpr int kCacheMaxEntries = 256;

// Checks mandatory fields. Returns ParsingResult::kSuccess if device
// description is valid; Otherwise returns specific error type.
ParsingResult ValidateParsedDeviceDescription(
    const DialDeviceData& device_data,
    const ParsedDialDeviceDescription& description_data) {
  if (description_data.unique_id.empty()) {
    return ParsingResult::kMissingUniqueId;
  }
  if (description_data.friendly_name.empty()) {
    return ParsingResult::kMissingFriendlyName;
  }
  if (!description_data.app_url.is_valid()) {
    return ParsingResult::kMissingAppUrl;
  }
  if (!device_data.IsValidUrl(description_data.app_url)) {
    return ParsingResult::kInvalidAppUrl;
  }
  return ParsingResult::kSuccess;
}

void RecordDialParsingResult(
    SafeDialDeviceDescriptionParser::ParsingResult parse_result) {
  UMA_HISTOGRAM_ENUMERATION("MediaRouter.Dial.DeviceDescriptionParsingResult",
                            parse_result);
}

}  // namespace

DeviceDescriptionService::DeviceDescriptionService(
    const DeviceDescriptionParseSuccessCallback& success_cb,
    const DeviceDescriptionParseErrorCallback& error_cb)
    : success_cb_(success_cb), error_cb_(error_cb) {}

DeviceDescriptionService::~DeviceDescriptionService() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void DeviceDescriptionService::GetDeviceDescriptions(
    const std::vector<DialDeviceData>& devices) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::map<std::string, std::unique_ptr<DeviceDescriptionFetcher>>
      existing_fetcher_map;
  for (auto& fetcher_it : device_description_fetcher_map_) {
    std::string device_label = fetcher_it.first;
    const auto& device_it =
        base::ranges::find(devices, device_label, &DialDeviceData::label);
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
    clean_up_timer_ = std::make_unique<base::RepeatingTimer>();
    clean_up_timer_->Start(FROM_HERE, base::Minutes(kCacheCleanUpTimeoutMins),
                           this,
                           &DeviceDescriptionService::CleanUpCacheEntries);
  }
}

void DeviceDescriptionService::CleanUpCacheEntries() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::Time now = GetNow();
  std::erase_if(description_cache_,
                [&now](const std::pair<std::string, CacheEntry>& cache_pair) {
                  return cache_pair.second.expire_time < now;
                });
  if (description_cache_.empty() && device_description_fetcher_map_.empty()) {
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

  std::unique_ptr<DeviceDescriptionFetcher> device_description_fetcher =
      CreateFetcher(
          device_data,
          base::BindOnce(
              &DeviceDescriptionService::OnDeviceDescriptionFetchComplete,
              base::Unretained(this), device_data),
          base::BindOnce(
              &DeviceDescriptionService::OnDeviceDescriptionFetchError,
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

std::unique_ptr<DeviceDescriptionFetcher>
DeviceDescriptionService::CreateFetcher(
    const DialDeviceData& device_data,
    base::OnceCallback<void(const DialDeviceDescriptionData&)> success_cb,
    base::OnceCallback<void(const std::string&)> error_cb) {
  return std::make_unique<DeviceDescriptionFetcher>(
      device_data, std::move(success_cb), std::move(error_cb));
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
    description_cache_.erase(it);
    return nullptr;
  }

  // Entry is valid.
  return &it->second;
}

void DeviceDescriptionService::OnParsedDeviceDescription(
    const DialDeviceData& device_data,
    const ParsedDialDeviceDescription& device_description,
    SafeDialDeviceDescriptionParser::ParsingResult parsing_result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  pending_device_count_--;
  if (parsing_result != ParsingResult::kSuccess) {
    RecordDialParsingResult(parsing_result);
    error_cb_.Run(device_data, "Failed to parse device description XML");
    return;
  }

  parsing_result =
      ValidateParsedDeviceDescription(device_data, device_description);
  if (parsing_result != ParsingResult::kSuccess) {
    RecordDialParsingResult(parsing_result);
    error_cb_.Run(device_data, "Failed to process fetch result");
    return;
  }

  RecordDialParsingResult(ParsingResult::kSuccess);
  if (description_cache_.size() >= kCacheMaxEntries) {
    success_cb_.Run(device_data, device_description);
    return;
  }

  CacheEntry cached_description_data;
  cached_description_data.expire_time =
      GetNow() + base::Hours(kDeviceDescriptionCacheTimeHours);
  cached_description_data.config_id = device_data.config_id();
  cached_description_data.description_data = device_description;
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

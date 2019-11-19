// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/supervised_user_site_list.h"

#include <algorithm>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/json/json_file_value_serializer.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/post_task.h"
#include "base/values.h"
#include "url/gurl.h"

const int kLegacyWhitelistFormatVersion = 2;
const int kWhitelistFormatVersion = 1;

const char kEntryPointUrlKey[] = "entry_point_url";
const char kHostnameHashesKey[] = "hostname_hashes";
const char kLegacyWhitelistFormatVersionKey[] = "version";
const char kSitelistFormatVersionKey[] = "sitelist_version";
const char kWhitelistKey[] = "whitelist";

namespace {

std::unique_ptr<base::Value> ReadFileOnBlockingThread(
    const base::FilePath& path) {
  SCOPED_UMA_HISTOGRAM_TIMER("ManagedUsers.Whitelist.ReadDuration");
  JSONFileValueDeserializer deserializer(path);
  int error_code;
  std::string error_msg;
  std::unique_ptr<base::Value> value =
      deserializer.Deserialize(&error_code, &error_msg);
  if (!value) {
    LOG(ERROR) << "Couldn't load site list " << path.value() << ": "
               << error_msg;
  }
  return value;
}

std::vector<std::string> ConvertListValues(const base::ListValue* list_values) {
  std::vector<std::string> converted;
  if (list_values) {
    for (const auto& entry : *list_values) {
      std::string entry_string;
      if (!entry.GetAsString(&entry_string)) {
        LOG(ERROR) << "Invalid whitelist entry";
        continue;
      }

      converted.push_back(entry_string);
    }
  }
  return converted;
}

}  // namespace

SupervisedUserSiteList::HostnameHash::HostnameHash(
    const std::string& hostname) {
  base::SHA1HashBytes(reinterpret_cast<const unsigned char*>(hostname.c_str()),
                      hostname.size(), bytes_.data());
}

SupervisedUserSiteList::HostnameHash::HostnameHash(
    const std::vector<uint8_t>& bytes) {
  CHECK_GE(bytes.size(), base::kSHA1Length);
  std::copy(bytes.begin(), bytes.end(), bytes_.begin());
}

SupervisedUserSiteList::HostnameHash::HostnameHash(const HostnameHash& other) =
    default;

bool SupervisedUserSiteList::HostnameHash::operator==(
    const HostnameHash& rhs) const {
  return bytes_ == rhs.bytes_;
}

size_t SupervisedUserSiteList::HostnameHash::hash() const {
  // This just returns the first sizeof(size_t) bytes of |bytes_|.
  return *reinterpret_cast<const size_t*>(bytes_.data());
}

void SupervisedUserSiteList::Load(const std::string& id,
                                  const base::string16& title,
                                  const base::FilePath& large_icon_path,
                                  const base::FilePath& path,
                                  const LoadedCallback& callback) {
  base::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::ThreadPool(), base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&ReadFileOnBlockingThread, path),
      base::BindOnce(&SupervisedUserSiteList::OnJsonLoaded, id, title,
                     large_icon_path, path, base::TimeTicks::Now(), callback));
}

SupervisedUserSiteList::SupervisedUserSiteList(
    const std::string& id,
    const base::string16& title,
    const GURL& entry_point,
    const base::FilePath& large_icon_path,
    const base::ListValue* patterns,
    const base::ListValue* hostname_hashes)
    : SupervisedUserSiteList(id,
                             title,
                             entry_point,
                             large_icon_path,
                             ConvertListValues(patterns),
                             ConvertListValues(hostname_hashes)) {}

SupervisedUserSiteList::SupervisedUserSiteList(
    const std::string& id,
    const base::string16& title,
    const GURL& entry_point,
    const base::FilePath& large_icon_path,
    const std::vector<std::string>& patterns,
    const std::vector<std::string>& hostname_hashes)
    : id_(id),
      title_(title),
      entry_point_(entry_point),
      large_icon_path_(large_icon_path),
      patterns_(patterns) {
  for (const std::string& hostname_hash : hostname_hashes) {
    std::vector<uint8_t> hash_bytes;
    if (hostname_hash.size() != 2 * base::kSHA1Length ||
        !base::HexStringToBytes(hostname_hash, &hash_bytes)) {
      LOG(ERROR) << "Invalid hostname_hashes entry";
      continue;
    }
    DCHECK_EQ(base::kSHA1Length, hash_bytes.size());
    hostname_hashes_.push_back(HostnameHash(hash_bytes));
  }
}

SupervisedUserSiteList::~SupervisedUserSiteList() {
}

// static
void SupervisedUserSiteList::OnJsonLoaded(
    const std::string& id,
    const base::string16& title,
    const base::FilePath& large_icon_path,
    const base::FilePath& path,
    base::TimeTicks start_time,
    const SupervisedUserSiteList::LoadedCallback& callback,
    std::unique_ptr<base::Value> value) {
  if (!value)
    return;

  if (!start_time.is_null()) {
    UMA_HISTOGRAM_TIMES("ManagedUsers.Whitelist.JsonParseDuration",
                        base::TimeTicks::Now() - start_time);
  }

  base::DictionaryValue* dict = nullptr;
  if (!value->GetAsDictionary(&dict)) {
    LOG(ERROR) << "Whitelist " << path.value() << " is invalid";
    return;
  }

  int version = 0;
  if (!dict->GetInteger(kSitelistFormatVersionKey, &version)) {
    // TODO(bauerb): Remove this code once all whitelists have been updated to
    // the new version.
    if (!dict->GetInteger(kLegacyWhitelistFormatVersionKey, &version)) {
      LOG(ERROR) << "Whitelist " << path.value() << " has invalid or missing "
                 << "version";
      return;
    }
    if (version != kLegacyWhitelistFormatVersion) {
      LOG(ERROR) << "Whitelist " << path.value() << " has wrong legacy version "
                 << version << ", expected " << kLegacyWhitelistFormatVersion;
      return;
    }
  } else if (version != kWhitelistFormatVersion) {
    LOG(ERROR) << "Whitelist " << path.value() << " has wrong version "
               << version << ", expected " << kWhitelistFormatVersion;
    return;
  }

  std::string entry_point_url;
  dict->GetString(kEntryPointUrlKey, &entry_point_url);

  base::ListValue* patterns = nullptr;
  dict->GetList(kWhitelistKey, &patterns);

  base::ListValue* hostname_hashes = nullptr;
  dict->GetList(kHostnameHashesKey, &hostname_hashes);

  callback.Run(base::WrapRefCounted(
      new SupervisedUserSiteList(id, title, GURL(entry_point_url),
                                 large_icon_path, patterns, hostname_hashes)));
}

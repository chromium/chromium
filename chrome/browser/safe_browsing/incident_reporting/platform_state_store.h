// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// An interface to platform-specific storage of IncidentReportingService prune
// state.

#ifndef CHROME_BROWSER_SAFE_BROWSING_INCIDENT_REPORTING_PLATFORM_STATE_STORE_H_
#define CHROME_BROWSER_SAFE_BROWSING_INCIDENT_REPORTING_PLATFORM_STATE_STORE_H_

#include <stdint.h>

#include <memory>
#include <optional>
#include <string>

#include "base/values.h"
#include "build/build_config.h"

// Certain platforms provide their own storage of protobuf-serialized prune
// state. On platforms where it is not supported, Load() and Store() are noops.
#if BUILDFLAG(IS_WIN)
// Store the state in the registry on Windows.
#define USE_PLATFORM_STATE_STORE
#endif

class Profile;

namespace safe_browsing {
namespace platform_state_store {

// Loads the platform-specific storage for |profile|. Returns std::nullopt if
// there is no such storage for the current platform or in case of error;
// otherwise, a (possibly empty) dictionary.
std::optional<base::Value::Dict> Load(Profile* profile);

// Stores the state for |profile| in |incidents_sent| into platform-specific
// storage if there is such for the current platform.
void Store(Profile* profile, const base::Value::Dict& incidents_sent);

#if defined(USE_PLATFORM_STATE_STORE)

// All declarations and definitions from this point forward are for use by
// implementations in platform-specific source files, or are exposed for the
// sake of testing.

// The result of loading platform-specific state. This is a histogram type; do
// not reorder.
enum class PlatformStateStoreLoadResult : int32_t {
  SUCCESS = 0,
  CLEARED_DATA = 1,
  CLEARED_NO_DATA = 2,
  DATA_CLEAR_FAILED = 3,
  OPEN_FAILED = 4,
  READ_FAILED = 5,
  PARSE_ERROR = 6,
  NUM_RESULTS
};

// A platform-specific function to read store data for |profile| into |data|.
// Returns SUCCESS if |data| was populated, or a load result value indicating
// why no data was read.
PlatformStateStoreLoadResult ReadStoreData(Profile* profile, std::string* data);

// A platform-specific function to write store data for |profile| from |data|.
void WriteStoreData(Profile* profile, const std::string& data);

// Serializes the |incidents_sent| preference into |data|, replacing its
// contents. Exposed for testing.
void SerializeIncidentsSent(const base::Value::Dict& incidents_sent,
                            std::string* data);

// Deserializes |data| into |value_dict|. Returns SUCCESS if |data| is empty or
// fully processed. Exposed for testing.
PlatformStateStoreLoadResult DeserializeIncidentsSent(
    const std::string& data,
    base::Value::Dict& value_dict);

#endif  // USE_PLATFORM_STATE_STORE

}  // namespace platform_state_store
}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_INCIDENT_REPORTING_PLATFORM_STATE_STORE_H_

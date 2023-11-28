// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_PAIR_COMPANION_APP_COMPANION_APP_PARSER_H_
#define ASH_QUICK_PAIR_COMPANION_APP_COMPANION_APP_PARSER_H_

#include <optional>

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"

namespace ash {
namespace quick_pair {

class Device;
class DeviceMetadata;

// CompanionAppParser finds the name of a device's companion app
// Call GetAppPackageName to use
class CompanionAppParser {
 public:
  CompanionAppParser();
  CompanionAppParser(const CompanionAppParser&) = delete;
  CompanionAppParser& operator=(const CompanionAppParser&) = delete;
  ~CompanionAppParser();

  // Finds the name of the given device's companion app
  // The optional string in the callback will be null if an error
  // occurs or if no companion app for this device was found
  void GetAppPackageName(scoped_refptr<Device> device,
                         base::OnceCallback<void(std::optional<std::string>)>
                             on_companion_app_parsed);

 private:
  void OnDeviceMetadataRetrieved(
      scoped_refptr<Device> device,
      base::OnceCallback<void(std::optional<std::string>)> callback,
      DeviceMetadata* device_metadata,
      bool retryable_err);

  std::optional<std::string> GetCompanionAppExtra(
      const std::string& intent_as_string);

  base::WeakPtrFactory<CompanionAppParser> weak_pointer_factory_{this};
};

}  // namespace quick_pair
}  // namespace ash

#endif  // ASH_QUICK_PAIR_COMPANION_APP_COMPANION_APP_PARSER_H_

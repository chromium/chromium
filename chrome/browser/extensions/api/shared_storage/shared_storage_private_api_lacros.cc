// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/shared_storage/shared_storage_private_api.h"

#include "base/values.h"
#include "chrome/common/extensions/api/shared_storage_private.h"
#include "chromeos/crosapi/mojom/prefs.mojom.h"
#include "chromeos/lacros/lacros_service.h"

// TODO(b/231890240): Once Terminal SWA runs in lacros rather than ash, we can
// migrate gnubbyd back to using chrome.storage.local and remove this private
// API.

namespace shared_api = extensions::api::shared_storage_private;

namespace extensions {
namespace {

constexpr char kErrorNotAvailable[] = "crosapi: Prefs API not available";
constexpr char kErrorFetching[] = "crosapi: Error fetching pref shared_storage";

}  // namespace

SharedStoragePrivateGetFunction::SharedStoragePrivateGetFunction() = default;
SharedStoragePrivateGetFunction::~SharedStoragePrivateGetFunction() = default;

ExtensionFunction::ResponseAction SharedStoragePrivateGetFunction::Run() {
  auto* lacros_service = chromeos::LacrosService::Get();
  if (!lacros_service ||
      !lacros_service->IsAvailable<crosapi::mojom::Prefs>()) {
    LOG(ERROR) << kErrorNotAvailable;
    return RespondNow(Error(kErrorNotAvailable));
  }
  lacros_service->GetRemote<crosapi::mojom::Prefs>()->GetPref(
      crosapi::mojom::PrefPath::kSharedStorage,
      base::BindOnce(&SharedStoragePrivateGetFunction::OnGet, this));
  return RespondLater();
}

void SharedStoragePrivateGetFunction::OnGet(absl::optional<base::Value> items) {
  if (!items) {
    LOG(ERROR) << kErrorFetching;
    return Respond(Error(kErrorFetching));
  }
  Respond(WithArguments(std::move(*items)));
}

SharedStoragePrivateSetFunction::SharedStoragePrivateSetFunction() = default;
SharedStoragePrivateSetFunction::~SharedStoragePrivateSetFunction() = default;

ExtensionFunction::ResponseAction SharedStoragePrivateSetFunction::Run() {
  absl::optional<shared_api::Set::Params> params =
      shared_api::Set::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  auto* lacros_service = chromeos::LacrosService::Get();
  if (!lacros_service ||
      !lacros_service->IsAvailable<crosapi::mojom::Prefs>()) {
    LOG(ERROR) << kErrorNotAvailable;
    return RespondNow(Error(kErrorNotAvailable));
  }
  lacros_service->GetRemote<crosapi::mojom::Prefs>()->GetPref(
      crosapi::mojom::PrefPath::kSharedStorage,
      base::BindOnce(&SharedStoragePrivateSetFunction::OnGet, this,
                     std::move(params->items.additional_properties)));
  return RespondLater();
}

void SharedStoragePrivateSetFunction::OnGet(base::Value::Dict to_add,
                                            absl::optional<base::Value> items) {
  if (!items) {
    LOG(ERROR) << kErrorFetching;
    return Respond(Error(kErrorFetching));
  }
  items->GetDict().Merge(std::move(to_add));
  auto* lacros_service = chromeos::LacrosService::Get();
  if (!lacros_service ||
      !lacros_service->IsAvailable<crosapi::mojom::Prefs>()) {
    LOG(ERROR) << kErrorNotAvailable;
    return Respond(Error(kErrorNotAvailable));
  }
  lacros_service->GetRemote<crosapi::mojom::Prefs>()->SetPref(
      crosapi::mojom::PrefPath::kSharedStorage, std::move(*items),
      base::BindOnce(&SharedStoragePrivateSetFunction::OnSet, this));
}

void SharedStoragePrivateSetFunction::OnSet() {
  Respond(NoArguments());
}

SharedStoragePrivateRemoveFunction::SharedStoragePrivateRemoveFunction() =
    default;
SharedStoragePrivateRemoveFunction::~SharedStoragePrivateRemoveFunction() =
    default;

ExtensionFunction::ResponseAction SharedStoragePrivateRemoveFunction::Run() {
  absl::optional<shared_api::Remove::Params> params =
      shared_api::Remove::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  auto* lacros_service = chromeos::LacrosService::Get();
  if (!lacros_service ||
      !lacros_service->IsAvailable<crosapi::mojom::Prefs>()) {
    LOG(ERROR) << kErrorNotAvailable;
    return RespondNow(Error(kErrorNotAvailable));
  }
  lacros_service->GetRemote<crosapi::mojom::Prefs>()->GetPref(
      crosapi::mojom::PrefPath::kSharedStorage,
      base::BindOnce(&SharedStoragePrivateRemoveFunction::OnGet, this,
                     std::move(params->keys)));
  return RespondLater();
}

void SharedStoragePrivateRemoveFunction::OnGet(
    std::vector<std::string> keys,
    absl::optional<base::Value> items) {
  if (!items || !items->is_dict()) {
    LOG(ERROR) << kErrorFetching;
    return Respond(Error(kErrorFetching));
  }
  for (const auto& key : keys) {
    items->GetDict().Remove(key);
  }
  auto* lacros_service = chromeos::LacrosService::Get();
  if (!lacros_service ||
      !lacros_service->IsAvailable<crosapi::mojom::Prefs>()) {
    LOG(ERROR) << kErrorNotAvailable;
    return Respond(Error(kErrorNotAvailable));
  }
  lacros_service->GetRemote<crosapi::mojom::Prefs>()->SetPref(
      crosapi::mojom::PrefPath::kSharedStorage, std::move(*items),
      base::BindOnce(&SharedStoragePrivateRemoveFunction::OnSet, this));
}

void SharedStoragePrivateRemoveFunction::OnSet() {
  Respond(NoArguments());
}

}  // namespace extensions

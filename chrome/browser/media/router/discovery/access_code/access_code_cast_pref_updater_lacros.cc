// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/discovery/access_code/access_code_cast_pref_updater_lacros.h"

#include "base/json/values_util.h"
#include "chrome/browser/media/router/discovery/access_code/access_code_media_sink_util.h"
#include "chromeos/crosapi/mojom/prefs.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#include "services/preferences/public/cpp/dictionary_value_update.h"

namespace media_router {
namespace {

chromeos::LacrosService* GetLacrosService() {
  auto* lacros_service = chromeos::LacrosService::Get();
  if (!lacros_service ||
      !lacros_service->IsAvailable<crosapi::mojom::Prefs>()) {
    LOG(WARNING) << "crosapi: Prefs API not available";
    return nullptr;
  }
  return lacros_service;
}

void GetPref(crosapi::mojom::PrefPath path,
             base::OnceCallback<void(absl::optional<base::Value>)>
                 on_get_pref_callback) {
  auto* lacros_service = GetLacrosService();
  if (!lacros_service) {
    std::move(on_get_pref_callback).Run(absl::nullopt);
    return;
  }
  lacros_service->GetRemote<crosapi::mojom::Prefs>()->GetPref(
      path, std::move(on_get_pref_callback));
}

void SetPref(crosapi::mojom::PrefPath path,
             base::Value::Dict value,
             base::OnceClosure on_set_pref_callback) {
  auto* lacros_service = GetLacrosService();
  if (!lacros_service) {
    std::move(on_set_pref_callback).Run();
    return;
  }
  lacros_service->GetRemote<crosapi::mojom::Prefs>()->SetPref(
      path, base::Value(std::move(value)), std::move(on_set_pref_callback));
}

void AddToPref(crosapi::mojom::PrefPath path,
               const std::string& key,
               base::Value value,
               base::OnceClosure on_sink_added_callback) {
  GetPref(path,
          base::BindOnce(
              [](crosapi::mojom::PrefPath path, const std::string& key,
                 base::Value value, base::OnceClosure on_sink_added_callback,
                 absl::optional<base::Value> pref_value) {
                auto dict = pref_value.has_value() && pref_value->is_dict()
                                ? std::move(*pref_value).TakeDict()
                                : base::Value::Dict();
                auto* ptr = dict.FindDict(key);
                if (ptr && *ptr == value) {
                  std::move(on_sink_added_callback).Run();
                  return;
                }

                dict.Set(key, std::move(value));
                SetPref(path, std::move(dict),
                        std::move(on_sink_added_callback));
              },
              path, key, std::move(value), std::move(on_sink_added_callback)));
}

void RemoveFromPref(crosapi::mojom::PrefPath path,
                    const std::string& key,
                    base::OnceClosure on_sink_removed_callback) {
  GetPref(path, base::BindOnce(
                    [](crosapi::mojom::PrefPath path, const std::string& key,
                       base::OnceClosure on_sink_removed_callback,
                       absl::optional<base::Value> pref_value) {
                      auto dict =
                          pref_value.has_value() && pref_value->is_dict()
                              ? std::move(*pref_value).TakeDict()
                              : base::Value::Dict();
                      if (!dict.contains(key)) {
                        std::move(on_sink_removed_callback).Run();
                        return;
                      }

                      dict.Remove(key);
                      SetPref(path, std::move(dict),
                              std::move(on_sink_removed_callback));
                    },
                    path, key, std::move(on_sink_removed_callback)));
}

}  // namespace

AccessCodeCastPrefUpdaterLacros::AccessCodeCastPrefUpdaterLacros() = default;
AccessCodeCastPrefUpdaterLacros::~AccessCodeCastPrefUpdaterLacros() = default;

// static
void AccessCodeCastPrefUpdaterLacros::IsAccessCodeCastDevicePrefAvailable(
    base::OnceCallback<void(bool)> availability_callback) {
  GetPref(crosapi::mojom::PrefPath::kAccessCodeCastDevices,
          base::BindOnce(
              [](base::OnceCallback<void(bool)> availability_callback,
                 absl::optional<base::Value> pref_value) {
                std::move(availability_callback).Run(pref_value.has_value());
              },
              std::move(availability_callback)));
}

void AccessCodeCastPrefUpdaterLacros::UpdateDevicesDict(
    const MediaSinkInternal& sink,
    base::OnceClosure on_updated_callback) {
  GetPref(crosapi::mojom::PrefPath::kAccessCodeCastDevices,
          base::BindOnce(
              [](const MediaSinkInternal& sink,
                 base::OnceClosure on_updated_callback,
                 absl::optional<base::Value> pref_value) {
                auto dict = pref_value.has_value() && pref_value->is_dict()
                                ? std::move(*pref_value).TakeDict()
                                : base::Value::Dict();
                for (auto existing_sink_id : GetMatchingIPEndPoints(
                         dict, sink.cast_data().ip_endpoint)) {
                  dict.Remove(existing_sink_id);
                }
                dict.Set(sink.id(), CreateValueDictFromMediaSinkInternal(sink));
                SetPref(crosapi::mojom::PrefPath::kAccessCodeCastDevices,
                        std::move(dict), std::move(on_updated_callback));
              },
              sink, std::move(on_updated_callback)));
}

// This stored preference looks like:
//   "prefs::kAccessCodeCastDeviceAdditionTime": {
//     A string-flavored base::value representing the int64_t number of
//     microseconds since the Windows epoch, using base::TimeToValue().
//     "<sink_id_1>": "1237234734723747234",
//     "<sink_id_2>": "12372347312312347234",
//   }
void AccessCodeCastPrefUpdaterLacros::UpdateDeviceAddedTimeDict(
    const MediaSink::Id sink_id,
    base::OnceClosure on_updated_callback) {
  AddToPref(crosapi::mojom::PrefPath::kAccessCodeCastDeviceAdditionTime,
            sink_id, base::TimeToValue(base::Time::Now()),
            std::move(on_updated_callback));
}

void AccessCodeCastPrefUpdaterLacros::GetDevicesDict(
    base::OnceCallback<void(base::Value::Dict)> get_devices_callback) {
  GetPref(crosapi::mojom::PrefPath::kAccessCodeCastDevices,
          base::BindOnce(
              &AccessCodeCastPrefUpdaterLacros::PrefServiceCallbackAdapter,
              weak_ptr_factory_.GetWeakPtr(), std::move(get_devices_callback)));
}

void AccessCodeCastPrefUpdaterLacros::GetDeviceAddedTimeDict(
    base::OnceCallback<void(base::Value::Dict)>
        get_device_added_time_callback) {
  GetPref(crosapi::mojom::PrefPath::kAccessCodeCastDeviceAdditionTime,
          base::BindOnce(
              &AccessCodeCastPrefUpdaterLacros::PrefServiceCallbackAdapter,
              weak_ptr_factory_.GetWeakPtr(),
              std::move(get_device_added_time_callback)));
}

void AccessCodeCastPrefUpdaterLacros::RemoveSinkIdFromDevicesDict(
    const MediaSink::Id sink_id,
    base::OnceClosure on_sink_removed_callback) {
  RemoveFromPref(crosapi::mojom::PrefPath::kAccessCodeCastDevices, sink_id,
                 std::move(on_sink_removed_callback));
}

void AccessCodeCastPrefUpdaterLacros::RemoveSinkIdFromDeviceAddedTimeDict(
    const MediaSink::Id sink_id,
    base::OnceClosure on_sink_removed_callback) {
  RemoveFromPref(crosapi::mojom::PrefPath::kAccessCodeCastDeviceAdditionTime,
                 sink_id, std::move(on_sink_removed_callback));
}

void AccessCodeCastPrefUpdaterLacros::ClearDevicesDict(
    base::OnceClosure on_cleared_callback) {
  SetPref(crosapi::mojom::PrefPath::kAccessCodeCastDevices, base::Value::Dict(),
          std::move(on_cleared_callback));
}

void AccessCodeCastPrefUpdaterLacros::ClearDeviceAddedTimeDict(
    base::OnceClosure on_cleared_callback) {
  SetPref(crosapi::mojom::PrefPath::kAccessCodeCastDeviceAdditionTime,
          base::Value::Dict(), std::move(on_cleared_callback));
}

void AccessCodeCastPrefUpdaterLacros::UpdateDevicesDictForTest(
    const MediaSinkInternal& sink) {
  AddToPref(crosapi::mojom::PrefPath::kAccessCodeCastDevices, sink.id(),
            base::Value(CreateValueDictFromMediaSinkInternal(sink)),
            base::DoNothing());
}

void AccessCodeCastPrefUpdaterLacros::PrefServiceCallbackAdapter(
    base::OnceCallback<void(base::Value::Dict)> on_get_dict_callback,
    absl::optional<base::Value> pref_value) {
  std::move(on_get_dict_callback)
      .Run(pref_value.has_value() && pref_value.value().is_dict()
               ? std::move(pref_value.value()).TakeDict()
               : base::Value::Dict());
}

}  // namespace media_router

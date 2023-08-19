// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_ACCESS_CODE_ACCESS_CODE_CAST_PREF_UPDATER_LACROS_H_
#define CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_ACCESS_CODE_ACCESS_CODE_CAST_PREF_UPDATER_LACROS_H_

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/media/router/discovery/access_code/access_code_cast_pref_updater.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace media_router {
// Pref updater for AccessCodeCasting for Lacros. It uses the Prefs
// Crosapi to modify and query prefs stored in Ash.
// TODO(b/285156130): `UpdateDevice*Dict()` and `RemoveSinkIdFromDevice*Dict()`
// can potentially lead to a race condition. Implement
// AccessCodeCastPrefUpdaterLacros with a custom crosapi.
class AccessCodeCastPrefUpdaterLacros : public AccessCodeCastPrefUpdater {
 public:
  AccessCodeCastPrefUpdaterLacros();
  AccessCodeCastPrefUpdaterLacros(const AccessCodeCastPrefUpdaterLacros&) =
      delete;
  AccessCodeCastPrefUpdaterLacros& operator=(
      const AccessCodeCastPrefUpdaterLacros&) = delete;
  ~AccessCodeCastPrefUpdaterLacros() override;

  static void IsAccessCodeCastDevicePrefAvailable(
      base::OnceCallback<void(bool)> availability_callback);

  // AccessCodeCastPrefUpdater implementation.
  void UpdateDevicesDict(const MediaSinkInternal& sink,
                         base::OnceClosure on_updated_callback) override;
  void UpdateDeviceAddedTimeDict(
      const MediaSink::Id sink_id,
      base::OnceClosure on_updated_callback) override;
  void GetDevicesDict(base::OnceCallback<void(base::Value::Dict)>
                          get_devices_callback) override;
  void GetDeviceAddedTimeDict(base::OnceCallback<void(base::Value::Dict)>
                                  get_device_added_time_callback) override;
  void RemoveSinkIdFromDevicesDict(
      const MediaSink::Id sink_id,
      base::OnceClosure on_sink_removed_callback) override;
  void RemoveSinkIdFromDeviceAddedTimeDict(
      const MediaSink::Id sink_id,
      base::OnceClosure on_sink_removed_callback) override;
  void ClearDevicesDict(base::OnceClosure on_cleared_callback) override;
  void ClearDeviceAddedTimeDict(base::OnceClosure on_cleared_callback) override;
  void UpdateDevicesDictForTest(const MediaSinkInternal& sink) override;

 private:
  // Prefs API returns a absl::optional<base::Value> but
  // AccessCodeCastPrefUpdater needs a base::Value::Dict.
  void PrefServiceCallbackAdapter(
      base::OnceCallback<void(base::Value::Dict)> on_get_dict_callback,
      absl::optional<base::Value> pref_value);

  base::WeakPtrFactory<AccessCodeCastPrefUpdaterLacros> weak_ptr_factory_{this};
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_ACCESS_CODE_ACCESS_CODE_CAST_PREF_UPDATER_LACROS_H_

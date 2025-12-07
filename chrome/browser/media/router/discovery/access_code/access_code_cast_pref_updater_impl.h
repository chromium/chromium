// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_ACCESS_CODE_ACCESS_CODE_CAST_PREF_UPDATER_IMPL_H_
#define CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_ACCESS_CODE_ACCESS_CODE_CAST_PREF_UPDATER_IMPL_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/media/router/discovery/access_code/access_code_cast_pref_updater.h"

class PrefService;

namespace media_router {

// Pref updater for AccessCodeCasting for Chrome desktop.
class AccessCodeCastPrefUpdaterImpl : public AccessCodeCastPrefUpdater {
 public:
  explicit AccessCodeCastPrefUpdaterImpl(PrefService* service);
  AccessCodeCastPrefUpdaterImpl(const AccessCodeCastPrefUpdaterImpl&) = delete;
  AccessCodeCastPrefUpdaterImpl& operator=(
      const AccessCodeCastPrefUpdaterImpl&) = delete;

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

  void UpdateDevicesDictForTesting(const MediaSinkInternal& sink) override;

 private:
  raw_ptr<PrefService> pref_service_;
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_ACCESS_CODE_ACCESS_CODE_CAST_PREF_UPDATER_IMPL_H_

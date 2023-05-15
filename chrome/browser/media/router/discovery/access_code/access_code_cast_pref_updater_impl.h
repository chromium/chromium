// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_ACCESS_CODE_ACCESS_CODE_CAST_PREF_UPDATER_IMPL_H_
#define CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_ACCESS_CODE_ACCESS_CODE_CAST_PREF_UPDATER_IMPL_H_

#include "chrome/browser/media/router/discovery/access_code/access_code_cast_pref_updater.h"

#include "base/memory/raw_ptr.h"

class PrefService;

namespace media_router {

// Pref updater for AccessCodeCasting for Win, Mac, Linux and ChromeOS Ash.
class AccessCodeCastPrefUpdaterImpl : public AccessCodeCastPrefUpdater {
 public:
  explicit AccessCodeCastPrefUpdaterImpl(PrefService* service);
  AccessCodeCastPrefUpdaterImpl(const AccessCodeCastPrefUpdaterImpl&) = delete;
  AccessCodeCastPrefUpdaterImpl& operator=(
      const AccessCodeCastPrefUpdaterImpl&) = delete;

  void UpdateDevicesDict(const MediaSinkInternal& sink) override;
  void UpdateDeviceAddedTimeDict(const MediaSink::Id sink_id) override;
  const base::Value::Dict& GetDevicesDict() override;
  const base::Value::Dict& GetDeviceAddedTimeDict() override;
  void RemoveSinkIdFromDevicesDict(const MediaSink::Id sink_id) override;
  void RemoveSinkIdFromDeviceAddedTimeDict(
      const MediaSink::Id sink_id) override;
  void ClearDevicesDict() override;
  void ClearDeviceAddedTimeDict() override;

  void UpdateDevicesDictForTest(const MediaSinkInternal& sink) override;

 private:
  raw_ptr<PrefService> pref_service_;
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_ACCESS_CODE_ACCESS_CODE_CAST_PREF_UPDATER_IMPL_H_

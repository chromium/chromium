// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/discovery/access_code/access_code_cast_feature.h"
#include "components/media_router/common/discovery/media_sink_internal.h"
#include "components/media_router/common/media_sink.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"

class PrefService;

#ifndef CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_ACCESS_CODE_ACCESS_CODE_CAST_PREF_UPDATER_H_
#define CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_ACCESS_CODE_ACCESS_CODE_CAST_PREF_UPDATER_H_

namespace media_router {

// Pref updater for AccessCodeCasting.
class AccessCodeCastPrefUpdater {
 public:
  explicit AccessCodeCastPrefUpdater(PrefService* service);

  AccessCodeCastPrefUpdater(const AccessCodeCastPrefUpdater&) = delete;
  AccessCodeCastPrefUpdater& operator=(const AccessCodeCastPrefUpdater&) =
      delete;

  virtual ~AccessCodeCastPrefUpdater();

  // Sets the key for the given |sink| id with the actual |sink| itself. This
  // function will overwrite a sink id if it already exists.
  void UpdateDevicesDict(const MediaSinkInternal& sink);

  // Tries to find an existing |network_id| within the dict pref. If a list is
  // found then append the |sink_id| to that list, otherwise create new list
  // with the |sink_id| as the sole value.
  void UpdateDiscoveredNetworksDict(const MediaSink::Id sink_id,
                                    const std::string& network_id);

  // Sets the key for the |sink_id| with the time it is added. This is
  // calculated at the time of the functions calling. If the |sink_id| already
  // exist, then update the value of that |sink_id| with a new time.
  void UpdateDeviceAdditionTimeDict(const MediaSink::Id sink_id);

  base::WeakPtr<AccessCodeCastPrefUpdater> GetWeakPtr();

 private:
  PrefService* pref_service_;

  base::WeakPtrFactory<AccessCodeCastPrefUpdater> weak_ptr_factory_{this};
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_ACCESS_CODE_ACCESS_CODE_CAST_PREF_UPDATER_H_

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_CAST_MOCK_MIRRORING_ACTIVITY_H_
#define CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_CAST_MOCK_MIRRORING_ACTIVITY_H_

#include "chrome/browser/media/router/providers/cast/mirroring_activity.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace media_router {

class MockMirroringActivity : public MirroringActivity {
 public:
  MockMirroringActivity(const MediaRoute& route,
                        const std::string& app_id,
                        OnStopCallback on_stop,
                        OnSourceChangedCallback on_source_changed);
  ~MockMirroringActivity() override;

  MOCK_METHOD(void, CreateMojoBindings, (mojom::MediaRouter * media_router));
  MOCK_METHOD(void, OnSessionSet, (const CastSession& session));
  MOCK_METHOD(void,
              SendStopSessionMessageToClients,
              (const std::string& hash_token));

  MOCK_METHOD(void, Play, ());
  MOCK_METHOD(void, Pause, ());
  MOCK_METHOD(void, SetMute, (bool mute));
  MOCK_METHOD(void, SetVolume, (float volume));
  MOCK_METHOD(void, Seek, (base::TimeDelta time));
  MOCK_METHOD(void, NextTrack, ());
  MOCK_METHOD(void, PreviousTrack, ());
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_CAST_MOCK_MIRRORING_ACTIVITY_H_

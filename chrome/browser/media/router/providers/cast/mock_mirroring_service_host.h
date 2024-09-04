// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_CAST_MOCK_MIRRORING_SERVICE_HOST_H_
#define CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_CAST_MOCK_MIRRORING_SERVICE_HOST_H_

#include "chrome/browser/media/mirroring_service_host.h"
#include "components/mirroring/mojom/session_parameters.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace media_router {

class MockMirroringServiceHost : public mirroring::MirroringServiceHost {
 public:
  MockMirroringServiceHost();
  ~MockMirroringServiceHost() override;

  MOCK_METHOD(void,
              Start,
              (mirroring::mojom::SessionParametersPtr params,
               mojo::PendingRemote<mirroring::mojom::SessionObserver> observer,
               mojo::PendingRemote<mirroring::mojom::CastMessageChannel>
                   outbound_channel,
               mojo::PendingReceiver<mirroring::mojom::CastMessageChannel>
                   inbound_channel,
               const std::string& sink_name));
  MOCK_METHOD(std::optional<content::FrameTreeNodeId>,
              GetTabSourceId,
              (),
              (const));
  MOCK_METHOD(void, Pause, (base::OnceClosure on_paused_callback));
  MOCK_METHOD(void, Resume, (base::OnceClosure on_resumed_callback));
  MOCK_METHOD(void,
              GetMirroringStats,
              (base::OnceCallback<void(const base::Value)>));
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_CAST_MOCK_MIRRORING_SERVICE_HOST_H_

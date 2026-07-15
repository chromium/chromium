// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/providers/redirection/redirection_service_host.h"

#include <utility>

#include "base/functional/bind.h"
#include "content/public/browser/service_process_host.h"

namespace media_router {

RedirectionServiceHost::RedirectionServiceHost() = default;

RedirectionServiceHost::~RedirectionServiceHost() = default;

void RedirectionServiceHost::Start() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  redirection_service_.reset();
  content::ServiceProcessHost::Launch(
      redirection_service_.BindNewPipeAndPassReceiver(),
      content::ServiceProcessHost::Options()
          .WithDisplayName("Redirection Service")
          .Pass());
  redirection_service_.set_disconnect_handler(base::BindOnce(
      &RedirectionServiceHost::OnDisconnected, weak_factory_.GetWeakPtr()));
  redirection_service_->Start(base::BindOnce(&RedirectionServiceHost::OnStarted,
                                             weak_factory_.GetWeakPtr()));
}

void RedirectionServiceHost::OnStarted(
    media::mojom::RemotingSinkMetadataPtr sink_metadata) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(crbug.com/525729343) Add the sink metadata to the
  // RedirectionConnector to support remoting video through redirection service.
}

void RedirectionServiceHost::OnDisconnected() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  redirection_service_.reset();
}

}  // namespace media_router

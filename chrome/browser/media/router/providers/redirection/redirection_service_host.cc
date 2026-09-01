// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/providers/redirection/redirection_service_host.h"

#include <utility>

#include "base/functional/bind.h"
#include "chrome/browser/media/redirection_connector.h"
#include "content/public/browser/service_process_host.h"

namespace media_router {

RedirectionServiceHost::RedirectionServiceHost() = default;

RedirectionServiceHost::~RedirectionServiceHost() {
  RedirectionConnector::Get()->StoppingRedirection();
}

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

void RedirectionServiceHost::CreateRedirectionSession(
    mojo::PendingReceiver<redirection::mojom::RedirectionSessionHost>
        session_host,
    mojo::PendingReceiver<media::mojom::Remoter> remoter,
    mojo::PendingRemote<media::mojom::RemotingSource> source) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(crbug.com/525333080): Implement call to redirection service once the
  // service is implemented.
}

void RedirectionServiceHost::OnStarted(
    media::mojom::RemotingSinkMetadataPtr sink_metadata) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  RedirectionConnector::Get()->StartingRedirection(
      base::BindRepeating(&RedirectionServiceHost::CreateRedirectionSession,
                          weak_factory_.GetWeakPtr()));
}

void RedirectionServiceHost::OnDisconnected() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  redirection_service_.reset();
  RedirectionConnector::Get()->StoppingRedirection();
}

}  // namespace media_router

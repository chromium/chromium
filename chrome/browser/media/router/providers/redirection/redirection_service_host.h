// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_REDIRECTION_REDIRECTION_SERVICE_HOST_H_
#define CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_REDIRECTION_REDIRECTION_SERVICE_HOST_H_

#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "chrome/services/redirection/public/mojom/redirection_service.mojom.h"
#include "media/mojo/mojom/remoting_common.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace media_router {

// RedirectionServiceHost start/stops a MMR redirection session through
// Redirection Service.
class RedirectionServiceHost {
 public:
  RedirectionServiceHost();
  RedirectionServiceHost(const RedirectionServiceHost&) = delete;
  RedirectionServiceHost& operator=(const RedirectionServiceHost&) = delete;
  virtual ~RedirectionServiceHost();

  // Launches the utility-process service. Virtual so tests can substitute a
  // fake host that does not launch a utility process.
  virtual void Start();

 private:
  void OnStarted(media::mojom::RemotingSinkMetadataPtr sink_metadata);
  void OnDisconnected();

  mojo::Remote<redirection::mojom::RedirectionService> redirection_service_;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<RedirectionServiceHost> weak_factory_{this};
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_REDIRECTION_REDIRECTION_SERVICE_HOST_H_

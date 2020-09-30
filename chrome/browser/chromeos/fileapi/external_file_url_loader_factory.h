// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_FILEAPI_EXTERNAL_FILE_URL_LOADER_FACTORY_H_
#define CHROME_BROWSER_CHROMEOS_FILEAPI_EXTERNAL_FILE_URL_LOADER_FACTORY_H_

#include "base/macros.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"

namespace chromeos {

// URLLoaderFactory that creates URLLoader instances for URLs with the
// externalfile scheme.
class ExternalFileURLLoaderFactory : public network::mojom::URLLoaderFactory {
 public:
  // |render_process_host_id| is used to verify that the child process has
  // permission to request the URL. It should be
  // ChildProcessHost::kInvalidUniqueID for navigations.
  ExternalFileURLLoaderFactory(void* profile_id, int render_process_host_id);
  ~ExternalFileURLLoaderFactory() override;

 private:
  friend class ExternalFileURLLoaderFactoryTest;

  // network::mojom::URLLoaderFactory:
  void CreateLoaderAndStart(
      mojo::PendingReceiver<network::mojom::URLLoader> loader,
      int32_t routing_id,
      int32_t request_id,
      uint32_t options,
      const network::ResourceRequest& request,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation)
      override;
  void Clone(
      mojo::PendingReceiver<network::mojom::URLLoaderFactory> loader) override;

  mojo::ReceiverSet<network::mojom::URLLoaderFactory> receivers_;
  void* profile_id_;
  const int render_process_host_id_;

  DISALLOW_COPY_AND_ASSIGN(ExternalFileURLLoaderFactory);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_FILEAPI_EXTERNAL_FILE_URL_LOADER_FACTORY_H_

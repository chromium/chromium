// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_DRIVEFS_DRIVEFS_HTTP_CLIENT_H_
#define ASH_COMPONENTS_DRIVEFS_DRIVEFS_HTTP_CLIENT_H_

#include "ash/components/drivefs/mojom/drivefs.mojom.h"
#include "base/component_export.h"
#include "mojo/public/cpp/bindings/unique_receiver_set.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace drivefs {

// Handles HTTP requests for DriveFS by translating them to a URLLoader
// request and passing the responses back to DriveFS.
class COMPONENT_EXPORT(DRIVEFS) DriveFsHttpClient {
 public:
  explicit DriveFsHttpClient(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

  DriveFsHttpClient(const DriveFsHttpClient&) = delete;
  DriveFsHttpClient& operator=(const DriveFsHttpClient&) = delete;

  ~DriveFsHttpClient();

  void ExecuteHttpRequest(mojom::HttpRequestPtr request,
                          mojo::PendingRemote<mojom::HttpDelegate> delegate);

 private:
  mojo::UniqueReceiverSet<network::mojom::URLLoaderClient> clients_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
};

}  // namespace drivefs

#endif  // ASH_COMPONENTS_DRIVEFS_DRIVEFS_HTTP_CLIENT_H_

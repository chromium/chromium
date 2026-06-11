// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_INDIGO_API_CLIENT_H_
#define CHROME_BROWSER_INDIGO_API_CLIENT_H_

#include <memory>
#include <string>

#include "base/containers/span.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/types/expected.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "url/gurl.h"

namespace google_apis {
class RequestSender;
}

namespace network {
class SharedURLLoaderFactory;
}

namespace signin {
class IdentityManager;
}

namespace indigo {

struct GeneratedImage {
  GURL image_url;
};

struct GenerateImageError {
  std::string message;
};

struct StatusResult {
  bool has_user_image = false;
  bool is_service_supported_for_account = false;
};

struct StatusError {
  std::string message;
};

struct DeleteError {
  std::string message;
};

class ApiClient : public signin::IdentityManager::Observer {
 public:
  ApiClient(signin::IdentityManager* identity_manager,
            scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);
  ApiClient(const ApiClient&) = delete;
  ApiClient& operator=(const ApiClient&) = delete;
  ~ApiClient() override;

  // Sends a request to the generate endpoint. Returns a callback that will
  // cancel the request when run.
  using GenerateCallback = base::OnceCallback<void(
      base::expected<GeneratedImage, GenerateImageError>)>;
  base::OnceClosure Generate(base::span<const uint8_t> product_image_bytes,
                             GenerateCallback callback);

  // Sends a request to the status endpoint.
  using StatusCallback =
      base::OnceCallback<void(base::expected<StatusResult, StatusError>)>;
  void GetStatus(StatusCallback callback);

  // Sends a request to the delete endpoint.
  using DeleteCallback =
      base::OnceCallback<void(base::expected<void, DeleteError>)>;
  void Delete(DeleteCallback callback);

 private:
  // signin::IdentityManager::Observer:
  void OnPrimaryAccountChanged(
      const signin::PrimaryAccountChangeEvent& event_details) override;

  void ReconstructRequestSender();

  const raw_ptr<signin::IdentityManager> identity_manager_;
  const scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  const GURL generate_url_;
  const GURL status_url_;
  const GURL delete_url_;

  // Null when the profile has no primary account.
  std::unique_ptr<google_apis::RequestSender> request_sender_;

  base::ScopedObservation<signin::IdentityManager,
                          signin::IdentityManager::Observer>
      identity_manager_observation_{this};
};

}  // namespace indigo

#endif  // CHROME_BROWSER_INDIGO_API_CLIENT_H_

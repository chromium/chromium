// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MANTA_ORCA_PROVIDER_H_
#define CHROME_BROWSER_MANTA_ORCA_PROVIDER_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/manta/manta_service_callbacks.h"
#include "components/endpoint_fetcher/endpoint_fetcher.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "url/gurl.h"

namespace signin {
class IdentityManager;
}  // namespace signin

namespace manta {

// The Orca provider for the Manta project. Provides a method for clients to
// call the relevant google API, handling OAuth and http fetching.
//
// IMPORTANT: This class depends on `IdentityManager`, a
// `ProfileKeyedServiceFactory`. You should ensure you do not call
// `OrcaProvider::Call` post `IdentityManager`'s destruction.
// There are several ways to ensure this. You can:
// 1. Make the owner of `OrcaProvider` a `ProfileKeyedServiceFactory` that
// `DependsOn` `IdentityManager`. See
// https://www.chromium.org/developers/design-documents/profile-architecture/#dependency-management-overview
// for more information.
// 2. Register an `IdentityManager::Observer` that listens to
// `OnIdentityManagerShutdown`.
// 3. Manually ensure OrcaProvider isn't used past `IdentityManager`'s
// lifetime.
class OrcaProvider {
 public:
  // Returns a `OrcaProvider` instance tied to the profile of the passed
  // arguments.
  OrcaProvider(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      signin::IdentityManager* identity_manager);

  OrcaProvider(const OrcaProvider&) = delete;
  OrcaProvider& operator=(const OrcaProvider&) = delete;

  virtual ~OrcaProvider();

  // Calls the google service endpoint with the http POST request payload
  // populated with the `input` parameters.
  // The fetched response is processed and returned to the caller via an
  // `MantaGenericCallback` callback.
  //
  // NOTE: This methods internally depends on a valid `IdentityManager`.
  void Call(const std::map<std::string, std::string>& input,
            MantaGenericCallback done_callback);

 private:
  friend class FakeOrcaProvider;

  // Creates and returns unique pointer to an `EndpointFetcher` initialized with
  // the provided parameters and defaults relevant to `OrcaProvider`. Virtual
  // to allow overriding in tests.
  virtual std::unique_ptr<EndpointFetcher> CreateEndpointFetcher(
      const GURL& url,
      const std::vector<std::string>& scopes,
      const std::string& post_data);

  const raw_ptr<signin::IdentityManager> identity_manager_;
  const scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  base::WeakPtrFactory<OrcaProvider> weak_ptr_factory_{this};
};

}  // namespace manta

#endif  // CHROME_BROWSER_MANTA_ORCA_PROVIDER_H_

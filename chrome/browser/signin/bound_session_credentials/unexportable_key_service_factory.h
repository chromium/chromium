// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_BOUND_SESSION_CREDENTIALS_UNEXPORTABLE_KEY_SERVICE_FACTORY_H_
#define CHROME_BROWSER_SIGNIN_BOUND_SESSION_CREDENTIALS_UNEXPORTABLE_KEY_SERVICE_FACTORY_H_

#include <memory>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "chrome/browser/signin/bound_session_credentials/unexportable_key_provider_config.h"
#include "components/unexportable_keys/mojom/unexportable_key_service_proxy_impl.h"
#include "crypto/unexportable_key.h"

class Profile;

namespace content {
class BrowserContext;
}

namespace unexportable_keys {
class UnexportableKeyService;
}

class UnexportableKeyServiceFactory : public ProfileKeyedServiceFactory {
 public:
  using ServiceFactory = base::RepeatingCallback<
      std::unique_ptr<unexportable_keys::UnexportableKeyService>(
          crypto::UnexportableKeyProvider::Config)>;

  // Creates a service instance for the given `config` to be used for garbage
  // collection.
  //
  // Returns nullptr if unexportable key provider is not supported by the
  // platform.
  static std::unique_ptr<unexportable_keys::UnexportableKeyService>
  CreateForGarbageCollection(crypto::UnexportableKeyProvider::Config config);

  // Returns a handle to the service instance for the default storage partition
  // of the given `profile` and `purpose`. Returns nullptr if unexportable key
  // provider is not supported by the platform.
  static unexportable_keys::UnexportableKeyService* GetForProfileAndPurpose(
      Profile* profile,
      unexportable_keys::KeyPurpose purpose);

  // Returns a handle to the service instance for the given `profile`,
  // `relative_partition_path` and `purpose`. Returns nullptr if unexportable
  // key provider is not supported by the platform.
  static unexportable_keys::UnexportableKeyService*
  GetForStoragePartitionPathAndPurpose(
      Profile* profile,
      const base::FilePath& relative_partition_path,
      unexportable_keys::KeyPurpose purpose);

  // Returns nullptr if unexportable key provider is not supported by the
  // platform.
  //
  // If called multiple times, it will replace the existing receiver with the
  // new instance passed. This will result in previous connections being
  // dropped.
  static unexportable_keys::UnexportableKeyServiceProxyImpl*
  RecreateMojoProxyForStoragePartitionPathAndPurposeWithReceiver(
      Profile* profile,
      const base::FilePath& relative_partition_path,
      unexportable_keys::KeyPurpose purpose,
      mojo::PendingReceiver<unexportable_keys::mojom::UnexportableKeyService>
          receiver);

  static UnexportableKeyServiceFactory* GetInstance();

  UnexportableKeyServiceFactory(const UnexportableKeyServiceFactory&) = delete;
  UnexportableKeyServiceFactory& operator=(
      const UnexportableKeyServiceFactory&) = delete;

  // Used in tests to override the service creation.
  void SetServiceFactoryForTesting(ServiceFactory factory);

 private:
  friend class base::NoDestructor<UnexportableKeyServiceFactory>;

  UnexportableKeyServiceFactory();
  ~UnexportableKeyServiceFactory() override;

  // ProfileKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;

  ServiceFactory service_factory_for_testing_;
};

#endif  // CHROME_BROWSER_SIGNIN_BOUND_SESSION_CREDENTIALS_UNEXPORTABLE_KEY_SERVICE_FACTORY_H_

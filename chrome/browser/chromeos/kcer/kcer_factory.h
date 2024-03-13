// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_KCER_KCER_FACTORY_H_
#define CHROME_BROWSER_CHROMEOS_KCER_KCER_FACTORY_H_

#include <utility>

#include <secmodt.h>

#include "base/containers/flat_map.h"
#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "chromeos/components/kcer/kcer.h"
#include "chromeos/components/kcer/kcer_token.h"
#include "components/keyed_service/core/keyed_service.h"

class Profile;

namespace net {
class NSSCertDatabase;
}
namespace kcer::internal {
class KcerImpl;
}  // namespace kcer::internal

namespace kcer {

// Feature flag to toggle between Kcer-over-NSS and Kcer-without-NSS. If
// disabled, Kcer will use the implementation that relies on NSS. If enabled,
// Kcer will talk directly with Chaps without using NSS.
BASE_DECLARE_FEATURE(kKcerWithoutNss);

// Factory for Kcer instances. At the moment it supports creating instances both
// for Kcer-over-NSS (implementation of the Kcer interface using NSS) and
// Kcer-without-NSS (implementation of the Kcer interface that directly
// communicates with Chaps). This makes the code harder to follow, but the
// general idea is:
// * In all cases KcerFactory owns the pointer to the factory singleton,
// KcerToken-s and the mapping from low level tokens (either NSS slots or Chaps
// tokens) to KcerToken-s.
// * For Kcer-over-NSS in both Ash and Lacros KcerFactory simply fetches the
// existing NSSCertDatabase and builds a Kcer instance using that.
// * For Kcer-without-NSS in Ash some Ash specific code is extracted into
// KcerFactoryAsh because of the build rules. Some more general code is in
// KcerFactory.
// * For Kcer-without-NSS in Lacros some Ash specific code is extracted into
// KcerFactoryLacros because of the build rules. Some more general code is in
// KcerFactory.
class KcerFactory : public ProfileKeyedServiceFactory {
 public:
  // Returns a Kcer instance for the `profile`. The lifetime of the instance is
  // bound to the `profile`. If the pointer is cached, it needs to be checked
  // before it's used.
  static base::WeakPtr<Kcer> GetKcer(Profile* profile);

  // Public for the implementation of this class.
  using UniqueSlotId = std::pair<SECMODModuleID, CK_SLOT_ID>;
  using KcerTokenMapNss =
      base::flat_map<UniqueSlotId, std::unique_ptr<internal::KcerToken>>;
  using KcerTokenMapWithoutNss =
      base::flat_map<SessionChapsClient::SlotId,
                     std::unique_ptr<internal::KcerToken>>;
  using InitializeOnUIThreadCallback =
      base::OnceCallback<void(base::WeakPtr<internal::KcerToken>,
                              base::WeakPtr<internal::KcerToken>)>;

  // Returns whether HighLevelChapsClient was initialized. The method is mostly
  // needed just for testing.
  static bool IsHighLevelChapsClientInitialized();

  // Creates an entry in user preferences in Ash that a PKCS#12 file was
  // dual-written. This will be used in case a rollback for the related
  // experiment is needed.
  static void RecordPkcs12CertDualWritten();

  // Should be called on shutdown to avoid dangling pointers.
  static void Shutdown();

 protected:
  struct KcerService : public KeyedService {
    explicit KcerService(std::unique_ptr<internal::KcerImpl> kcer_instance);
    ~KcerService() override;

    std::unique_ptr<internal::KcerImpl> kcer;
  };

  KcerFactory();
  ~KcerFactory() override;

  // A getter for the specializations to access the singleton pointer.
  static KcerFactory*& GetGlobalPointer();

  // Returns a functor that will create KcerTokens for each slot from `nss_db`
  // (if necessary) and return weak pointers to them to the provided `callback`.
  // The functor should be run on the IO thread, the callback will be run on the
  // UI thread.
  base::OnceCallback<void(KcerFactory::InitializeOnUIThreadCallback callback,
                          net::NSSCertDatabase* nss_db)>
  GetPrepareTokensForNssOnIOThreadFunctor();

  virtual base::WeakPtr<Kcer> GetKcerImpl(Profile* profile);
  // Returns true if the Kcer instance for the `context` should be saved into
  // kcer::ExtraInstances as "default Kcer".
  virtual bool IsPrimaryContext(content::BrowserContext* context) const = 0;
  // Initiates the initialization of `kcer_service` - the instance of Kcer for
  // the `context`. The implementations should identify correct token ids and
  // then call `InitializeKcerInstanceWithoutNss`.
  virtual void StartInitializingKcerWithoutNss(
      base::WeakPtr<internal::KcerImpl> kcer_service,
      content::BrowserContext* context) = 0;
  // Initializes `high_level_chaps_client_` when necessary. Returns true if
  // `high_level_chaps_client_` is initialized.
  virtual bool EnsureHighLevelChapsClientInitialized() = 0;
  // Implements RecordPkcs12CertDualWritten(). Ash needs to write into the
  // preferences of the active user. Lacros needs to send a notification to Ash.
  virtual void RecordPkcs12CertDualWrittenImpl() = 0;

  // Initializes each token for `kcer_service` with `user_token_id` and
  // `device_token_id` respectively, initializes `kcer_service` with the tokens.
  void InitializeKcerInstanceWithoutNss(
      base::WeakPtr<internal::KcerImpl> kcer_service,
      std::optional<SessionChapsClient::SlotId> user_token_id,
      std::optional<SessionChapsClient::SlotId> device_token_id);

  // Initializes a single token.
  base::WeakPtr<internal::KcerToken> GetTokenWithoutNss(
      std::optional<SessionChapsClient::SlotId> token_id,
      Token token_type);

  // Returns whether the Kcer-without-NSS experiment is enabled.
  bool UseKcerWithoutNss() const;

  // Used by `high_level_chaps_client_` and must outlive it.
  std::unique_ptr<SessionChapsClient> session_chaps_client_;
  // Used by tokens in `chaps_tokens_ui_` (to communicate with Chaps) and must
  // outlive them.
  std::unique_ptr<HighLevelChapsClient> high_level_chaps_client_;

 private:
  friend class base::NoDestructor<KcerFactory>;

  // BrowserContextKeyedServiceFactory:
  // The services need to be created immediately after the browser context
  // because the instance for the main profile will also be exposed as the
  // default one and it won't be able to be created on demand from there.
  bool ServiceIsCreatedWithBrowserContext() const override;
  // Creates a new service, which is not fully initialized yet, but can already
  // be used as if it's initialized. Posts a task to finish the initialization.
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;

  void StartInitializingKcerInstance(
      base::WeakPtr<internal::KcerImpl> kcer_service,
      content::BrowserContext* context);
  void StartInitializingKcerForNss(
      base::WeakPtr<internal::KcerImpl> kcer_service,
      content::BrowserContext* context);
  void InitializeKcerInstanceForNss(
      base::WeakPtr<internal::KcerImpl> kcer_service,
      base::WeakPtr<internal::KcerToken> user_token,
      base::WeakPtr<internal::KcerToken> device_token);

  void StartRetrievingContextInfo(content::BrowserContext* context);
  void OnContextInfoRetrieved(content::BrowserContext* context,
                              bool is_default_context,
                              bool should_have_user_token,
                              bool should_have_device_token);

  // Stores the mapping between NSS slots and KcerToken-s. Each private or
  // system slot is mapped into a KcerToken, so a Kcer instance equivalent to a
  // given NSSCertDatabase can be created. Public slots are ignored because
  // there should be no keys or client certificates there by the time Kcer is
  // launched.
  // The map is created on the UI thread, only used on the IO thread and is
  // never destroyed (as a part of NoDestructor<> factory).
  KcerTokenMapNss nss_tokens_io_;
  // Stores the mapping between Chaps tokens and KcerToken-s. The map is created
  // on the UI thread, only used on the UI thread and is never destroyed (as a
  // part of NoDestructor<> factory).
  // Only one token map is used for each Chromium launch, which is controlled by
  // an experiment.
  KcerTokenMapWithoutNss chaps_tokens_ui_;
};

}  // namespace kcer

#endif  // CHROME_BROWSER_CHROMEOS_KCER_KCER_FACTORY_H_

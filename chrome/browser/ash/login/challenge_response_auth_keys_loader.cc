// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/challenge_response_auth_keys_loader.h"

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/scoped_multi_source_observation.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/values.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/certificate_provider/certificate_provider.h"
#include "chrome/browser/certificate_provider/certificate_provider_service.h"
#include "chrome/browser/certificate_provider/certificate_provider_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "chromeos/ash/components/login/auth/challenge_response/cert_utils.h"
#include "chromeos/ash/components/login/auth/challenge_response/known_user_pref_utils.h"
#include "components/account_id/account_id.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/known_user.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extension_host_observer.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/browser/lazy_context_id.h"
#include "extensions/browser/lazy_context_task_queue.h"
#include "extensions/browser/pref_names.h"
#include "extensions/browser/process_manager.h"
#include "extensions/browser/process_manager_observer.h"
#include "extensions/common/manifest_handlers/background_info.h"

namespace ash {
namespace {

constexpr base::TimeDelta kDefaultMaximumExtensionLoadWaitingTime =
    base::Seconds(5);

base::flat_set<std::string> GetLoginScreenPolicyExtensionIds() {
  DCHECK(BrowserContextHelper::Get()->GetSigninBrowserContext());

  const PrefService* const prefs =
      ProfileHelper::GetSigninProfile()->GetPrefs();
  DCHECK_EQ(prefs->GetAllPrefStoresInitializationStatus(),
            PrefService::INITIALIZATION_STATUS_SUCCESS);

  const PrefService::Preference* const pref =
      prefs->FindPreference(extensions::pref_names::kInstallForceList);
  if (!pref || !pref->IsManaged() ||
      pref->GetType() != base::Value::Type::DICT) {
    return {};
  }

  base::flat_set<std::string> extension_ids;
  for (const auto item : pref->GetValue()->GetDict()) {
    extension_ids.insert(item.first);
  }
  return extension_ids;
}

Profile* GetProfile() {
  return ProfileHelper::GetSigninProfile()->GetOriginalProfile();
}

extensions::ExtensionRegistry* GetExtensionRegistry() {
  return extensions::ExtensionRegistry::Get(GetProfile());
}

extensions::ProcessManager* GetProcessManager() {
  return extensions::ProcessManager::Get(GetProfile());
}

// Loads the persistently stored information about the challenge-response keys
// that can be used for authenticating the user.
void LoadStoredChallengeResponseSpkiKeysForUser(
    const AccountId& account_id,
    std::vector<std::string>* spki_items,
    base::flat_set<std::string>* extension_ids) {
  const base::Value::List known_user_value =
      user_manager::KnownUser(g_browser_process->local_state())
          .GetChallengeResponseKeys(account_id);
  std::vector<DeserializedChallengeResponseKey>
      deserialized_challenge_response_keys;
  DeserializeChallengeResponseKeyFromKnownUser(
      known_user_value, &deserialized_challenge_response_keys);
  for (const DeserializedChallengeResponseKey& challenge_response_key :
       deserialized_challenge_response_keys) {
    if (!challenge_response_key.public_key_spki_der.empty()) {
      spki_items->push_back(challenge_response_key.public_key_spki_der);
    }
    if (!challenge_response_key.extension_id.empty()) {
      extension_ids->insert(challenge_response_key.extension_id);
    }
  }
}

// Returns the certificate provider service that should be used for querying the
// currently available cryptographic keys.
// The sign-in profile is used since it's where the needed extensions are
// installed (e.g., for the smart card based login they are force-installed via
// the DeviceLoginScreenExtensions admin policy).
chromeos::CertificateProviderService* GetCertificateProviderService() {
  return chromeos::CertificateProviderServiceFactory::GetForBrowserContext(
      ProfileHelper::GetSigninProfile());
}

// Maps from the TLS 1.3 SignatureScheme values into the challenge-response key
// algorithm list.
std::vector<ChallengeResponseKey::SignatureAlgorithm> MakeAlgorithmListFromSsl(
    const std::vector<uint16_t>& ssl_algorithms) {
  std::vector<ChallengeResponseKey::SignatureAlgorithm>
      challenge_response_algorithms;
  for (auto ssl_algorithm : ssl_algorithms) {
    std::optional<ChallengeResponseKey::SignatureAlgorithm> algorithm =
        GetChallengeResponseKeyAlgorithmFromSsl(ssl_algorithm);
    if (algorithm)
      challenge_response_algorithms.push_back(*algorithm);
  }
  return challenge_response_algorithms;
}

// Can observe the installation and loading of force-installed extensions.
// Allows to wait until the background pages of a set of extensions are loaded.
//
// When waiting for an extension that is not yet installed, the following chain
// of calls should happen, either directly or trough some observers:
// 1. StartWaiting
// 2. OnExtensionInstalled
// 3. OnBackgroundHostCreated
// 4. OnExtensionBackgroundHostActive
// 5. OnExtensionHostDidStopFirstLoad
// 6. StopWaitingOnExtension
// 7. TriggerExtensionsReadyCallback
// 8. OnStopWaiting
// Depending on the state of installation/loading when observation starts, some
// of these steps can be omitted.
class ExtensionLoadObserver final
    : public extensions::ProcessManagerObserver,
      public extensions::ExtensionHostObserver,
      public extensions::ExtensionRegistryObserver {
 public:
  // Waits until all extensions in `extension_ids` are ready and invokes
  // `callback` afterwards.
  static void WaitUntilExtensionsReady(
      const base::flat_set<std::string>& extension_ids,
      base::TimeDelta maximum_waiting_time,
      base::OnceClosure callback) {
    std::unique_ptr<ExtensionLoadObserver> extension_load_observer =
        std::make_unique<ExtensionLoadObserver>(maximum_waiting_time);
    ExtensionLoadObserver* extension_load_observer_ptr =
        extension_load_observer.get();
    // Starts waiting asynchronously. The object is kept alive since it is bound
    // to the callback.
    extension_load_observer_ptr->StartWaiting(
        extension_ids, base::BindOnce(&ExtensionLoadObserver::OnStopWaiting,
                                      std::move(extension_load_observer),
                                      std::move(callback)));
  }

  explicit ExtensionLoadObserver(base::TimeDelta maximum_waiting_time)
      : maximum_waiting_time_(maximum_waiting_time) {
    process_manager_observation_.Observe(GetProcessManager());
    extension_registry_observation_.Observe(GetExtensionRegistry());
  }
  ExtensionLoadObserver(const ExtensionLoadObserver&) = delete;
  ExtensionLoadObserver& operator=(const ExtensionLoadObserver&) = delete;
  ~ExtensionLoadObserver() override = default;

  // Observers that observe extension installation and loading in order to delay
  // requests until all relevant extensions are ready.

  // extensions::ExtensionRegistryObserver

  void OnExtensionInstalled(content::BrowserContext* browser_context,
                            const extensions::Extension* extension,
                            bool is_update) override {
    if (!extensions_waited_for_.contains(extension->id()))
      return;

    // If the extension does not have a background page, do not wait on it since
    // we do not know how it indicates readiness.
    if (!extensions::BackgroundInfo::HasBackgroundPage(extension)) {
      StopWaitingOnExtension(extension->id());
      return;
    }

    // Ensure that the extension's background host is active.
    const auto context_id =
        extensions::LazyContextId::ForExtension(browser_context, extension);
    extensions::LazyContextTaskQueue* queue = context_id.GetTaskQueue();
    if (!queue->ShouldEnqueueTask(browser_context, extension)) {
      // The background host already exists.
      OnExtensionBackgroundHostActive(extension->id());
    } else {
      // In case the extension got suspended or isn't ready yet, wake it up and
      // wait until it is ready.
      queue->AddPendingTask(
          context_id,
          base::BindOnce(
              &ExtensionLoadObserver::OnExtensionBackgroundHostActive,
              weak_ptr_factory_.GetWeakPtr(), extension->id()));
    }
  }

  void OnShutdown(extensions::ExtensionRegistry* registry) override {
    DCHECK(extension_registry_observation_.IsObservingSource(registry));
    extension_registry_observation_.Reset();
    TriggerExtensionsReadyCallback();
  }

  // extensions::ProcessManagerObserver

  void OnBackgroundHostCreated(
      extensions::ExtensionHost* extension_host) override {
    if (extensions_waited_for_.contains(extension_host->extension_id()))
      OnExtensionBackgroundHostActive(extension_host->extension_id());
  }

  void OnProcessManagerShutdown(extensions::ProcessManager* manager) override {
    DCHECK(process_manager_observation_.IsObservingSource(manager));
    process_manager_observation_.Reset();
    TriggerExtensionsReadyCallback();
  }

  // extensions::ExtensionHostObserver

  void OnExtensionHostDestroyed(extensions::ExtensionHost* host) override {
    DCHECK(extension_host_observations_.IsObservingSource(host));
    extension_host_observations_.RemoveObservation(host);
    StopWaitingOnExtension(host->extension_id());
  }

  void OnExtensionHostDidStopFirstLoad(
      const extensions::ExtensionHost* host) override {
    StopWaitingOnExtension(host->extension_id());
  }

 private:
  static void OnStopWaiting(
      std::unique_ptr<ExtensionLoadObserver> extension_load_observer,
      base::OnceClosure callback) {
    std::move(callback).Run();
  }

  // Waits until all extensions in `extension_ids` are ready and invokes
  // `callback` afterwards.
  void StartWaiting(const base::flat_set<std::string>& extension_ids,
                    base::OnceClosure callback) {
    extensions_ready_callback_ = std::move(callback);

    // Do not wait longer than `maximum_waiting_time_`.
    stop_waiting_timer_.Start(
        FROM_HERE, maximum_waiting_time_, this,
        &ExtensionLoadObserver::TriggerExtensionsReadyCallback);

    base::flat_set<std::string> login_screen_policy_extension_ids =
        GetLoginScreenPolicyExtensionIds();
    // Get all ids that are both in `extension_ids` and
    // `login_screen_policy_extension_ids`.
    std::vector<std::string> extension_ids_to_wait_for;
    for (const std::string& extension_id : extension_ids) {
      if (login_screen_policy_extension_ids.contains(extension_id)) {
        extension_ids_to_wait_for.push_back(extension_id);
      }
    }

    extensions_waited_for_.insert(extension_ids_to_wait_for.begin(),
                                  extension_ids_to_wait_for.end());

    if (extensions_waited_for_.empty()) {
      TriggerExtensionsReadyCallback();
      return;
    }

    for (const std::string& extension_id : extension_ids_to_wait_for) {
      const extensions::Extension* extension =
          GetExtensionRegistry()->GetInstalledExtension(extension_id);
      if (extension) {
        OnExtensionInstalled(GetProfile(), extension,
                             /*is_update=*/false);
      }
    }
    // OnExtensionInstalled() can cause `this` to be destroyed, so it needs to
    // be the last call of the method.
  }

  void OnExtensionBackgroundHostActive(
      const std::string& extension_id,
      std::unique_ptr<extensions::LazyContextTaskQueue::ContextInfo> params =
          nullptr /*ignored*/) {
    extensions::ExtensionHost* extension_host =
        GetProcessManager()->GetBackgroundHostForExtension(extension_id);
    if (!extension_host) {
      // Generally this should not happen, but better safe than sorry.
      NOTREACHED_IN_MIGRATION();
      return;
    }

    if (extension_host->has_loaded_once()) {
      StopWaitingOnExtension(extension_id);
      return;
    }

    // Observe first load of background page.
    if (!extension_host_observations_.IsObservingSource(extension_host)) {
      extension_host_observations_.AddObservation(extension_host);
    }
  }

  void StopWaitingOnExtension(const std::string& extension_id) {
    if (!extensions_waited_for_.contains(extension_id)) {
      NOTREACHED_IN_MIGRATION();
      return;
    }

    extensions_waited_for_.erase(extension_id);
    if (extensions_waited_for_.empty()) {
      TriggerExtensionsReadyCallback();
    }
  }

  // Triggers the callback. `this` may be destroyed after this function.
  void TriggerExtensionsReadyCallback() {
    if (extensions_ready_callback_) {
      std::move(extensions_ready_callback_).Run();
    }
  }

  const base::TimeDelta maximum_waiting_time_;

  base::OnceClosure extensions_ready_callback_;
  base::OneShotTimer stop_waiting_timer_;
  // Ids of all extensions that are necessary but not yet ready.
  base::flat_set<std::string> extensions_waited_for_;

  base::ScopedObservation<extensions::ProcessManager,
                          extensions::ProcessManagerObserver>
      process_manager_observation_{this};
  base::ScopedMultiSourceObservation<extensions::ExtensionHost,
                                     extensions::ExtensionHostObserver>
      extension_host_observations_{this};
  base::ScopedObservation<extensions::ExtensionRegistry,
                          extensions::ExtensionRegistryObserver>
      extension_registry_observation_{this};

  base::WeakPtrFactory<ExtensionLoadObserver> weak_ptr_factory_{this};
};

}  // namespace

// static
bool ChallengeResponseAuthKeysLoader::CanAuthenticateUser(
    const AccountId& account_id) {
  std::vector<std::string> suitable_public_key_spki_items;
  base::flat_set<std::string> extension_ids_ignored;
  LoadStoredChallengeResponseSpkiKeysForUser(
      account_id, &suitable_public_key_spki_items, &extension_ids_ignored);
  return !suitable_public_key_spki_items.empty();
}

ChallengeResponseAuthKeysLoader::ChallengeResponseAuthKeysLoader()
    : maximum_extension_load_waiting_time_(
          kDefaultMaximumExtensionLoadWaitingTime) {
  profile_subscription_.Observe(GetProfile());
}

ChallengeResponseAuthKeysLoader::~ChallengeResponseAuthKeysLoader() = default;

void ChallengeResponseAuthKeysLoader::LoadAvailableKeys(
    const AccountId& account_id,
    LoadAvailableKeysCallback callback) {
  if (profile_is_destroyed_) {
    // Don't proceed during shutdown.
    std::move(callback).Run(/*challenge_response_keys=*/{});
    return;
  }
  // Load the list of public keys of the cryptographic keys that can be used
  // for authenticating the user.
  std::vector<std::string> suitable_public_key_spki_items;
  base::flat_set<std::string> extension_ids;
  LoadStoredChallengeResponseSpkiKeysForUser(
      account_id, &suitable_public_key_spki_items, &extension_ids);
  if (suitable_public_key_spki_items.empty()) {
    // This user's profile doesn't support challenge-response authentication.
    std::move(callback).Run(/*challenge_response_keys=*/{});
    return;
  }

  // Wait until the extensions that are needed to sign the keys are ready.
  ExtensionLoadObserver::WaitUntilExtensionsReady(
      extension_ids, maximum_extension_load_waiting_time_,
      base::BindOnce(&ChallengeResponseAuthKeysLoader::
                         ContinueLoadAvailableKeysExtensionsLoaded,
                     weak_ptr_factory_.GetWeakPtr(), account_id,
                     std::move(suitable_public_key_spki_items),
                     std::move(callback)));
}

void ChallengeResponseAuthKeysLoader::OnProfileWillBeDestroyed(
    Profile* profile) {
  profile_is_destroyed_ = true;
  DCHECK(profile_subscription_.IsObservingSource(profile));
  profile_subscription_.Reset();
}

void ChallengeResponseAuthKeysLoader::ContinueLoadAvailableKeysExtensionsLoaded(
    const AccountId& account_id,
    const std::vector<std::string>& suitable_public_key_spki_items,
    LoadAvailableKeysCallback callback) {
  if (profile_is_destroyed_) {
    // Don't proceed during shutdown.
    std::move(callback).Run(/*challenge_response_keys=*/{});
    return;
  }
  // Asynchronously poll all certificate providers to get the list of
  // currently available cryptographic keys.
  std::unique_ptr<chromeos::CertificateProvider> cert_provider =
      GetCertificateProviderService()->CreateCertificateProvider();
  cert_provider->GetCertificates(base::BindOnce(
      &ChallengeResponseAuthKeysLoader::ContinueLoadAvailableKeysWithCerts,
      weak_ptr_factory_.GetWeakPtr(), account_id,
      std::move(suitable_public_key_spki_items), std::move(callback)));
}

void ChallengeResponseAuthKeysLoader::ContinueLoadAvailableKeysWithCerts(
    const AccountId& account_id,
    const std::vector<std::string>& suitable_public_key_spki_items,
    LoadAvailableKeysCallback callback,
    net::ClientCertIdentityList /* cert_identities */) {
  if (profile_is_destroyed_) {
    // Don't proceed during shutdown.
    std::move(callback).Run(/*challenge_response_keys=*/{});
    return;
  }
  chromeos::CertificateProviderService* const cert_provider_service =
      GetCertificateProviderService();
  std::vector<ChallengeResponseKey> filtered_keys;
  // Filter those of the currently available cryptographic keys that can be used
  // for authenticating the user. Also fill out for the selected keys the
  // currently available cryptographic signature algorithms.
  for (const auto& suitable_spki : suitable_public_key_spki_items) {
    std::vector<uint16_t> supported_ssl_algorithms;
    std::string extension_id;
    cert_provider_service->LookUpSpki(suitable_spki, &supported_ssl_algorithms,
                                      &extension_id);
    if (supported_ssl_algorithms.empty()) {
      // This key is not currently exposed by any certificate provider or,
      // potentially, is exposed but without supporting any signature
      // algorithm.
      continue;
    }
    std::vector<ChallengeResponseKey::SignatureAlgorithm> supported_algorithms =
        MakeAlgorithmListFromSsl(supported_ssl_algorithms);
    if (supported_algorithms.empty()) {
      // This currently available key doesn't support any of the algorithms
      // that are supported by the challenge-response user authentication.
      continue;
    }
    ChallengeResponseKey filtered_key;
    filtered_key.set_public_key_spki_der(suitable_spki);
    filtered_key.set_signature_algorithms(supported_algorithms);
    filtered_key.set_extension_id(extension_id);
    filtered_keys.push_back(filtered_key);
  }
  std::move(callback).Run(std::move(filtered_keys));
}

}  // namespace ash

// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ash/arc/enterprise/cert_store/cert_store_service.h"

#include <algorithm>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "ash/components/arc/arc_browser_context_keyed_service_factory_base.h"
#include "ash/components/arc/arc_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/memory/singleton.h"
#include "base/memory/weak_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/ash/arc/enterprise/cert_store/arc_cert_installer_utils.h"
#include "chrome/browser/ash/arc/keymaster/arc_keymaster_bridge.h"
#include "chrome/browser/ash/arc/keymint/arc_keymint_bridge.h"
#include "chrome/browser/ash/arc/policy/arc_policy_bridge.h"
#include "chrome/browser/ash/platform_keys/key_permissions/key_permissions_service_factory.h"
#include "chrome/browser/ash/platform_keys/key_permissions/key_permissions_service_impl.h"
#include "chrome/browser/ash/platform_keys/platform_keys_service.h"
#include "chrome/browser/ash/platform_keys/platform_keys_service_factory.h"
#include "chrome/browser/certificate_provider/certificate_provider_service_factory.h"
#include "chrome/browser/chromeos/platform_keys/platform_keys.h"
#include "chrome/browser/net/nss_service.h"
#include "chrome/browser/net/nss_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "chrome/common/net/x509_certificate_model_nss.h"
#include "chrome/services/keymaster/public/mojom/cert_store.mojom.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_namespace.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "crypto/rsa_private_key.h"
#include "net/cert/x509_util_nss.h"

// Enable VLOG level 1.
#undef ENABLED_VLOG_LEVEL
#define ENABLED_VLOG_LEVEL 1

namespace arc {

namespace {

// The following series of functions related to ListCerts make use of the
// NSSCertDatabase to fulfill its goal of listing certificates. The cert
// database is accessed through a raw pointer with limited lifetime guarantees
// and is not thread safe. Namely, the cert database is guaranteed valid for the
// single IO thread task where it was received.
//
// Furthermore, creating an NssCertDatabaseGetter requires a BrowserContext,
// which can only be accessed on the UI thread.
//
// ListCerts and related functions are implemented to make sure the above
// requirements are respected. Here's a diagram of the interaction between UI
// and IO threads.
//
//                    UI Thread                     IO Thread
//
//                    ListCerts
//                        |
//       NssService::CreateNSSCertDatabaseGetterForIOThread
//                        |
//                        \----------------------------v
//                                          ListCertsWithDbGetterOnIO
//                                                     |
//                                           database_getter.Run()
//                                                     |
//                                               ListCertsOnIO
//                                                     |
//                                              ListCertsInSlot
//                                                     |
//                                   PostListedCertsBackToOriginalTaskRunner
//                                                     |
//                        v----------------------------/
//  Process certs / Repeat ListCerts for system slot
//
// ARC requires certs from both the 'user' and 'system' chaps slots to be
// processed. Because ListCertsInSlot is asynchronous, it's not possible to
// guarantee that both ListCertsInSlot calls happen in the same task execution,
// so this entire process is performed twice: first for the user slot, then for
// the system slot. The ordering of the calls is not important, other than the
// implementation lists the 'user' slot first, and uses the 'system' slot to
// signal the process is complete.
//
// The current user may not have access to the system slot, but that is only
// discoverable on the IO thread. In that case, the sequence for the system slot
// becomes:
//
//                    UI Thread                     IO Thread
//
//                    ListCerts
//                        |
//       NssService::CreateNSSCertDatabaseGetterForIOThread
//                        |
//                        \----------------------------v
//                                          ListCertsWithDbGetterOnIO
//                                                     |
//                                           database_getter.Run()
//                                                     |
//                                                ListCertsOnIO
//                                                     |
//                                   (Determine system slot isn't allowed)
//                                                     |
//                                   PostListedCertsBackToOriginalTaskRunner
//                                                     |
//                        v----------------------------/
//             Process list of certs...

void PostListedCertsBackToOriginalTaskRunner(
    scoped_refptr<base::TaskRunner> original_task_runner,
    net::NSSCertDatabase::ListCertsCallback callback,
    net::ScopedCERTCertificateList cert_list) {
  original_task_runner->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::move(cert_list)));
}

void ListCertsOnIO(scoped_refptr<base::TaskRunner> original_task_runner,
                   keymanagement::mojom::ChapsSlot slot,
                   net::NSSCertDatabase::ListCertsCallback callback,
                   net::NSSCertDatabase* database) {
  // |database->ListCertsInSlot| must be called from the IO thread.
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  if (slot == keymanagement::mojom::ChapsSlot::kSystem &&
      !database->GetSystemSlot()) {
    // Trying to list system slot when it's not available, return empty list.
    PostListedCertsBackToOriginalTaskRunner(original_task_runner,
                                            std::move(callback),
                                            net::ScopedCERTCertificateList());
    return;
  }

  // List the certs on |database|, which may dispatch to a worker thread to
  // avoid blocking. The actual result needs to make it back to the UI thread,
  // but the callback will be invoked on the IO thread, so the results need
  // to be forwarded onwards.
  database->ListCertsInSlot(
      base::BindOnce(&PostListedCertsBackToOriginalTaskRunner,
                     original_task_runner, std::move(callback)),
      slot == keymanagement::mojom::ChapsSlot::kUser
          ? database->GetPrivateSlot().get()
          : database->GetSystemSlot().get());
}

void ListCertsWithDbGetterOnIO(
    scoped_refptr<base::TaskRunner> original_task_runner,
    keymanagement::mojom::ChapsSlot slot,
    net::NSSCertDatabase::ListCertsCallback callback,
    NssCertDatabaseGetter database_getter) {
  // |database_getter| must be run from the IO thread.
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  // Running |database_getter| may either return a non-null pointer
  // synchronously or invoke the given callback asynchronously with a non-null
  // pointer. |split_callback| is used here to handle both cases.
  auto split_callback = base::SplitOnceCallback(
      base::BindOnce(&ListCertsOnIO, std::move(original_task_runner), slot,
                     std::move(callback)));

  net::NSSCertDatabase* database =
      std::move(database_getter).Run(std::move(split_callback.first));
  if (database)
    std::move(split_callback.second).Run(database);
}

// Returns the list of certificates in |slot|, making sure to fetch the cert
// database and list certs from the IO thread, while posting |callback| with the
// output list to the original caller thread.
void ListCerts(content::BrowserContext* const context,
               keymanagement::mojom::ChapsSlot slot,
               net::NSSCertDatabase::ListCertsCallback callback) {
  // |context| must be accessed on the UI thread.
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // The NssCertDatabaseGetter must be posted to the IO thread immediately.
  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&ListCertsWithDbGetterOnIO,
                     base::SingleThreadTaskRunner::GetCurrentDefault(), slot,
                     std::move(callback),
                     NssServiceFactory::GetForContext(context)
                         ->CreateNSSCertDatabaseGetterForIOThread()));
}

using IsCertificateAllowedCallback = base::OnceCallback<void(bool allowed)>;

void CheckCorporateFlag(
    IsCertificateAllowedCallback callback,
    std::optional<bool> corporate_key,
    chromeos::platform_keys::Status is_corporate_key_status) {
  if (is_corporate_key_status != chromeos::platform_keys::Status::kSuccess) {
    LOG(ERROR) << "Error checking whether key is corporate. Will not install "
                  "key in ARC";
    std::move(callback).Run(/* allowed */ false);
    return;
  }
  DCHECK(corporate_key.has_value());
  std::move(callback).Run(/* allowed */ corporate_key.value());
}

// Returns true if the certificate is allowed to be used by ARC. The certificate
// is allowed to be used by ARC if its key is marked for corporate usage. |cert|
// must be non-null.
void IsCertificateAllowed(IsCertificateAllowedCallback callback,
                          scoped_refptr<net::X509Certificate> cert,
                          content::BrowserContext* const context) {
  // |context| must be accessed on the UI thread.
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(cert);

  // Check if the key is marked for corporate usage.
  ash::platform_keys::KeyPermissionsServiceFactory::GetForBrowserContext(
      context)
      ->IsCorporateKey(
          chromeos::platform_keys::GetSubjectPublicKeyInfoBlob(cert),
          base::BindOnce(&CheckCorporateFlag, std::move(callback)));
}

// Creates a |CertDescription| for the given |nss_cert| known to be stored in
// |slot|. May return |std::nullopt| if some cert metadata can't be found, e.g.
// when the cert private key is deleted while we still keep a valid pointer to
// |nss_cert|.
std::optional<CertDescription> BuildCertDescritionOnWorkerThread(
    net::ScopedCERTCertificate nss_cert,
    keymanagement::mojom::ChapsSlot slot) {
  // Direct NSS calls must be made on a worker thread (not the IO/UI threads).
  DCHECK(!content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
  DCHECK(!content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  // NSS cert must be non null.
  if (!nss_cert)
    return std::nullopt;

  // TODO(b/193771095) Use a valid wincx.
  // Must have a private key in order to access label and ID.
  SECKEYPrivateKey* private_key =
      PK11_FindKeyByAnyCert(nss_cert.get(), nullptr /* wincx */);
  // TODO(b/193771180) Investigate race condition with null private keys.
  if (!private_key)
    return std::nullopt;
  crypto::ScopedSECKEYPrivateKey priv_key_destroyer(private_key);

  // Must have a nickname (PKCS#11 CKA_LABEL).
  char* nickname = PK11_GetPrivateKeyNickname(private_key);
  if (!nickname)
    return std::nullopt;
  std::string pkcs11_label(nickname);
  PORT_Free(nickname);

  // Finally, must have an ID item (PKCS#11 CKA_ID).
  SECItem* id_item = PK11_GetLowLevelKeyIDForPrivateKey(private_key);
  if (!id_item)
    return std::nullopt;
  crypto::ScopedSECItem sec_item_destroyer(id_item);
  std::string pkcs11_id(id_item->data, id_item->data + id_item->len);

  // TODO(b/193784305) Try to avoid (some) key generation if possible.
  // Generate the placeholder RSA key that will be installed in ARC.
  auto placeholder_key = crypto::RSAPrivateKey::Create(2048);
  DCHECK(placeholder_key);

  return CertDescription(placeholder_key.release(), nss_cert.release(), slot,
                         pkcs11_label, pkcs11_id);
}

using BuildCertDescritionCallback =
    base::OnceCallback<void(std::optional<CertDescription> populated_cert)>;

// Tries to asynchronously create a |CertDescription| for the given |nss_cert|
// known to be stored in |slot| in a worker thread. Note direct NSS calls must
// be made at a worker thread.
void BuildCertDescription(net::ScopedCERTCertificate nss_cert,
                          keymanagement::mojom::ChapsSlot slot,
                          BuildCertDescritionCallback callback) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&BuildCertDescritionOnWorkerThread, std::move(nss_cert),
                     slot),
      std::move(callback));
}

// Returns the list of Chrome OS keys with the data arc-keymasterd needs to find
// and execute operations on the certs in |cert_descriptions| through chaps.
// Each ChromeOsKey instance contains a ChapsKeyData with its CKA_LABEL,
// CKA_ID, and the slot where it's stored. Note this slot is NOT the PKCS#11
// CK_SLOT_ID, but a more abstract representation that can be used to find the
// corresponding CK_SLOT_ID at runtime.
std::vector<keymaster::mojom::ChromeOsKeyPtr> PrepareChromeOsKeysForKeymaster(
    const std::vector<CertDescription>& cert_descriptions) {
  std::vector<keymaster::mojom::ChromeOsKeyPtr> chrome_os_keys;
  for (const auto& certificate : cert_descriptions) {
    // Build a mojo ChromeOsKey and store it in the output vector.
    keymaster::mojom::ChapsKeyDataPtr key_data =
        keymaster::mojom::ChapsKeyData::New(certificate.label, certificate.id,
                                            certificate.slot);
    keymaster::mojom::ChromeOsKeyPtr key = keymaster::mojom::ChromeOsKey::New(
        ExportSpki(certificate.placeholder_key.get()),
        keymaster::mojom::KeyData::NewChapsKeyData(std::move(key_data)));

    chrome_os_keys.push_back(std::move(key));
  }

  return chrome_os_keys;
}

// Returns the list of Chrome OS keys with the data arc-keymintd needs to find
// and execute operations on the certs in |cert_descriptions| through chaps.
// Each ChromeOsKey instance contains a ChapsKeyData with its CKA_LABEL,
// CKA_ID, and the slot where it's stored. Note this slot is NOT the PKCS#11
// CK_SLOT_ID, but a more abstract representation that can be used to find the
// corresponding CK_SLOT_ID at runtime.
std::vector<keymint::mojom::ChromeOsKeyPtr> PrepareChromeOsKeysForKeyMint(
    const std::vector<CertDescription>& cert_descriptions) {
  std::vector<keymint::mojom::ChromeOsKeyPtr> chrome_os_keys;
  for (const auto& certificate : cert_descriptions) {
    // Build a mojo ChromeOsKey and store it in the output vector.
    keymint::mojom::ChapsKeyDataPtr key_data =
        keymint::mojom::ChapsKeyData::New(certificate.label, certificate.id,
                                          certificate.slot);
    keymint::mojom::ChromeOsKeyPtr key = keymint::mojom::ChromeOsKey::New(
        ExportSpki(certificate.placeholder_key.get()),
        keymint::mojom::KeyData::NewChapsKeyData(std::move(key_data)));

    chrome_os_keys.push_back(std::move(key));
  }

  return chrome_os_keys;
}

}  // namespace

CertStoreService::CertStoreService(content::BrowserContext* context)
    : CertStoreService(context, std::make_unique<ArcCertInstaller>(context)) {}

CertStoreService::CertStoreService(content::BrowserContext* context,
                                   std::unique_ptr<ArcCertInstaller> installer)
    : context_(context), installer_(std::move(installer)) {
  // Do not perform any actions if context is nullptr for unit tests.
  if (!context_)
    return;

  net::CertDatabase::GetInstance()->AddObserver(this);

  UpdateCertificates();
}

CertStoreService::~CertStoreService() {
  if (context_)
    net::CertDatabase::GetInstance()->RemoveObserver(this);
}

void CertStoreService::OnClientCertStoreChanged() {
  UpdateCertificates();
}

void CertStoreService::UpdateCertificates() {
  ListCerts(context_, keymanagement::mojom::ChapsSlot::kUser,
            base::BindOnce(&CertStoreService::OnCertificatesListed,
                           weak_ptr_factory_.GetWeakPtr(),
                           keymanagement::mojom::ChapsSlot::kUser,
                           std::vector<CertDescription>()));
}

void CertStoreService::OnCertificatesListed(
    keymanagement::mojom::ChapsSlot slot,
    std::vector<CertDescription> cert_descriptions,
    net::ScopedCERTCertificateList cert_list) {
  base::queue<net::ScopedCERTCertificate> cert_queue;
  for (auto& cert : cert_list) {
    if (cert)
      cert_queue.push(std::move(cert));
  }

  BuildAllowedCertDescriptionsRecursively(
      base::BindOnce(&CertStoreService::OnBuiltAllowedCertDescriptions,
                     weak_ptr_factory_.GetWeakPtr(), slot),
      slot, std::move(cert_queue), std::move(cert_descriptions));
}

// TODO(b/193785308) Try to simplify these recursive calls.
void CertStoreService::BuildAllowedCertDescriptionsRecursively(
    BuildAllowedCertDescriptionsCallback callback,
    keymanagement::mojom::ChapsSlot slot,
    base::queue<net::ScopedCERTCertificate> cert_queue,
    std::vector<CertDescription> allowed_certs) const {
  if (cert_queue.empty()) {
    std::move(callback).Run(std::move(allowed_certs));
    return;
  }

  net::ScopedCERTCertificate cert = std::move(cert_queue.front());
  DCHECK(cert);
  cert_queue.pop();

  scoped_refptr<net::X509Certificate> x509_cert =
      net::x509_util::CreateX509CertificateFromCERTCertificate(cert.get());

  if (!x509_cert) {
    BuildAllowedCertDescriptionsRecursively(std::move(callback), slot,
                                            std::move(cert_queue),
                                            std::move(allowed_certs));
    return;
  }

  IsCertificateAllowed(
      base::BindOnce(&CertStoreService::BuildAllowedCertDescriptionAndRecurse,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback), slot,
                     std::move(cert_queue), std::move(allowed_certs),
                     std::move(cert)),
      std::move(x509_cert), context_);
}

void CertStoreService::BuildAllowedCertDescriptionAndRecurse(
    BuildAllowedCertDescriptionsCallback callback,
    keymanagement::mojom::ChapsSlot slot,
    base::queue<net::ScopedCERTCertificate> cert_queue,
    std::vector<CertDescription> allowed_certs,
    net::ScopedCERTCertificate cert,
    bool certificate_allowed) const {
  // Continue to build a CertDescription if the cert is allowed.
  if (certificate_allowed) {
    BuildCertDescription(
        std::move(cert), slot,
        base::BindOnce(&CertStoreService::AppendCertDescriptionAndRecurse,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                       slot, std::move(cert_queue), std::move(allowed_certs)));
    return;
  }
  // Otherwise proceed to the rest of the queue recursively.
  BuildAllowedCertDescriptionsRecursively(std::move(callback), slot,
                                          std::move(cert_queue),
                                          std::move(allowed_certs));
}

void CertStoreService::AppendCertDescriptionAndRecurse(
    BuildAllowedCertDescriptionsCallback callback,
    keymanagement::mojom::ChapsSlot slot,
    base::queue<net::ScopedCERTCertificate> cert_queue,
    std::vector<CertDescription> allowed_certs,
    std::optional<CertDescription> cert_description) const {
  if (cert_description.has_value())
    allowed_certs.emplace_back(std::move(cert_description.value()));

  BuildAllowedCertDescriptionsRecursively(std::move(callback), slot,
                                          std::move(cert_queue),
                                          std::move(allowed_certs));
}

void CertStoreService::OnBuiltAllowedCertDescriptions(
    keymanagement::mojom::ChapsSlot slot,
    std::vector<CertDescription> cert_descriptions) const {
  if (ShouldUseArcKeyMint()) {
    OnBuiltAllowedCertDescriptionsForKeyMint(slot,
                                             std::move(cert_descriptions));
  } else {
    OnBuiltAllowedCertDescriptionsForKeymaster(slot,
                                               std::move(cert_descriptions));
  }
}

void CertStoreService::OnBuiltAllowedCertDescriptionsForKeyMint(
    keymanagement::mojom::ChapsSlot slot,
    std::vector<CertDescription> cert_descriptions) const {
  ArcKeyMintBridge* const keymint_bridge =
      ArcKeyMintBridge::GetForBrowserContext(context_);
  if (!keymint_bridge) {
    LOG(ERROR) << "Missing instance of ArcKeyMintBridge.";
    return;
  }

  if (slot == keymanagement::mojom::ChapsSlot::kUser) {
    ListCertsInSystemSlot(std::move(cert_descriptions));
    return;
  }
  // At this point certs have been gathered from all available slots (i.e. user
  // slot and potentially system slot if access is allowed to this user),
  // proceed to send them to arc-keymint and ARC.
  std::vector<keymint::mojom::ChromeOsKeyPtr> keys =
      PrepareChromeOsKeysForKeyMint(cert_descriptions);
  keymint_bridge->UpdatePlaceholderKeys(
      std::move(keys), base::BindOnce(&CertStoreService::OnUpdatedKeys,
                                      weak_ptr_factory_.GetMutableWeakPtr(),
                                      std::move(cert_descriptions)));
}

void CertStoreService::OnBuiltAllowedCertDescriptionsForKeymaster(
    keymanagement::mojom::ChapsSlot slot,
    std::vector<CertDescription> cert_descriptions) const {
  ArcKeymasterBridge* const keymaster_bridge =
      ArcKeymasterBridge::GetForBrowserContext(context_);
  if (!keymaster_bridge) {
    LOG(ERROR) << "Missing instance of ArcKeymasterBridge.";
    return;
  }

  if (slot == keymanagement::mojom::ChapsSlot::kUser) {
    ListCertsInSystemSlot(std::move(cert_descriptions));
    return;
  }
  // At this point certs have been gathered from all available slots (i.e. user
  // slot and potentially system slot if access is allowed to this user),
  // proceed to send them to arc-keymaster and ARC.
  std::vector<keymaster::mojom::ChromeOsKeyPtr> keys =
      PrepareChromeOsKeysForKeymaster(cert_descriptions);
  keymaster_bridge->UpdatePlaceholderKeys(
      std::move(keys), base::BindOnce(&CertStoreService::OnUpdatedKeys,
                                      weak_ptr_factory_.GetMutableWeakPtr(),
                                      std::move(cert_descriptions)));
}

void CertStoreService::ListCertsInSystemSlot(
    std::vector<CertDescription> cert_descriptions) const {
  ListCerts(context_, keymanagement::mojom::ChapsSlot::kSystem,
            base::BindOnce(&CertStoreService::OnCertificatesListed,
                           weak_ptr_factory_.GetMutableWeakPtr(),
                           keymanagement::mojom::ChapsSlot::kSystem,
                           std::move(cert_descriptions)));
}

void CertStoreService::OnUpdatedKeys(
    std::vector<CertDescription> certificate_descriptions,
    bool success) {
  if (!success) {
    LOG(WARNING) << "Could not update placeholder keys with keymaster.";
    return;
  }

  bool updated = certificate_cache_.Update(certificate_descriptions);

  installer_->InstallArcCerts(
      std::move(certificate_descriptions),
      base::BindOnce(&CertStoreService::OnArcCertsInstalled,
                     weak_ptr_factory_.GetWeakPtr(), updated));
}

CertStoreService::CertificateCache::CertificateCache() = default;
CertStoreService::CertificateCache::~CertificateCache() = default;

bool CertStoreService::CertificateCache::Update(
    const std::vector<CertDescription>& cert_descriptions) {
  std::set<std::string> new_required_cert_names;
  for (const auto& certificate : cert_descriptions) {
    CERTCertificate* nss_cert = certificate.nss_cert.get();
    DCHECK(nss_cert);

    // Fetch certificate name.
    std::string cert_name =
        x509_certificate_model::GetCertNameOrNickname(nss_cert);

    new_required_cert_names.insert(cert_name);
  }
  bool need_policy_update = (required_cert_names_ != new_required_cert_names);
  required_cert_names_ = new_required_cert_names;
  return need_policy_update;
}

void CertStoreService::OnArcCertsInstalled(bool need_policy_update,
                                           bool success) {
  VLOG(1) << "ARC certificates installation has finished with result="
          << success;
  if (need_policy_update) {
    ArcPolicyBridge* const policy_bridge =
        ArcPolicyBridge::GetForBrowserContext(context_);
    if (policy_bridge) {
      policy_bridge->OnPolicyUpdated(policy::PolicyNamespace(),
                                     policy::PolicyMap(), policy::PolicyMap());
    }
  }
}

}  // namespace arc

// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_INSTALL_VERIFIER_H_
#define CHROME_BROWSER_EXTENSIONS_INSTALL_VERIFIER_H_

#include <memory>
#include <set>
#include <string>

#include "base/containers/queue.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/keyed_service/core/keyed_service.h"
#include "extensions/browser/management_policy.h"
#include "extensions/common/extension_id.h"

namespace content {
class BrowserContext;
}

namespace extensions {
class Extension;
class ExtensionPrefs;
class InstallSigner;
struct InstallSignature;

// This class implements verification that a set of extensions are either from
// the webstore or are allowlisted by enterprise policy.  The webstore
// verification process works by sending a request to a backend server to get a
// signature proving that a set of extensions are verified. This signature is
// written into the extension preferences and is checked for validity when
// being read back again.
//
// This class should be kept notified of runtime changes to the set of
// extensions installed from the webstore.
class InstallVerifier : public KeyedService,
                        public ManagementPolicy::Provider {
 public:
  InstallVerifier(ExtensionPrefs* prefs, content::BrowserContext* context);

  InstallVerifier(const InstallVerifier&) = delete;
  InstallVerifier& operator=(const InstallVerifier&) = delete;

  ~InstallVerifier() override;

  // Convenience method to return the InstallVerifier for a given |context|.
  static InstallVerifier* Get(content::BrowserContext* context);

  // Returns whether install verification should be enforced.
  static bool ShouldEnforce();

  // Returns whether |extension| is of a type that needs verification.
  static bool NeedsVerification(const Extension& extension,
                                content::BrowserContext* context);

  // Determines if an extension claims to be from the webstore.
  static bool IsFromStore(const Extension& extension,
                          content::BrowserContext* context);

  // Initializes this object for use, including reading preferences and
  // validating the stored signature.
  void Init();

  // Returns the timestamp of our InstallSignature, if we have one.
  base::Time SignatureTimestamp();

  // Returns true if |id| is either verified or our stored signature explicitly
  // tells us that it was invalid when we asked the server about it.
  bool IsKnownId(const std::string& id) const;

  // Returns whether the given |id| is considered invalid by our verified
  // signature.
  bool IsInvalid(const std::string& id) const;

  // Attempts to verify a single extension and add it to the verified list.
  void VerifyExtension(const std::string& extension_id);

  // Attempts to verify all extensions.
  void VerifyAllExtensions();

  // Call this to add a set of ids that will immediately be considered allowed,
  // and kick off an aysnchronous request to Add.
  void AddProvisional(const ExtensionIdSet& ids);

  // Removes an id or set of ids from the verified list.
  void Remove(const std::string& id);
  void RemoveMany(const ExtensionIdSet& ids);

  // Returns whether an extension id is allowed by policy.
  bool AllowedByEnterprisePolicy(const std::string& id) const;

  // ManagementPolicy::Provider interface.
  std::string GetDebugPolicyProviderName() const override;
  bool MustRemainDisabled(const Extension* extension,
                          disable_reason::DisableReason* reason,
                          std::u16string* error) const override;

 private:
  // We keep a list of operations to the current set of extensions.
  enum OperationType {
    ADD_SINGLE,         // Adding a single extension to be verified.
    ADD_ALL,            // Adding all extensions to be verified.
    ADD_ALL_BOOTSTRAP,  // Adding all extensions because of a bootstrapping.
    ADD_PROVISIONAL,    // Adding one or more provisionally-allowed extensions.
    REMOVE              // Remove one or more extensions.
  };

  // This is an operation we want to apply to the current set of verified ids.
  struct PendingOperation {
    OperationType type;

    // This is the set of ids being either added or removed.
    ExtensionIdSet ids;

    explicit PendingOperation(OperationType type);
    ~PendingOperation();
  };

  // Returns the set of IDs for all extensions that potentially need to be
  // verified.
  ExtensionIdSet GetExtensionsToVerify() const;

  // Bootstrap the InstallVerifier if we do not already have a signature, or if
  // there are unknown extensions which need to be verified.
  void MaybeBootstrapSelf();

  // Try adding a new set of |ids| to the list of verified ids.
  void AddMany(const ExtensionIdSet& ids, OperationType type);

  // Record the result of the verification for the histograms, and notify the
  // ExtensionPrefs if we verified all extensions.
  void OnVerificationComplete(bool success, OperationType type);

  // Removes any no-longer-installed ids, requesting a new signature if needed.
  void GarbageCollect();

  // Returns whether the given |id| is included in our verified signature.
  bool IsVerified(const std::string& id) const;

  // Returns true if the extension with |id| was installed later than the
  // timestamp of our signature.
  bool WasInstalledAfterSignature(const std::string& id) const;

  // Begins the process of fetching a new signature, based on applying the
  // operation at the head of the queue to the current set of ids in
  // |signature_| (if any) and then sending a request to sign that.
  void BeginFetch();

  // Saves the current value of |signature_| to the prefs;
  void SaveToPrefs();

  // Called with the result of a signature request, or NULL on failure.
  void SignatureCallback(std::unique_ptr<InstallSignature> signature);

  raw_ptr<ExtensionPrefs> prefs_;

  // The context with which the InstallVerifier is associated.
  raw_ptr<content::BrowserContext> context_;

  // Have we finished our bootstrap check yet?
  bool bootstrap_check_complete_;

  // This is the most up-to-date signature, read out of |prefs_| during
  // initialization and updated anytime we get new id's added.
  std::unique_ptr<InstallSignature> signature_;

  // The current InstallSigner, if we have a signature request running.
  std::unique_ptr<InstallSigner> signer_;

  // A queue of operations to apply to the current set of allowed ids.
  base::queue<std::unique_ptr<PendingOperation>> operation_queue_;

  // A set of ids that have been provisionally added, which we're willing to
  // consider allowed until we hear back from the server signature request.
  ExtensionIdSet provisional_;

  base::WeakPtrFactory<InstallVerifier> weak_factory_{this};
};

// Instances of this class can be constructed to disable install verification
// during tests.
class ScopedInstallVerifierBypassForTest {
 public:
  enum ForceType {
    kForceOn,
    kForceOff,
  };

  explicit ScopedInstallVerifierBypassForTest(ForceType force_type = kForceOff);

  ScopedInstallVerifierBypassForTest(
      const ScopedInstallVerifierBypassForTest&) = delete;
  ScopedInstallVerifierBypassForTest& operator=(
      const ScopedInstallVerifierBypassForTest&) = delete;

  ~ScopedInstallVerifierBypassForTest();

 private:
  ForceType value_;
  raw_ptr<ForceType> old_value_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_INSTALL_VERIFIER_H_

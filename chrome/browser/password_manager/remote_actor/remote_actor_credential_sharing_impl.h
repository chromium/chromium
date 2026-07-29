// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_REMOTE_ACTOR_REMOTE_ACTOR_CREDENTIAL_SHARING_IMPL_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_REMOTE_ACTOR_REMOTE_ACTOR_CREDENTIAL_SHARING_IMPL_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/common/password_manager/remote_actor_credential_sharing.mojom.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_store/password_store_consumer.h"
#include "content/public/browser/document_user_data.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"

class Profile;

namespace content {
class RenderFrameHost;
class WebContents;
}  // namespace content

namespace device_reauth {
class DeviceAuthenticator;
}

namespace password_manager {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(RemoteActorCredentialSharingResult)
enum class RemoteActorCredentialSharingResult {
  kOtherError = 0,
  kSuccess = 1,
  kUserIdentityOrSyncStateInvalid = 2,
  kNoSyncOrAccountStorage = 3,
  kNoPasswordsFound = 4,
  kUserCancelledDialog = 5,
  kAuthenticatorFailed = 6,
  kSharingServiceUnavailable = 7,
  kSharingFailed = 8,
  kMaxValue = kSharingFailed,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/password/enums.xml:RemoteActorCredentialSharingResult)

class RemoteActorSelectionDialogController;

class RemoteActorCredentialSharingImpl
    : public chrome::mojom::RemoteActorCredentialSharing,
      public content::DocumentUserData<RemoteActorCredentialSharingImpl>,
      public PasswordStoreConsumer {
 public:
  // Factory callback type used to create the credential selection dialog.
  using DialogFactory = base::RepeatingCallback<std::unique_ptr<
      RemoteActorSelectionDialogController>(
      content::WebContents* web_contents,
      std::vector<std::unique_ptr<PasswordForm>> credentials,
      const std::string& credential_domain,
      base::OnceCallback<void(std::optional<PasswordForm>)>
          callback)>;
  ~RemoteActorCredentialSharingImpl() override;
  RemoteActorCredentialSharingImpl(const RemoteActorCredentialSharingImpl&) =
      delete;
  RemoteActorCredentialSharingImpl& operator=(
      const RemoteActorCredentialSharingImpl&) = delete;

  static void BindReceiver(
      mojo::PendingAssociatedReceiver<
          chrome::mojom::RemoteActorCredentialSharing> receiver,
      content::RenderFrameHost* rfh);

  void Bind(mojo::PendingAssociatedReceiver<
            chrome::mojom::RemoteActorCredentialSharing> receiver);

  // chrome::mojom::RemoteActorCredentialSharing:
  void RequestAgentAuthentication(
      const std::string& gaia_id,
      const std::string& domain,
      const std::string& remote_actor_id,
      RequestAgentAuthenticationCallback callback) override;

 private:
  static DialogFactory CreateDefaultFactory();

  explicit RemoteActorCredentialSharingImpl(
      content::RenderFrameHost* rfh,
      DialogFactory dialog_factory = CreateDefaultFactory());

  friend class content::DocumentUserData<RemoteActorCredentialSharingImpl>;
  DOCUMENT_USER_DATA_KEY_DECL();

  struct PendingRequest {
    // The GAIA ID of the user requesting authentication.
    std::string gaia_id;
    // The credential domain for which credentials are requested.
    std::string domain;
    // The ID of the remote actor requesting credentials.
    std::string remote_actor_id;
    // The callback to return the result to the Mojo caller.
    RequestAgentAuthenticationCallback callback;
    // The number of password store queries started.
    int expected_callbacks = 0;
    // The number of password store queries that have completed.
    int received_callbacks = 0;
    // Collected credentials from the stores.
    std::vector<PasswordForm> credentials;
  };

  // PasswordStoreConsumer:
  void OnGetPasswordStoreResultsOrErrorFrom(
      PasswordStoreInterface* store,
      LoginsResultOrError results_or_error) override;

  // Called when all password store queries have completed.
  void OnAllLoginsRetrieved();
  void ProceedWithCredential(PasswordForm selected_form, bool auth_success);

  // Validates Mojo request preconditions (e.g., primary main frame, user gesture).
  bool ValidateRequestPreconditions(const std::string& gaia_id,
                                    const std::string& domain,
                                    const std::string& remote_actor_id);

  // Verifies the GAIA ID matches the signed-in user and sync is not in error.
  bool VerifyUserIdentityAndSyncState(Profile* profile,
                                      const std::string& gaia_id);

  // Initiates asynchronous queries to the relevant password stores.
  void QueryPasswordStores(Profile* profile,
                           const std::string& gaia_id,
                           const std::string& domain,
                           const std::string& remote_actor_id,
                           RequestAgentAuthenticationCallback callback);

  // Callback triggered when the user selects a credential or cancels the dialog.
  void OnDialogResult(std::optional<PasswordForm> selected_form);

  // Callback triggered when the sharing service completes the operation.
  void OnShareCompleted(RequestAgentAuthenticationCallback callback,
                        bool success);

  // Asynchronously posts a failure response to the Mojo callback.
  void RespondWithError(RequestAgentAuthenticationCallback callback);

  mojo::AssociatedReceiver<chrome::mojom::RemoteActorCredentialSharing>
      receiver_;

  DialogFactory dialog_factory_;

  std::optional<PendingRequest> pending_request_;
  std::unique_ptr<RemoteActorSelectionDialogController> dialog_controller_;
  std::unique_ptr<device_reauth::DeviceAuthenticator> device_authenticator_;

  base::WeakPtrFactory<RemoteActorCredentialSharingImpl> weak_ptr_factory_{
      this};
};

}  // namespace password_manager

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_REMOTE_ACTOR_REMOTE_ACTOR_CREDENTIAL_SHARING_IMPL_H_

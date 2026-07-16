// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_REMOTE_ACTOR_REMOTE_ACTOR_CREDENTIAL_SHARING_IMPL_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_REMOTE_ACTOR_REMOTE_ACTOR_CREDENTIAL_SHARING_IMPL_H_

#include <string>

#include "chrome/common/password_manager/remote_actor_credential_sharing.mojom.h"
#include "content/public/browser/document_user_data.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"

namespace content {
class RenderFrameHost;
}  // namespace content

namespace password_manager {

class RemoteActorCredentialSharingImpl
    : public chrome::mojom::RemoteActorCredentialSharing,
      public content::DocumentUserData<RemoteActorCredentialSharingImpl> {
 public:
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

  void RequestAgentAuthentication(
      const std::string& gaia_id,
      const std::string& domain,
      const std::string& remote_actor_id,
      RequestAgentAuthenticationCallback callback) override;

 private:
  explicit RemoteActorCredentialSharingImpl(content::RenderFrameHost* rfh);

  friend class content::DocumentUserData<RemoteActorCredentialSharingImpl>;
  DOCUMENT_USER_DATA_KEY_DECL();

  mojo::AssociatedReceiver<chrome::mojom::RemoteActorCredentialSharing>
      receiver_;
};

}  // namespace password_manager

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_REMOTE_ACTOR_REMOTE_ACTOR_CREDENTIAL_SHARING_IMPL_H_

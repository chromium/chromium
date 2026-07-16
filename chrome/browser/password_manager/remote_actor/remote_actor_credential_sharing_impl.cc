// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/remote_actor/remote_actor_credential_sharing_impl.h"

#include "base/logging.h"
#include "chrome/common/password_manager/remote_actor_credential_sharing_policy.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/associated_receiver_set.h"
#include "mojo/public/cpp/bindings/message.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace password_manager {

DOCUMENT_USER_DATA_KEY_IMPL(RemoteActorCredentialSharingImpl);

// static
void RemoteActorCredentialSharingImpl::BindReceiver(
    mojo::PendingAssociatedReceiver<chrome::mojom::RemoteActorCredentialSharing>
        receiver,
    content::RenderFrameHost* rfh) {
  if (!rfh->IsInPrimaryMainFrame()) {
    return;
  }
  if (!password_manager::IsRemoteActorCredentialSharingAllowedForOrigin(
          rfh->GetLastCommittedOrigin())) {
    rfh->GetProcess()->ShutdownForBadMessage(
        content::RenderProcessHost::CrashReportMode::GENERATE_CRASH_DUMP);
    return;
  }

  auto* impl =
      RemoteActorCredentialSharingImpl::GetOrCreateForCurrentDocument(rfh);
  impl->Bind(std::move(receiver));
}

RemoteActorCredentialSharingImpl::RemoteActorCredentialSharingImpl(
    content::RenderFrameHost* rfh)
    : content::DocumentUserData<RemoteActorCredentialSharingImpl>(rfh),
      receiver_(this) {}

RemoteActorCredentialSharingImpl::~RemoteActorCredentialSharingImpl() = default;

void RemoteActorCredentialSharingImpl::Bind(
    mojo::PendingAssociatedReceiver<chrome::mojom::RemoteActorCredentialSharing>
        receiver) {
  receiver_.Bind(std::move(receiver));
}

void RemoteActorCredentialSharingImpl::RequestAgentAuthentication(
    const std::string& gaia_id,
    const std::string& domain,
    const std::string& remote_actor_id,
    RequestAgentAuthenticationCallback callback) {
  content::RenderFrameHost& target_frame = render_frame_host();

  if (!target_frame.IsInPrimaryMainFrame()) {
    receiver_.ReportBadMessage(
        "RemoteActorCredentialSharing: Request from subframe");
    std::move(callback).Run(false);
    return;
  }

  if (!target_frame.HasTransientUserActivation()) {
    receiver_.ReportBadMessage(
        "RemoteActorCredentialSharing: Request without user gesture");
    std::move(callback).Run(false);
    return;
  }

  auto* policy = content::ChildProcessSecurityPolicy::GetInstance();
  if (!policy->CanAccessDataForOrigin(
          target_frame.GetProcess()->GetID().GetUnsafeValue(),
          target_frame.GetLastCommittedOrigin())) {
    receiver_.ReportBadMessage(
        "RemoteActorCredentialSharing: Process cannot access origin");
    std::move(callback).Run(false);
    return;
  }

  if (gaia_id.length() >= 256 || domain.length() >= 256 ||
      remote_actor_id.length() >= 256) {
    receiver_.ReportBadMessage(
        "RemoteActorCredentialSharing: Argument length limit exceeded");
    std::move(callback).Run(false);
    return;
  }

  // TODO(crbug.com/532482931): Replace mock authentication with the real
  // implementation that uses the RemoteActorCredentialSharingService/OAuth2
  // flow. For now, always return false.
  std::move(callback).Run(false);
}

}  // namespace password_manager

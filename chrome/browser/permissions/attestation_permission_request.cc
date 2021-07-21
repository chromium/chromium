// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/attestation_permission_request.h"

#include "base/callback.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/grit/generated_resources.h"
#include "components/permissions/permission_request.h"
#include "components/permissions/request_type.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/origin.h"

// AttestationPermissionRequest is a delegate class that provides information
// and callbacks to the PermissionRequestManager.
//
// PermissionRequestManager has a reference to this object and so this object
// must outlive it. Since attestation requests are never canceled,
// PermissionRequestManager guarantees that `PermissionRequest::RequestFinished`
// will always, eventually, be called. This object uses that fact to delete
// itself during `DeleteRequest` and thus owns itself.
class AttestationPermissionRequest : public permissions::PermissionRequest {
 public:
  AttestationPermissionRequest(const url::Origin& requesting_origin,
                               base::OnceCallback<void(bool)> callback)
      : PermissionRequest(
            requesting_origin.GetURL(),
            permissions::RequestType::kSecurityAttestation,
            /*has_gesture=*/false,
            base::BindOnce(&AttestationPermissionRequest::PermissionDecided,
                           base::Unretained(this)),
            base::BindOnce(&AttestationPermissionRequest::DeleteRequest,
                           base::Unretained(this))),
        callback_(std::move(callback)) {}

  ~AttestationPermissionRequest() override = default;

  void PermissionDecided(ContentSetting result, bool is_one_time) {
    DCHECK(!is_one_time);
    std::move(callback_).Run(result == CONTENT_SETTING_ALLOW);
  }

  void DeleteRequest() {
    // callback_ may not have run if the prompt was ignored. (I.e. the tab was
    // closed while the prompt was displayed.)
    if (callback_)
      std::move(callback_).Run(false);
    delete this;
  }

 private:
  base::OnceCallback<void(bool)> callback_;

  DISALLOW_COPY_AND_ASSIGN(AttestationPermissionRequest);
};

permissions::PermissionRequest* NewAttestationPermissionRequest(
    const url::Origin& origin,
    base::OnceCallback<void(bool)> callback) {
  return new AttestationPermissionRequest(origin, std::move(callback));
}

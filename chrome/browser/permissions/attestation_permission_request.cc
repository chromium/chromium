// Copyright 2018 The Chromium Authors
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

// U2fApiPermissionRequest is a delegate class that provides information
// and callbacks to the PermissionRequestManager.
//
// PermissionRequestManager has a reference to this object and so this object
// must outlive it. Since attestation requests are never canceled,
// PermissionRequestManager guarantees that `PermissionRequest::RequestFinished`
// will always, eventually, be called. This object uses that fact to delete
// itself during `DeleteRequest` and thus owns itself.
class U2fApiPermissionRequest : public permissions::PermissionRequest {
 public:
  U2fApiPermissionRequest(permissions::RequestType type,
                          const url::Origin& requesting_origin,
                          base::OnceCallback<void(bool)> callback)
      : PermissionRequest(
            requesting_origin.GetURL(),
            type,
            /*has_gesture=*/false,
            base::BindRepeating(&U2fApiPermissionRequest::PermissionDecided,
                                base::Unretained(this)),
            base::BindOnce(&U2fApiPermissionRequest::DeleteRequest,
                           base::Unretained(this))),
        callback_(std::move(callback)) {}
  ~U2fApiPermissionRequest() override = default;

  void PermissionDecided(ContentSetting result,
                         bool is_one_time,
                         bool is_final_decision) {
    DCHECK(!is_one_time);
    DCHECK(is_final_decision);

    if (callback_) {
      std::move(callback_).Run(result == CONTENT_SETTING_ALLOW);
    }
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
};

permissions::PermissionRequest* NewAttestationPermissionRequest(
    const url::Origin& origin,
    base::OnceCallback<void(bool)> callback) {
  return new U2fApiPermissionRequest(
      permissions::RequestType::kSecurityAttestation, origin,
      std::move(callback));
}

permissions::PermissionRequest* NewU2fApiPermissionRequest(
    const url::Origin& origin,
    base::OnceCallback<void(bool)> callback) {
  return new U2fApiPermissionRequest(permissions::RequestType::kU2fApiRequest,
                                     origin, std::move(callback));
}

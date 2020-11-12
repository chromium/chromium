// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/attestation_permission_request.h"

#include "base/callback.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/grit/generated_resources.h"
#include "components/permissions/permission_request.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/origin.h"

// AttestationPermissionRequest is a delegate class that provides information
// and callbacks to the PermissionRequestManager.
//
// PermissionRequestManager has a reference to this object and so this object
// must outlive it. Since attestation requests are never canceled,
// PermissionRequestManager guarentees that |RequestFinished| will always,
// eventually, be called. This object uses that fact to delete itself during
// |RequestFinished| and thus owns itself.
class AttestationPermissionRequest : public permissions::PermissionRequest {
 public:
  AttestationPermissionRequest(const url::Origin& origin,
                               base::OnceCallback<void(bool)> callback)
      : origin_(origin), callback_(std::move(callback)) {}

  permissions::PermissionRequest::IconId GetIconId() const override {
    return kUsbSecurityKeyIcon;
  }

  base::string16 GetMessageTextFragment() const override {
    return l10n_util::GetStringUTF16(
        IDS_SECURITY_KEY_ATTESTATION_PERMISSION_FRAGMENT);
  }
  GURL GetOrigin() const override { return origin_.GetURL(); }
  void PermissionGranted() override { std::move(callback_).Run(true); }
  void PermissionDenied() override { std::move(callback_).Run(false); }
  void Cancelled() override { std::move(callback_).Run(false); }

  void RequestFinished() override {
    // callback_ may not have run if the prompt was ignored. (I.e. the tab was
    // closed while the prompt was displayed.)
    if (callback_)
      std::move(callback_).Run(false);
    delete this;
  }

  permissions::PermissionRequestType GetPermissionRequestType() const override {
    return permissions::PermissionRequestType::
        PERMISSION_SECURITY_KEY_ATTESTATION;
  }

 private:
  ~AttestationPermissionRequest() override = default;

  const url::Origin origin_;
  base::OnceCallback<void(bool)> callback_;

  DISALLOW_COPY_AND_ASSIGN(AttestationPermissionRequest);
};

permissions::PermissionRequest* NewAttestationPermissionRequest(
    const url::Origin& origin,
    base::OnceCallback<void(bool)> callback) {
  return new AttestationPermissionRequest(origin, std::move(callback));
}

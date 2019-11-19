// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERMISSIONS_PERMISSION_REQUEST_IMPL_H_
#define CHROME_BROWSER_PERMISSIONS_PERMISSION_REQUEST_IMPL_H_

#include "base/callback.h"
#include "base/macros.h"
#include "chrome/browser/permissions/permission_request.h"
#include "chrome/browser/permissions/permission_request_id.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"

class GURL;

// Default implementation of PermissionRequest, it is assumed that the
// caller owns it and that it can be deleted once the |delete_callback|
// is executed.
class PermissionRequestImpl : public PermissionRequest {
 public:
  using PermissionDecidedCallback = base::OnceCallback<void(ContentSetting)>;

  PermissionRequestImpl(const GURL& request_origin,
                        ContentSettingsType content_settings_type,
                        bool has_gesture,
                        PermissionDecidedCallback permission_decided_callback,
                        base::OnceClosure delete_callback);

  ~PermissionRequestImpl() override;

 private:
  // PermissionRequest:
  IconId GetIconId() const override;
#if defined(OS_ANDROID)
  base::string16 GetTitleText() const override;
  base::string16 GetMessageText() const override;
  base::string16 GetQuietTitleText() const override;
  base::string16 GetQuietMessageText() const override;
#endif
  base::string16 GetMessageTextFragment() const override;
  GURL GetOrigin() const override;
  void PermissionGranted() override;
  void PermissionDenied() override;
  void Cancelled() override;
  void RequestFinished() override;
  PermissionRequestType GetPermissionRequestType() const override;
  PermissionRequestGestureType GetGestureType() const override;
  ContentSettingsType GetContentSettingsType() const override;

  GURL request_origin_;
  ContentSettingsType content_settings_type_;
  bool has_gesture_;

  // Called once a decision is made about the permission.
  PermissionDecidedCallback permission_decided_callback_;

  // Called when the request is no longer in use so it can be deleted by the
  // caller.
  base::OnceClosure delete_callback_;
  bool is_finished_;

  DISALLOW_COPY_AND_ASSIGN(PermissionRequestImpl);
};

#endif  // CHROME_BROWSER_PERMISSIONS_PERMISSION_REQUEST_IMPL_H_

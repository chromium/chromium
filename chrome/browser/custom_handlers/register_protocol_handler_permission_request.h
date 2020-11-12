// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CUSTOM_HANDLERS_REGISTER_PROTOCOL_HANDLER_PERMISSION_REQUEST_H_
#define CHROME_BROWSER_CUSTOM_HANDLERS_REGISTER_PROTOCOL_HANDLER_PERMISSION_REQUEST_H_

#include "base/callback_helpers.h"
#include "base/macros.h"
#include "chrome/common/custom_handlers/protocol_handler.h"
#include "components/permissions/permission_request.h"

class ProtocolHandlerRegistry;

// This class provides display data for a permission request, shown when a page
// wants to register a protocol handler and was triggered by a user action.
class RegisterProtocolHandlerPermissionRequest
    : public permissions::PermissionRequest {
 public:
  RegisterProtocolHandlerPermissionRequest(
      ProtocolHandlerRegistry* registry,
      const ProtocolHandler& handler,
      GURL url,
      bool user_gesture,
      base::ScopedClosureRunner fullscreen_block);
  ~RegisterProtocolHandlerPermissionRequest() override;

 private:
  // permissions::PermissionRequest:
  IconId GetIconId() const override;
  base::string16 GetMessageTextFragment() const override;
  GURL GetOrigin() const override;
  void PermissionGranted() override;
  void PermissionDenied() override;
  void Cancelled() override;
  void RequestFinished() override;
  permissions::PermissionRequestType GetPermissionRequestType() const override;

  ProtocolHandlerRegistry* registry_;
  ProtocolHandler handler_;
  GURL origin_;
  // Fullscreen will be blocked for the duration of the lifetime of this block.
  // TODO(avi): Move to either permissions::PermissionRequest or the
  // PermissionRequestManager?
  base::ScopedClosureRunner fullscreen_block_;

  DISALLOW_COPY_AND_ASSIGN(RegisterProtocolHandlerPermissionRequest);
};

#endif  // CHROME_BROWSER_CUSTOM_HANDLERS_REGISTER_PROTOCOL_HANDLER_PERMISSION_REQUEST_H_

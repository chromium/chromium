// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/custom_handlers/register_protocol_handler_permission_request.h"

#include "base/metrics/user_metrics.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/custom_handlers/protocol_handler_registry.h"
#include "chrome/grit/generated_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"

RegisterProtocolHandlerPermissionRequest::
    RegisterProtocolHandlerPermissionRequest(
        ProtocolHandlerRegistry* registry,
        const ProtocolHandler& handler,
        GURL url,
        bool user_gesture,
        base::ScopedClosureRunner fullscreen_block)
    : registry_(registry),
      handler_(handler),
      origin_(url.GetOrigin()),
      fullscreen_block_(std::move(fullscreen_block)) {}

RegisterProtocolHandlerPermissionRequest::
    ~RegisterProtocolHandlerPermissionRequest() = default;

permissions::PermissionRequest::IconId
RegisterProtocolHandlerPermissionRequest::GetIconId() const {
  return vector_icons::kProtocolHandlerIcon;
}

base::string16
RegisterProtocolHandlerPermissionRequest::GetMessageTextFragment() const {
  ProtocolHandler old_handler = registry_->GetHandlerFor(handler_.protocol());
  return old_handler.IsEmpty()
             ? l10n_util::GetStringFUTF16(
                   IDS_REGISTER_PROTOCOL_HANDLER_CONFIRM_FRAGMENT,
                   handler_.GetProtocolDisplayName())
             : l10n_util::GetStringFUTF16(
                   IDS_REGISTER_PROTOCOL_HANDLER_CONFIRM_REPLACE_FRAGMENT,
                   handler_.GetProtocolDisplayName(),
                   base::UTF8ToUTF16(old_handler.url().host_piece()));
}

GURL RegisterProtocolHandlerPermissionRequest::GetOrigin() const {
  return origin_;
}

void RegisterProtocolHandlerPermissionRequest::PermissionGranted() {
  base::RecordAction(
      base::UserMetricsAction("RegisterProtocolHandler.Infobar_Accept"));
  registry_->OnAcceptRegisterProtocolHandler(handler_);
}

void RegisterProtocolHandlerPermissionRequest::PermissionDenied() {
  base::RecordAction(
      base::UserMetricsAction("RegisterProtocolHandler.InfoBar_Deny"));
  registry_->OnIgnoreRegisterProtocolHandler(handler_);
}

void RegisterProtocolHandlerPermissionRequest::Cancelled() {
  base::RecordAction(
      base::UserMetricsAction("RegisterProtocolHandler.InfoBar_Deny"));
  registry_->OnIgnoreRegisterProtocolHandler(handler_);
}

void RegisterProtocolHandlerPermissionRequest::RequestFinished() {
  delete this;
}

permissions::PermissionRequestType
RegisterProtocolHandlerPermissionRequest::GetPermissionRequestType() const {
  return permissions::PermissionRequestType::REGISTER_PROTOCOL_HANDLER;
}

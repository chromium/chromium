// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/extension_telemetry/remote_host_contacted_signal.h"
#include "base/strings/string_util.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"

namespace safe_browsing {

RemoteHostContactedSignal::RemoteHostContactedSignal(
    const extensions::ExtensionId& extension_id,
    const GURL& host_url,
    RemoteHostInfo::ProtocolType protocol)
    : ExtensionSignal(extension_id),
      remote_host_url_(host_url),
      protocol_(protocol),
      contact_initiator_(safe_browsing::RemoteHostInfo::EXTENSION) {}

RemoteHostContactedSignal::RemoteHostContactedSignal(
    const extensions::ExtensionId& extension_id,
    const GURL& host_url,
    RemoteHostInfo::ProtocolType protocol,
    RemoteHostInfo::ContactInitiator contact_initiator)
    : ExtensionSignal(extension_id),
      remote_host_url_(host_url),
      protocol_(protocol),
      contact_initiator_(contact_initiator) {}

RemoteHostContactedSignal::RemoteHostContactedSignal(
    const RemoteHostContactedSignal& other) = default;

RemoteHostContactedSignal::RemoteHostContactedSignal(
    RemoteHostContactedSignal&& other) = default;

RemoteHostContactedSignal& RemoteHostContactedSignal::operator=(
    const RemoteHostContactedSignal& other) = default;

RemoteHostContactedSignal& RemoteHostContactedSignal::operator=(
    RemoteHostContactedSignal&& other) = default;

RemoteHostContactedSignal::~RemoteHostContactedSignal() = default;

std::string RemoteHostContactedSignal::GetUniqueRemoteHostContactedId() const {
  return base::JoinString(
      {remote_host_url_.host(), RemoteHostInfo::ProtocolType_Name(protocol_),
       RemoteHostInfo::ContactInitiator_Name(contact_initiator_)},
      ",");
}

ExtensionSignalType RemoteHostContactedSignal::GetType() const {
  return ExtensionSignalType::kRemoteHostContacted;
}

}  // namespace safe_browsing

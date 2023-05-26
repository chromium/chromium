// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/dlp/dlp_files_utils.h"

namespace policy {
namespace dlp {

::dlp::DlpComponent MapPolicyComponentToProto(
    data_controls::Component component) {
  switch (component) {
    case data_controls::Component::kUnknownComponent:
      return ::dlp::DlpComponent::UNKNOWN_COMPONENT;
    case data_controls::Component::kArc:
      return ::dlp::DlpComponent::ARC;
    case data_controls::Component::kCrostini:
      return ::dlp::DlpComponent::CROSTINI;
    case data_controls::Component::kPluginVm:
      return ::dlp::DlpComponent::PLUGIN_VM;
    case data_controls::Component::kUsb:
      return ::dlp::DlpComponent::USB;
    case data_controls::Component::kDrive:
      return ::dlp::DlpComponent::GOOGLE_DRIVE;
    case data_controls::Component::kOneDrive:
      return ::dlp::DlpComponent::MICROSOFT_ONEDRIVE;
  }
}

}  // namespace dlp
}  // namespace policy

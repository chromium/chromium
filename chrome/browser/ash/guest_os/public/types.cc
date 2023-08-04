// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/guest_os/public/types.h"

#include "chromeos/ash/components/dbus/vm_concierge/concierge_service.pb.h"

namespace guest_os {

VmType ToVmType(vm_tools::concierge::VmInfo::VmType type) {
  switch (type) {
    case vm_tools::concierge::VmInfo_VmType_TERMINA:
      return VmType::TERMINA;
    case vm_tools::concierge::VmInfo_VmType_ARC_VM:
      return VmType::ARCVM;
    case vm_tools::concierge::VmInfo_VmType_PLUGIN_VM:
      return VmType::PLUGIN_VM;
    case vm_tools::concierge::VmInfo_VmType_BOREALIS:
      return VmType::BOREALIS;
    case vm_tools::concierge::VmInfo_VmType_BRUSCHETTA:
      return VmType::BRUSCHETTA;

    case vm_tools::concierge::VmInfo_VmType_UNKNOWN:
    // "default" is necessary because proto enums compile with (e.g.)
    // *_INT_MIN_SENTINEL_DO_NOT_USE_ which we aren't supposed to use.
    default:
      // Note to uprever:
      //  - Add new cases here.
      //  - Make whoever added a type to concierge without adding one to
      //    the vm_apps api put a dollar in the tech-debt jar...
      return VmType::UNKNOWN;
  }
}

apps::AppType ToAppType(vm_tools::apps::VmType type) {
  switch (type) {
    case vm_tools::apps::TERMINA:
      return apps::AppType::kCrostini;
    case vm_tools::apps::PLUGIN_VM:
      return apps::AppType::kPluginVm;
    case vm_tools::apps::BOREALIS:
      return apps::AppType::kBorealis;
    case vm_tools::apps::BRUSCHETTA:
      return apps::AppType::kBruschetta;
    case vm_tools::apps::ARCVM:
      return apps::AppType::kArc;
    // "default" is necessary because proto enums compile with (e.g.)
    // *_INT_MIN_SENTINEL_DO_NOT_USE_ which we aren't supposed to use.
    default:
      return apps::AppType::kUnknown;
  }
}

}  // namespace guest_os

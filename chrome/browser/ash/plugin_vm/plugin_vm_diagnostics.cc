// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/plugin_vm/plugin_vm_diagnostics.h"

#include <utility>

#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/system/sys_info.h"
#include "base/values.h"
#include "chrome/browser/ash/guest_os/guest_os_diagnostics.mojom.h"
#include "chrome/browser/ash/guest_os/guest_os_diagnostics_builder.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_features.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_pref_names.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_util.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chromeos/dbus//concierge_client.h"
#include "chromeos/dbus/concierge/concierge_service.pb.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "components/prefs/pref_service.h"
#include "third_party/protobuf/src/google/protobuf/repeated_field.h"

namespace plugin_vm {

namespace {

using DiagnosticsCallback =
    base::OnceCallback<void(guest_os::mojom::DiagnosticsPtr)>;

std::string CapitalizedBoardName() {
  const std::string uppercase = base::SysInfo::HardwareModelName();

  CHECK_GE(uppercase.size(), 1);
  base::StringPiece uppercase_first_char(uppercase.c_str(), 1);
  base::StringPiece uppercase_remaining(uppercase.c_str() + 1,
                                        uppercase.length() - 1);

  return base::StrCat(
      {uppercase_first_char, base::ToLowerASCII(uppercase_remaining)});
}

class PluginVmDiagnostics : public base::RefCounted<PluginVmDiagnostics> {
 public:
  static void Run(DiagnosticsCallback callback) {
    // Kick off the first step. The object is kept alive in callbacks until it
    // is finished.
    base::WrapRefCounted(new PluginVmDiagnostics(std::move(callback)))
        ->CheckPluginVmIsAllowed();
  }

 private:
  friend class base::RefCounted<PluginVmDiagnostics>;

  using EntryBuilder = guest_os::DiagnosticsBuilder::EntryBuilder;
  using ImageListType =
      ::google::protobuf::RepeatedPtrField<vm_tools::concierge::VmDiskInfo>;

  explicit PluginVmDiagnostics(DiagnosticsCallback callback)
      : active_profile_{ProfileManager::GetActiveUserProfile()},
        callback_(std::move(callback)) {}
  ~PluginVmDiagnostics() { DCHECK(callback_.is_null()); }

  void CheckPluginVmIsAllowed() {
    using ProfileSupported = plugin_vm::PluginVmFeatures::ProfileSupported;
    using PolicyConfigured = plugin_vm::PluginVmFeatures::PolicyConfigured;

    auto is_allowed_diagnostics =
        plugin_vm::PluginVmFeatures::Get()->GetIsAllowedDiagnostics(
            active_profile_);

    // TODO(b/173653141): Consider reconciling the error messages with
    // `is_allowed_diagnostics.GetTopError()` so that we can reuse it.

    {
      EntryBuilder entry("Device is supported");
      if (!is_allowed_diagnostics.device_supported) {
        entry.SetFail(
            base::StrCat({CapitalizedBoardName(), " is not supported"}));
      }
      builder_.AddEntry(std::move(entry));
    }

    {
      EntryBuilder entry("Profile is supported");

      switch (is_allowed_diagnostics.profile_supported) {
        case ProfileSupported::kOk:
          break;
        case ProfileSupported::kErrorNonPrimary:
          entry.SetFail("Secondary profiles are not supported");
          break;
        case ProfileSupported::kErrorChildAccount:
          entry.SetFail("Child accounts are not supported");
          break;
        case ProfileSupported::kErrorOffTheRecord:
          entry.SetFail("Guest profiles are not supported");
          break;
        case ProfileSupported::kErrorEphemeral:
          entry.SetFail(
              "Ephemeral user profiles are not supported. Contact your admin");
          break;
        case ProfileSupported::kErrorNotSupported:
          entry.SetFail("The profile is not supported");
          break;
      }
      builder_.AddEntry(std::move(entry));
    }

    {
      EntryBuilder entry("Policies are configured correctly");
      const std::string standard_top_error =
          "One or more policies are not configured correctly. Please contact "
          "your administrator";
      switch (is_allowed_diagnostics.policy_configured) {
        case PolicyConfigured::kOk: {
          // Additional check for image policy. See b/185281662#comment2.
          const base::DictionaryValue* image_policy =
              active_profile_->GetPrefs()->GetDictionary(prefs::kPluginVmImage);
          const base::Value* url =
              image_policy->FindKey(prefs::kPluginVmImageUrlKeyName);
          const base::Value* hash =
              image_policy->FindKey(prefs::kPluginVmImageHashKeyName);
          if (!url || !GURL(url->GetString()).is_valid()) {
            entry.SetFail("Image url is invalid", standard_top_error);
          } else if (!hash || hash->GetString().empty()) {
            entry.SetFail("Image hash is not set", standard_top_error);
          }
        } break;
        case PolicyConfigured::kErrorUnableToCheckPolicy:
          entry.SetFail("Unable to check policies", standard_top_error);
          break;
        case PolicyConfigured::kErrorNotEnterpriseEnrolled:
          entry.SetFail("Device is not enrolled", /*top_error_message=*/
                        "You must be on an enterprise-enrolled device");
          break;
        case PolicyConfigured::kErrorUserNotAffiliated:
          entry.SetFail("User is not affiliated with domain",
                        standard_top_error);
          break;
        case PolicyConfigured::kErrorUnableToCheckDevicePolicy:
          entry.SetFail("Unable to check whether device is allowed",
                        standard_top_error);
          break;
        case PolicyConfigured::kErrorNotAllowedByDevicePolicy:
          entry.SetFail("Device is not allowed", standard_top_error);
          break;
        case PolicyConfigured::kErrorNotAllowedByUserPolicy:
          entry.SetFail("User is not allowed", standard_top_error);
          break;
        case PolicyConfigured::kErrorLicenseNotSetUp:
          entry.SetFail("License is not set up", standard_top_error);
          break;
      }
      builder_.AddEntry(std::move(entry));
    }

    // Next step.
    CheckDefaultVmExists(is_allowed_diagnostics.IsOk());
  }

  void CheckDefaultVmExists(bool plugin_vm_is_allowed) {
    if (!plugin_vm_is_allowed) {
      OnListVmDisks(false, base::nullopt);
      return;
    }

    vm_tools::concierge::ListVmDisksRequest request;
    request.set_cryptohome_id(
        chromeos::ProfileHelper::GetUserIdHashFromProfile(active_profile_));
    request.set_storage_location(
        vm_tools::concierge::STORAGE_CRYPTOHOME_PLUGINVM);

    chromeos::DBusThreadManager::Get()->GetConciergeClient()->ListVmDisks(
        std::move(request),
        base::BindOnce(&PluginVmDiagnostics::OnListVmDisks, this,
                       /*plugin_vm_is_allowed=*/true));
  }

  void OnListVmDisks(
      bool plugin_vm_is_allowed,
      base::Optional<vm_tools::concierge::ListVmDisksResponse> response) {
    EntryBuilder entry(
        base::StrCat({"VM \"", plugin_vm::kPluginVmName, "\" exists"}));

    if (plugin_vm_is_allowed) {
      if (!response.has_value()) {
        entry.SetFail("Failed to check VMs");
      } else if (!HasDefaultVm(response->images())) {
        entry.SetFail(GetMissingDefaultVmExplanation(response->images()),
                      /*top_error_message=*/
                      "A required virtual machine does not exist. Please try "
                      "setting up Parallels to continue.");
      } else {
        // Everything is good. Do nothing.
      }
    } else {  // !plugin_vm_is_allowed.
      entry.SetNotApplicable();
    }

    builder_.AddEntry(std::move(entry));

    Finish();
  }

  void Finish() {
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback_), builder_.Build()));
  }

  bool HasDefaultVm(const ImageListType& images) {
    for (auto& image : images) {
      if (image.name() == plugin_vm::kPluginVmName) {
        return true;
      }
    }
    return false;
  }

  std::string GetMissingDefaultVmExplanation(const ImageListType& images) {
    if (images.empty()) {
      return "No Parallels Desktop VMs found";
    }

    std::stringstream stream;
    // The string looks like this:
    //
    // n Parallels Desktop VM(s) found: "vm1", "vm2"
    stream << images.size() << " Parallels Desktop VM"
           << (images.size() >= 2 ? "s" : "") << " found: ";
    bool first_vm = true;
    for (auto& image : images) {
      if (!first_vm) {
        stream << ", ";
      }
      stream << '"' << image.name() << '"';

      first_vm = false;
    }

    return stream.str();
  }

  Profile* const active_profile_;
  DiagnosticsCallback callback_;
  guest_os::DiagnosticsBuilder builder_;
};

}  // namespace

void GetDiagnostics(
    base::OnceCallback<void(guest_os::mojom::DiagnosticsPtr)> callback) {
  PluginVmDiagnostics::Run(std::move(callback));
}

}  // namespace plugin_vm

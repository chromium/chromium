// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/dbus/vm/vm_permission_service_provider.h"

#include <memory>
#include <utility>
#include <vector>

#include "ash/constants/ash_features.h"
#include "base/containers/adapters.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/ash/borealis/borealis_prefs.h"
#include "chrome/browser/ash/bruschetta/bruschetta_pref_names.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_pref_names.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/pref_names.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "chromeos/ash/components/dbus/vm_permission_service/vm_permission_service.pb.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user_manager.h"
#include "dbus/message.h"
#include "media/capture/video/chromeos/camera_hal_dispatcher_impl.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace {

base::UnguessableToken TokenFromString(const std::string& str) {
  static constexpr int kBytesPerUint64 = sizeof(uint64_t) / sizeof(uint8_t);
  static constexpr int kMaxBytesPerToken = 2 * kBytesPerUint64;

  std::vector<uint8_t> bytes;
  if (!base::HexStringToBytes(str, &bytes) || bytes.size() == 0 ||
      bytes.size() > kMaxBytesPerToken) {
    return base::UnguessableToken();
  }

  uint64_t high = 0, low = 0;
  int count = 0;
  base::ranges::for_each(base::Reversed(bytes), [&](auto byte) {
    auto* p = count < kBytesPerUint64 ? &low : &high;
    int pos = count < kBytesPerUint64 ? count : count - kBytesPerUint64;
    *p += static_cast<uint64_t>(byte) << (pos * 8);
    count++;
  });

  std::optional<base::UnguessableToken> token =
      base::UnguessableToken::Deserialize(high, low);
  if (!token.has_value()) {
    return base::UnguessableToken();
  }
  return token.value();
}

}  // namespace

namespace ash {

VmPermissionServiceProvider::VmInfo::VmInfo(std::string vm_owner_id,
                                            std::string vm_name,
                                            VmType vm_type)
    : owner_id(std::move(vm_owner_id)),
      name(std::move(vm_name)),
      type(vm_type) {}

VmPermissionServiceProvider::VmInfo::~VmInfo() = default;

VmPermissionServiceProvider::VmPermissionServiceProvider() {}

VmPermissionServiceProvider::~VmPermissionServiceProvider() = default;

VmPermissionServiceProvider::VmMap::iterator
VmPermissionServiceProvider::FindVm(const std::string& owner_id,
                                    const std::string& name) {
  return base::ranges::find_if(vms_, [&](const auto& vm) {
    return vm.second->owner_id == owner_id && vm.second->name == name;
  });
}

void VmPermissionServiceProvider::Start(
    scoped_refptr<dbus::ExportedObject> exported_object) {
  exported_object->ExportMethod(
      chromeos::kVmPermissionServiceInterface,
      chromeos::kVmPermissionServiceRegisterVmMethod,
      base::BindRepeating(&VmPermissionServiceProvider::RegisterVm,
                          weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&VmPermissionServiceProvider::OnExported,
                     weak_ptr_factory_.GetWeakPtr()));
  exported_object->ExportMethod(
      chromeos::kVmPermissionServiceInterface,
      chromeos::kVmPermissionServiceUnregisterVmMethod,
      base::BindRepeating(&VmPermissionServiceProvider::UnregisterVm,
                          weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&VmPermissionServiceProvider::OnExported,
                     weak_ptr_factory_.GetWeakPtr()));
  exported_object->ExportMethod(
      chromeos::kVmPermissionServiceInterface,
      chromeos::kVmPermissionServiceGetPermissionsMethod,
      base::BindRepeating(&VmPermissionServiceProvider::GetPermissions,
                          weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&VmPermissionServiceProvider::OnExported,
                     weak_ptr_factory_.GetWeakPtr()));
  exported_object->ExportMethod(
      chromeos::kVmPermissionServiceInterface,
      chromeos::kVmPermissionServiceSetPermissionsMethod,
      base::BindRepeating(&VmPermissionServiceProvider::SetPermissions,
                          weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&VmPermissionServiceProvider::OnExported,
                     weak_ptr_factory_.GetWeakPtr()));
}

void VmPermissionServiceProvider::OnExported(const std::string& interface_name,
                                             const std::string& method_name,
                                             bool success) {
  if (!success)
    LOG(ERROR) << "Failed to export " << interface_name << "." << method_name;
}

void VmPermissionServiceProvider::RegisterVm(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  std::unique_ptr<dbus::Response> response =
      dbus::Response::FromMethodCall(method_call);

  dbus::MessageReader reader(method_call);
  vm_permission_service::RegisterVmRequest request;
  if (!reader.PopArrayOfBytesAsProto(&request)) {
    constexpr char error_message[] =
        "Unable to parse RegisterVmRequest from message";
    LOG(ERROR) << error_message;
    std::move(response_sender)
        .Run(dbus::ErrorResponse::FromMethodCall(
            method_call, DBUS_ERROR_INVALID_ARGS, error_message));
    return;
  }

  if (FindVm(request.owner_id(), request.name()) != vms_.end()) {
    LOG(ERROR) << "VM " << request.owner_id() << "/" << request.name()
               << " is already registered with permission service";
    std::move(response_sender)
        .Run(dbus::ErrorResponse::FromMethodCall(
            method_call, DBUS_ERROR_INVALID_ARGS, "VM is already registered"));
    return;
  }

  VmInfo::VmType vm_type;
  if (request.type() == vm_permission_service::RegisterVmRequest::PLUGIN_VM) {
    vm_type = VmInfo::VmType::PluginVm;
  } else if (request.type() ==
             vm_permission_service::RegisterVmRequest::BOREALIS) {
    vm_type = VmInfo::VmType::Borealis;
  } else if (request.type() ==
             vm_permission_service::RegisterVmRequest::BRUSCHETTA) {
    vm_type = VmInfo::VmType::Bruschetta;
  } else {
    LOG(ERROR) << "Unsupported VM " << request.owner_id() << "/"
               << request.name() << " type: " << request.type();
    std::move(response_sender)
        .Run(dbus::ErrorResponse::FromMethodCall(
            method_call, DBUS_ERROR_INVALID_ARGS, "Unsupported VM type"));
    return;
  }

  auto vm =
      base::WrapUnique(new VmInfo(request.owner_id(), request.name(), vm_type));

  // Seed the initial set of permission. Because in the initial release we
  // only support static permissions, i.e. for changes to take effect we need
  // to re-launch the VM, we do not need to update them after this.
  UpdateVmPermissions(vm.get());

  const base::UnguessableToken token(base::UnguessableToken::Create());

  SetCameraPermission(token,
                      vm->permission_to_enabled_map[VmInfo::PermissionCamera]);

  vms_[token] = std::move(vm);

  vm_permission_service::RegisterVmResponse payload;
  payload.set_token(token.ToString());

  dbus::MessageWriter writer(response.get());
  writer.AppendProtoAsArrayOfBytes(payload);
  std::move(response_sender).Run(std::move(response));
}

void VmPermissionServiceProvider::UnregisterVm(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  std::unique_ptr<dbus::Response> response =
      dbus::Response::FromMethodCall(method_call);

  dbus::MessageReader reader(method_call);
  vm_permission_service::UnregisterVmRequest request;
  if (!reader.PopArrayOfBytesAsProto(&request)) {
    constexpr char error_message[] =
        "Unable to parse RegisterVmRequest from message";
    LOG(ERROR) << error_message;
    std::move(response_sender)
        .Run(dbus::ErrorResponse::FromMethodCall(
            method_call, DBUS_ERROR_INVALID_ARGS, error_message));
    return;
  }

  auto iter = FindVm(request.owner_id(), request.name());
  if (iter == vms_.end()) {
    LOG(ERROR) << "VM " << request.owner_id() << "/" << request.name()
               << " is not registered with permission service";
    std::move(response_sender)
        .Run(dbus::ErrorResponse::FromMethodCall(
            method_call, DBUS_ERROR_INVALID_ARGS, "VM is not registered"));
    return;
  }

  SetCameraPermission(iter->first, false);

  vms_.erase(iter);

  std::move(response_sender).Run(std::move(response));
}

void VmPermissionServiceProvider::SetPermissions(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  std::unique_ptr<dbus::Response> response =
      dbus::Response::FromMethodCall(method_call);

  dbus::MessageReader reader(method_call);
  vm_permission_service::SetPermissionsRequest request;
  if (!reader.PopArrayOfBytesAsProto(&request)) {
    constexpr char error_message[] =
        "Unable to parse SetPermissionsRequest from message";
    LOG(ERROR) << error_message;
    std::move(response_sender)
        .Run(dbus::ErrorResponse::FromMethodCall(
            method_call, DBUS_ERROR_INVALID_ARGS, error_message));
    return;
  }

  auto iter = FindVm(request.owner_id(), request.name());
  if (iter == vms_.end()) {
    LOG(ERROR) << "VM " << request.owner_id() << "/" << request.name()
               << " is not registered with permission service";
    std::move(response_sender)
        .Run(dbus::ErrorResponse::FromMethodCall(
            method_call, DBUS_ERROR_INVALID_ARGS, "VM is not registered"));
    return;
  }

  base::flat_map<VmInfo::PermissionType, bool> new_permissions(
      iter->second->permission_to_enabled_map);
  for (const auto& p : request.permissions()) {
    VmInfo::PermissionType kind;
    if (p.kind() == vm_permission_service::Permission::CAMERA) {
      kind = VmInfo::PermissionCamera;
    } else if (p.kind() == vm_permission_service::Permission::MICROPHONE) {
      kind = VmInfo::PermissionMicrophone;
    } else {
      constexpr char error_message[] = "Unknown permission type";
      LOG(ERROR) << error_message;
      std::move(response_sender)
          .Run(dbus::ErrorResponse::FromMethodCall(
              method_call, DBUS_ERROR_INVALID_ARGS, error_message));
      return;
    }

    new_permissions[kind] = p.allowed();
  }

  // Commit final version of permissions.
  iter->second->permission_to_enabled_map = std::move(new_permissions);
  SetCameraPermission(
      iter->first,
      iter->second->permission_to_enabled_map[VmInfo::PermissionCamera]);

  std::move(response_sender).Run(std::move(response));
}

void VmPermissionServiceProvider::GetPermissions(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  std::unique_ptr<dbus::Response> response =
      dbus::Response::FromMethodCall(method_call);

  dbus::MessageReader reader(method_call);
  vm_permission_service::GetPermissionsRequest request;
  if (!reader.PopArrayOfBytesAsProto(&request)) {
    constexpr char error_message[] =
        "Unable to parse GetPermissionsRequest from message";
    LOG(ERROR) << error_message;
    std::move(response_sender)
        .Run(dbus::ErrorResponse::FromMethodCall(
            method_call, DBUS_ERROR_INVALID_ARGS, error_message));
    return;
  }

  auto token = TokenFromString(request.token());
  if (!token) {
    LOG(ERROR) << "Malformed token '" << request.token() << "'";
    std::move(response_sender)
        .Run(dbus::ErrorResponse::FromMethodCall(
            method_call, DBUS_ERROR_INVALID_ARGS, "Malformed token"));
    return;
  }

  auto iter = vms_.find(token);
  if (iter == vms_.end()) {
    LOG(ERROR) << "Invalid token " << token;
    std::move(response_sender)
        .Run(dbus::ErrorResponse::FromMethodCall(
            method_call, DBUS_ERROR_INVALID_ARGS, "Invalid token"));
    return;
  }

  vm_permission_service::GetPermissionsResponse payload;
  for (auto permission : iter->second->permission_to_enabled_map) {
    auto* p = payload.add_permissions();
    switch (permission.first) {
      case VmInfo::PermissionCamera:
        p->set_kind(vm_permission_service::Permission::CAMERA);
        break;
      case VmInfo::PermissionMicrophone:
        p->set_kind(vm_permission_service::Permission::MICROPHONE);
        break;
    }
    p->set_allowed(permission.second);
  }

  dbus::MessageWriter writer(response.get());
  writer.AppendProtoAsArrayOfBytes(payload);
  std::move(response_sender).Run(std::move(response));
}

void VmPermissionServiceProvider::UpdateVmPermissions(VmInfo* vm) {
  vm->permission_to_enabled_map.clear();
  switch (vm->type) {
    case VmInfo::PluginVm:
      UpdatePluginVmPermissions(vm);
      break;
    case VmInfo::Borealis:
      UpdateBorealisPermissions(vm);
      break;
    case VmInfo::Bruschetta:
      UpdateBruschettaPermissions(vm);
      break;
    case VmInfo::CrostiniVm:
      NOTREACHED_IN_MIGRATION();
  }
}

void VmPermissionServiceProvider::UpdatePluginVmPermissions(VmInfo* vm) {
  Profile* profile = ProfileManager::GetPrimaryUserProfile();
  if (!profile ||
      ProfileHelper::GetUserIdHashFromProfile(profile) != vm->owner_id) {
    return;
  }

  const PrefService* prefs = profile->GetPrefs();
  if (prefs->GetBoolean(prefs::kVideoCaptureAllowed)) {
    vm->permission_to_enabled_map[VmInfo::PermissionCamera] =
        prefs->GetBoolean(plugin_vm::prefs::kPluginVmCameraAllowed);
  }

  if (prefs->GetBoolean(prefs::kAudioCaptureAllowed)) {
    vm->permission_to_enabled_map[VmInfo::PermissionMicrophone] =
        prefs->GetBoolean(plugin_vm::prefs::kPluginVmMicAllowed);
  }
}

void VmPermissionServiceProvider::UpdateBorealisPermissions(VmInfo* vm) {
  Profile* profile = ProfileManager::GetPrimaryUserProfile();
  if (!profile ||
      ProfileHelper::GetUserIdHashFromProfile(profile) != vm->owner_id) {
    return;
  }

  const PrefService* prefs = profile->GetPrefs();
  if (prefs->GetBoolean(prefs::kAudioCaptureAllowed)) {
    vm->permission_to_enabled_map[VmInfo::PermissionMicrophone] =
        prefs->GetBoolean(borealis::prefs::kBorealisMicAllowed);
  }
}

void VmPermissionServiceProvider::UpdateBruschettaPermissions(VmInfo* vm) {
  Profile* profile = Profile::FromBrowserContext(
      BrowserContextHelper::Get()->GetBrowserContextByUser(
          user_manager::UserManager::Get()->GetPrimaryUser()));

  if (!profile ||
      ProfileHelper::GetUserIdHashFromProfile(profile) != vm->owner_id) {
    return;
  }

  const PrefService* prefs = profile->GetPrefs();
  if (prefs->GetBoolean(prefs::kAudioCaptureAllowed)) {
    vm->permission_to_enabled_map[VmInfo::PermissionMicrophone] =
        prefs->GetBoolean(bruschetta::prefs::kBruschettaMicAllowed);
  }
}

void VmPermissionServiceProvider::SetCameraPermission(
    base::UnguessableToken token,
    bool enabled) {
  if (enabled) {
    media::CameraHalDispatcherImpl::GetInstance()->RegisterPluginVmToken(token);
  } else {
    media::CameraHalDispatcherImpl::GetInstance()->UnregisterPluginVmToken(
        token);
  }
}

}  // namespace ash

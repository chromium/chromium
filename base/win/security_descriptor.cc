// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/win/security_descriptor.h"

// clang-format off
#include <windows.h>  // Must be in front of other Windows header files.
// clang-format on

#include <aclapi.h>
#include <sddl.h>

#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/numerics/checked_math.h"
#include "base/win/scoped_localalloc.h"

namespace base::win {

namespace {
template <typename T>
std::optional<T> CloneValue(const std::optional<T>& value) {
  if (!value)
    return std::nullopt;
  return value->Clone();
}

PSID UnwrapSid(const std::optional<Sid>& sid) {
  if (!sid)
    return nullptr;
  return sid->GetPSID();
}

PACL UnwrapAcl(const std::optional<AccessControlList>& acl) {
  if (!acl)
    return nullptr;
  return acl->get();
}

SE_OBJECT_TYPE ConvertObjectType(SecurityObjectType object_type) {
  switch (object_type) {
    case SecurityObjectType::kFile:
      return SE_FILE_OBJECT;
    case SecurityObjectType::kRegistry:
      return SE_REGISTRY_KEY;
    case SecurityObjectType::kWindowStation:
    case SecurityObjectType::kDesktop:
      return SE_WINDOW_OBJECT;
    case SecurityObjectType::kKernel:
      return SE_KERNEL_OBJECT;
  }
  return SE_UNKNOWN_OBJECT_TYPE;
}

GENERIC_MAPPING GetGenericMappingForType(SecurityObjectType object_type) {
  GENERIC_MAPPING generic_mapping = {};
  switch (object_type) {
    case SecurityObjectType::kFile:
      generic_mapping.GenericRead = FILE_GENERIC_READ;
      generic_mapping.GenericWrite = FILE_GENERIC_WRITE;
      generic_mapping.GenericExecute = FILE_GENERIC_EXECUTE;
      generic_mapping.GenericAll = FILE_ALL_ACCESS;
      break;
    case SecurityObjectType::kRegistry:
      generic_mapping.GenericRead = KEY_READ;
      generic_mapping.GenericWrite = KEY_WRITE;
      generic_mapping.GenericExecute = KEY_EXECUTE;
      generic_mapping.GenericAll = KEY_ALL_ACCESS;
      break;
    case SecurityObjectType::kDesktop:
      generic_mapping.GenericRead =
          STANDARD_RIGHTS_READ | DESKTOP_READOBJECTS | DESKTOP_ENUMERATE;
      generic_mapping.GenericWrite =
          STANDARD_RIGHTS_WRITE | DESKTOP_CREATEWINDOW | DESKTOP_CREATEMENU |
          DESKTOP_HOOKCONTROL | DESKTOP_JOURNALRECORD |
          DESKTOP_JOURNALPLAYBACK | DESKTOP_WRITEOBJECTS;
      generic_mapping.GenericExecute =
          STANDARD_RIGHTS_EXECUTE | DESKTOP_SWITCHDESKTOP;
      generic_mapping.GenericAll =
          STANDARD_RIGHTS_REQUIRED | DESKTOP_CREATEMENU | DESKTOP_CREATEWINDOW |
          DESKTOP_ENUMERATE | DESKTOP_HOOKCONTROL | DESKTOP_JOURNALPLAYBACK |
          DESKTOP_JOURNALRECORD | DESKTOP_READOBJECTS | DESKTOP_SWITCHDESKTOP |
          DESKTOP_WRITEOBJECTS;
      break;
    case SecurityObjectType::kWindowStation:
      generic_mapping.GenericRead = STANDARD_RIGHTS_READ | WINSTA_ENUMDESKTOPS |
                                    WINSTA_ENUMERATE | WINSTA_READATTRIBUTES |
                                    WINSTA_READSCREEN;
      generic_mapping.GenericWrite =
          STANDARD_RIGHTS_WRITE | WINSTA_ACCESSCLIPBOARD |
          WINSTA_CREATEDESKTOP | WINSTA_WRITEATTRIBUTES;
      generic_mapping.GenericExecute = STANDARD_RIGHTS_EXECUTE |
                                       WINSTA_ACCESSGLOBALATOMS |
                                       WINSTA_EXITWINDOWS;
      generic_mapping.GenericAll =
          STANDARD_RIGHTS_REQUIRED | WINSTA_ACCESSCLIPBOARD |
          WINSTA_ACCESSGLOBALATOMS | WINSTA_CREATEDESKTOP |
          WINSTA_ENUMDESKTOPS | WINSTA_ENUMERATE | WINSTA_EXITWINDOWS |
          WINSTA_READATTRIBUTES | WINSTA_READSCREEN | WINSTA_WRITEATTRIBUTES;
      break;
    case SecurityObjectType::kKernel:
      NOTREACHED();
  }
  return generic_mapping;
}

template <typename T>
std::optional<SecurityDescriptor> GetSecurityDescriptor(
    T object,
    SecurityObjectType object_type,
    SECURITY_INFORMATION security_info,
    DWORD(WINAPI* get_sd)(T,
                          SE_OBJECT_TYPE,
                          SECURITY_INFORMATION,
                          PSID*,
                          PSID*,
                          PACL*,
                          PACL*,
                          PSECURITY_DESCRIPTOR*)) {
  PSECURITY_DESCRIPTOR sd = nullptr;

  DWORD error = get_sd(object, ConvertObjectType(object_type), security_info,
                       nullptr, nullptr, nullptr, nullptr, &sd);
  if (error != ERROR_SUCCESS) {
    ::SetLastError(error);
    DPLOG(ERROR) << "Failed getting security descriptor for object.";
    return std::nullopt;
  }
  auto sd_ptr = TakeLocalAlloc(sd);
  return SecurityDescriptor::FromPointer(sd_ptr.get());
}

template <typename T>
bool SetSecurityDescriptor(const SecurityDescriptor& sd,
                           T object,
                           SecurityObjectType object_type,
                           SECURITY_INFORMATION security_info,
                           DWORD(WINAPI* set_sd)(T,
                                                 SE_OBJECT_TYPE,
                                                 SECURITY_INFORMATION,
                                                 PSID,
                                                 PSID,
                                                 PACL,
                                                 PACL)) {
  security_info &= ~(PROTECTED_DACL_SECURITY_INFORMATION |
                     UNPROTECTED_DACL_SECURITY_INFORMATION |
                     PROTECTED_SACL_SECURITY_INFORMATION |
                     UNPROTECTED_SACL_SECURITY_INFORMATION);
  if (security_info & DACL_SECURITY_INFORMATION) {
    if (sd.dacl_protected()) {
      security_info |= PROTECTED_DACL_SECURITY_INFORMATION;
    } else {
      security_info |= UNPROTECTED_DACL_SECURITY_INFORMATION;
    }
  }
  if (security_info & SACL_SECURITY_INFORMATION) {
    if (sd.sacl_protected()) {
      security_info |= PROTECTED_SACL_SECURITY_INFORMATION;
    } else {
      security_info |= UNPROTECTED_SACL_SECURITY_INFORMATION;
    }
  }
  DWORD error = set_sd(object, ConvertObjectType(object_type), security_info,
                       UnwrapSid(sd.owner()), UnwrapSid(sd.group()),
                       UnwrapAcl(sd.dacl()), UnwrapAcl(sd.sacl()));
  if (error != ERROR_SUCCESS) {
    ::SetLastError(error);
    DPLOG(ERROR) << "Failed setting DACL for object.";
    return false;
  }
  return true;
}

std::optional<Sid> GetSecurityDescriptorSid(
    PSECURITY_DESCRIPTOR sd,
    BOOL(WINAPI* get_sid)(PSECURITY_DESCRIPTOR, PSID*, LPBOOL)) {
  PSID sid;
  BOOL defaulted;
  if (!get_sid(sd, &sid, &defaulted) || !sid) {
    return std::nullopt;
  }
  return Sid::FromPSID(sid);
}

std::optional<AccessControlList> GetSecurityDescriptorAcl(
    PSECURITY_DESCRIPTOR sd,
    BOOL(WINAPI* get_acl)(PSECURITY_DESCRIPTOR, LPBOOL, PACL*, LPBOOL)) {
  PACL acl;
  BOOL present;
  BOOL defaulted;
  if (!get_acl(sd, &present, &acl, &defaulted) || !present) {
    return std::nullopt;
  }
  return AccessControlList::FromPACL(acl);
}

}  // namespace

SecurityDescriptor::SelfRelative::SelfRelative(const SelfRelative&) = default;
SecurityDescriptor::SelfRelative::~SelfRelative() = default;
SecurityDescriptor::SelfRelative::SelfRelative(std::vector<uint8_t>&& sd)
    : sd_(sd) {}

std::optional<SecurityDescriptor> SecurityDescriptor::FromPointer(
    PSECURITY_DESCRIPTOR sd) {
  if (!sd || !::IsValidSecurityDescriptor(sd)) {
    ::SetLastError(ERROR_INVALID_SECURITY_DESCR);
    return std::nullopt;
  }

  SECURITY_DESCRIPTOR_CONTROL control;
  DWORD revision;
  if (!::GetSecurityDescriptorControl(sd, &control, &revision)) {
    return std::nullopt;
  }

  return SecurityDescriptor{
      GetSecurityDescriptorSid(sd, ::GetSecurityDescriptorOwner),
      GetSecurityDescriptorSid(sd, ::GetSecurityDescriptorGroup),
      GetSecurityDescriptorAcl(sd, ::GetSecurityDescriptorDacl),
      !!(control & SE_DACL_PROTECTED),
      GetSecurityDescriptorAcl(sd, ::GetSecurityDescriptorSacl),
      !!(control & SE_SACL_PROTECTED)};
}

std::optional<SecurityDescriptor> SecurityDescriptor::FromFile(
    const base::FilePath& path,
    SECURITY_INFORMATION security_info) {
  return FromName(path.value(), SecurityObjectType::kFile, security_info);
}

std::optional<SecurityDescriptor> SecurityDescriptor::FromName(
    const std::wstring& name,
    SecurityObjectType object_type,
    SECURITY_INFORMATION security_info) {
  return GetSecurityDescriptor(name.c_str(), object_type, security_info,
                               ::GetNamedSecurityInfo);
}

std::optional<SecurityDescriptor> SecurityDescriptor::FromHandle(
    HANDLE handle,
    SecurityObjectType object_type,
    SECURITY_INFORMATION security_info) {
  return GetSecurityDescriptor<HANDLE>(handle, object_type, security_info,
                                       ::GetSecurityInfo);
}

std::optional<SecurityDescriptor> SecurityDescriptor::FromSddl(
    const std::wstring& sddl) {
  PSECURITY_DESCRIPTOR sd;
  if (!::ConvertStringSecurityDescriptorToSecurityDescriptor(
          sddl.c_str(), SDDL_REVISION_1, &sd, nullptr)) {
    return std::nullopt;
  }
  auto sd_ptr = TakeLocalAlloc(sd);
  return FromPointer(sd_ptr.get());
}

SecurityDescriptor::SecurityDescriptor() = default;
SecurityDescriptor::SecurityDescriptor(SecurityDescriptor&&) = default;
SecurityDescriptor& SecurityDescriptor::operator=(SecurityDescriptor&&) =
    default;
SecurityDescriptor::~SecurityDescriptor() = default;

bool SecurityDescriptor::WriteToFile(const base::FilePath& path,
                                     SECURITY_INFORMATION security_info) const {
  return WriteToName(path.value(), SecurityObjectType::kFile, security_info);
}

bool SecurityDescriptor::WriteToName(const std::wstring& name,
                                     SecurityObjectType object_type,
                                     SECURITY_INFORMATION security_info) const {
  return SetSecurityDescriptor<wchar_t*>(
      *this, const_cast<wchar_t*>(name.c_str()), object_type, security_info,
      ::SetNamedSecurityInfo);
}

bool SecurityDescriptor::WriteToHandle(
    HANDLE handle,
    SecurityObjectType object_type,
    SECURITY_INFORMATION security_info) const {
  return SetSecurityDescriptor<HANDLE>(*this, handle, object_type,
                                       security_info, ::SetSecurityInfo);
}

std::optional<std::wstring> SecurityDescriptor::ToSddl(
    SECURITY_INFORMATION security_info) const {
  SECURITY_DESCRIPTOR sd = {};
  ToAbsolute(sd);
  LPWSTR sddl;
  if (!::ConvertSecurityDescriptorToStringSecurityDescriptor(
          &sd, SDDL_REVISION_1, security_info, &sddl, nullptr)) {
    return std::nullopt;
  }
  auto sddl_ptr = TakeLocalAlloc(sddl);
  return sddl_ptr.get();
}

void SecurityDescriptor::ToAbsolute(SECURITY_DESCRIPTOR& sd) const {
  memset(&sd, 0, sizeof(sd));
  sd.Revision = SECURITY_DESCRIPTOR_REVISION;
  sd.Owner = owner_ ? owner_->GetPSID() : nullptr;
  sd.Group = group_ ? group_->GetPSID() : nullptr;
  if (dacl_) {
    sd.Dacl = dacl_->get();
    sd.Control |= SE_DACL_PRESENT;
    if (dacl_protected_) {
      sd.Control |= SE_DACL_PROTECTED;
    }
  }
  if (sacl_) {
    sd.Sacl = sacl_->get();
    sd.Control |= SE_SACL_PRESENT;
    if (sacl_protected_) {
      sd.Control |= SE_SACL_PROTECTED;
    }
  }
  DCHECK(::IsValidSecurityDescriptor(&sd));
}

std::optional<SecurityDescriptor::SelfRelative>
SecurityDescriptor::ToSelfRelative() const {
  SECURITY_DESCRIPTOR sd = {};
  ToAbsolute(sd);
  DWORD size = sizeof(SECURITY_DESCRIPTOR_MIN_LENGTH);
  std::vector<uint8_t> buffer(SECURITY_DESCRIPTOR_MIN_LENGTH);
  if (::MakeSelfRelativeSD(&sd, buffer.data(), &size)) {
    return SelfRelative(std::move(buffer));
  }

  if (::GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
    return std::nullopt;
  }

  buffer.resize(size);
  if (!::MakeSelfRelativeSD(&sd, buffer.data(), &size)) {
    return std::nullopt;
  }
  return SelfRelative(std::move(buffer));
}

SecurityDescriptor SecurityDescriptor::Clone() const {
  return SecurityDescriptor{CloneValue(owner_), CloneValue(group_),
                            CloneValue(dacl_),  dacl_protected_,
                            CloneValue(sacl_),  sacl_protected_};
}

bool SecurityDescriptor::SetMandatoryLabel(DWORD integrity_level,
                                           DWORD inheritance,
                                           DWORD mandatory_policy) {
  std::optional<AccessControlList> sacl = AccessControlList::FromMandatoryLabel(
      integrity_level, inheritance, mandatory_policy);
  if (!sacl) {
    return false;
  }
  sacl_ = std::move(*sacl);
  return true;
}

bool SecurityDescriptor::SetDaclEntries(
    const std::vector<ExplicitAccessEntry>& entries) {
  if (!dacl_) {
    dacl_ = AccessControlList{};
  }
  return dacl_->SetEntries(entries);
}

bool SecurityDescriptor::SetDaclEntry(const Sid& sid,
                                      SecurityAccessMode mode,
                                      DWORD access_mask,
                                      DWORD inheritance) {
  if (!dacl_) {
    dacl_ = AccessControlList{};
  }
  return dacl_->SetEntry(sid, mode, access_mask, inheritance);
}

bool SecurityDescriptor::SetDaclEntry(WellKnownSid known_sid,
                                      SecurityAccessMode mode,
                                      DWORD access_mask,
                                      DWORD inheritance) {
  return SetDaclEntry(Sid(known_sid), mode, access_mask, inheritance);
}

std::optional<AccessCheckResult> SecurityDescriptor::AccessCheck(
    const AccessToken& token,
    ACCESS_MASK desired_access,
    const GENERIC_MAPPING& generic_mapping) {
  GENERIC_MAPPING local_mapping = generic_mapping;
  ::MapGenericMask(&desired_access, &local_mapping);

  // Allocate a privilege set which could cover all possible privileges.
  DWORD priv_set_length = checked_cast<DWORD>(
      sizeof(PRIVILEGE_SET) +
      (token.Privileges().size() * sizeof(LUID_AND_ATTRIBUTES)));
  std::vector<char> priv_set(priv_set_length);
  DWORD granted_access = 0;
  BOOL access_status = FALSE;
  SECURITY_DESCRIPTOR sd = {};
  ToAbsolute(sd);
  if (!::AccessCheck(&sd, token.get(), desired_access, &local_mapping,
                     reinterpret_cast<PPRIVILEGE_SET>(priv_set.data()),
                     &priv_set_length, &granted_access, &access_status)) {
    return std::nullopt;
  }
  return AccessCheckResult{granted_access, !!access_status};
}

std::optional<AccessCheckResult> SecurityDescriptor::AccessCheck(
    const AccessToken& token,
    ACCESS_MASK desired_access,
    SecurityObjectType object_type) {
  if (object_type == SecurityObjectType::kKernel) {
    ::SetLastError(ERROR_INVALID_PARAMETER);
    return std::nullopt;
  }
  return AccessCheck(token, desired_access,
                     GetGenericMappingForType(object_type));
}

SecurityDescriptor::SecurityDescriptor(std::optional<Sid>&& owner,
                                       std::optional<Sid>&& group,
                                       std::optional<AccessControlList>&& dacl,
                                       bool dacl_protected,
                                       std::optional<AccessControlList>&& sacl,
                                       bool sacl_protected) {
  owner_.swap(owner);
  group_.swap(group);
  dacl_.swap(dacl);
  dacl_protected_ = dacl_protected;
  sacl_.swap(sacl);
  sacl_protected_ = sacl_protected;
}

}  // namespace base::win

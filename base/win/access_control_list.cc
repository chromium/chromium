// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/win/access_control_list.h"

#include <windows.h>

#include <aclapi.h>

#include <utility>
#include <vector>

#include "base/check.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/numerics/checked_math.h"
#include "base/win/scoped_localalloc.h"

namespace base::win {

namespace {

std::unique_ptr<uint8_t[]> AclToBuffer(const ACL* acl) {
  if (!acl) {
    return nullptr;
  }
  size_t size = acl->AclSize;
  DCHECK(size >= sizeof(*acl));
  std::unique_ptr<uint8_t[]> ptr = std::make_unique<uint8_t[]>(size);
  memcpy(ptr.get(), acl, size);
  return ptr;
}

std::unique_ptr<uint8_t[]> EmptyAclToBuffer() {
  ACL acl = {};
  acl.AclRevision = ACL_REVISION;
  acl.AclSize = static_cast<WORD>(sizeof(acl));
  return AclToBuffer(&acl);
}

ACCESS_MODE ConvertAccessMode(SecurityAccessMode access_mode) {
  switch (access_mode) {
    case SecurityAccessMode::kGrant:
      return GRANT_ACCESS;
    case SecurityAccessMode::kSet:
      return SET_ACCESS;
    case SecurityAccessMode::kDeny:
      return DENY_ACCESS;
    case SecurityAccessMode::kRevoke:
      return REVOKE_ACCESS;
  }
}

std::unique_ptr<uint8_t[]> AddACEToAcl(
    ACL* old_acl,
    const std::vector<ExplicitAccessEntry>& entries) {
  std::vector<EXPLICIT_ACCESS> access_entries(entries.size());
  auto entries_interator = access_entries.begin();
  for (const ExplicitAccessEntry& entry : entries) {
    EXPLICIT_ACCESS& new_access = *entries_interator++;
    new_access.grfAccessMode = ConvertAccessMode(entry.mode());
    new_access.grfAccessPermissions = entry.access_mask();
    new_access.grfInheritance = entry.inheritance();
    ::BuildTrusteeWithSid(&new_access.Trustee, entry.sid().GetPSID());
  }

  PACL new_acl = nullptr;
  DWORD error = ::SetEntriesInAcl(checked_cast<ULONG>(access_entries.size()),
                                  access_entries.data(), old_acl, &new_acl);
  if (error != ERROR_SUCCESS) {
    ::SetLastError(error);
    DPLOG(ERROR) << "Failed adding ACEs to ACL";
    return nullptr;
  }
  auto new_acl_ptr = TakeLocalAlloc(new_acl);
  return AclToBuffer(new_acl_ptr.get());
}

}  // namespace

ExplicitAccessEntry ExplicitAccessEntry::Clone() const {
  return ExplicitAccessEntry{sid_, mode_, access_mask_, inheritance_};
}

ExplicitAccessEntry::ExplicitAccessEntry(const Sid& sid,
                                         SecurityAccessMode mode,
                                         DWORD access_mask,
                                         DWORD inheritance)
    : sid_(sid.Clone()),
      mode_(mode),
      access_mask_(access_mask),
      inheritance_(inheritance) {}

ExplicitAccessEntry::ExplicitAccessEntry(WellKnownSid known_sid,
                                         SecurityAccessMode mode,
                                         DWORD access_mask,
                                         DWORD inheritance)
    : ExplicitAccessEntry(Sid(known_sid), mode, access_mask, inheritance) {}

ExplicitAccessEntry::ExplicitAccessEntry(ExplicitAccessEntry&&) = default;
ExplicitAccessEntry& ExplicitAccessEntry::operator=(ExplicitAccessEntry&&) =
    default;
ExplicitAccessEntry::~ExplicitAccessEntry() = default;

std::optional<AccessControlList> AccessControlList::FromPACL(ACL* acl) {
  if (acl && !::IsValidAcl(acl)) {
    ::SetLastError(ERROR_INVALID_ACL);
    return std::nullopt;
  }
  return AccessControlList{acl};
}

std::optional<AccessControlList> AccessControlList::FromMandatoryLabel(
    DWORD integrity_level,
    DWORD inheritance,
    DWORD mandatory_policy) {
  Sid sid = Sid::FromIntegrityLevel(integrity_level);
  // Get total ACL length. SYSTEM_MANDATORY_LABEL_ACE contains the first DWORD
  // of the SID so remove it from total.
  DWORD length = sizeof(ACL) + sizeof(SYSTEM_MANDATORY_LABEL_ACE) +
                 ::GetLengthSid(sid.GetPSID()) - sizeof(DWORD);
  std::unique_ptr<uint8_t[]> sacl_ptr = std::make_unique<uint8_t[]>(length);
  PACL sacl = reinterpret_cast<PACL>(sacl_ptr.get());

  if (!::InitializeAcl(sacl, length, ACL_REVISION)) {
    return std::nullopt;
  }

  if (!::AddMandatoryAce(sacl, ACL_REVISION, inheritance, mandatory_policy,
                         sid.GetPSID())) {
    return std::nullopt;
  }

  DCHECK(::IsValidAcl(sacl));
  AccessControlList ret;
  ret.acl_ = std::move(sacl_ptr);
  return ret;
}

AccessControlList::AccessControlList() : acl_(EmptyAclToBuffer()) {}
AccessControlList::AccessControlList(AccessControlList&&) = default;
AccessControlList& AccessControlList::operator=(AccessControlList&&) = default;
AccessControlList::~AccessControlList() = default;

bool AccessControlList::SetEntries(
    const std::vector<ExplicitAccessEntry>& entries) {
  if (entries.empty())
    return true;

  std::unique_ptr<uint8_t[]> acl = AddACEToAcl(get(), entries);
  if (!acl)
    return false;

  acl_ = std::move(acl);
  return true;
}

bool AccessControlList::SetEntry(const Sid& sid,
                                 SecurityAccessMode mode,
                                 DWORD access_mask,
                                 DWORD inheritance) {
  std::vector<ExplicitAccessEntry> ace_list;
  ace_list.emplace_back(sid, mode, access_mask, inheritance);
  return SetEntries(ace_list);
}

AccessControlList AccessControlList::Clone() const {
  return AccessControlList{get()};
}

void AccessControlList::Clear() {
  acl_ = EmptyAclToBuffer();
}

AccessControlList::AccessControlList(const ACL* acl) : acl_(AclToBuffer(acl)) {}

}  // namespace base::win

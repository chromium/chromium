// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/user_type_filter.h"

#include "base/logging.h"
#include "base/values.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/policy/profile_policy_connector.h"  // nogncheck crbug.com/1420759
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_thread.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chromeos/components/mgs/managed_guest_session_utils.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace apps {

// kUserType is required key that specifies enumeration of user types for which
// web app is visible. See kUserType* constants
const char kKeyUserType[] = "user_type";

const char kUserTypeChild[] = "child";
const char kUserTypeGuest[] = "guest";
const char kUserTypeManaged[] = "managed";
const char kUserTypeManagedGuest[] = "managed_guest";
const char kUserTypeUnmanaged[] = "unmanaged";

std::string DetermineUserType(Profile* profile) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (profile->IsGuestSession())
    return kUserTypeGuest;
  DCHECK(!profile->IsOffTheRecord());
  if (profile->IsChild())
    return kUserTypeChild;
  if (profile->GetProfilePolicyConnector()->IsManaged()) {
#if BUILDFLAG(IS_CHROMEOS)
    if (chromeos::IsManagedGuestSession()) {
      return kUserTypeManagedGuest;
    }
#endif  // BUILDFLAG(IS_CHROMEOS)
    return kUserTypeManaged;
  }
  return kUserTypeUnmanaged;
}

bool UserTypeMatchesJsonUserType(const std::string& user_type,
                                 const std::string& app_id,
                                 const base::Value::Dict& json_root,
                                 const base::Value::List* default_user_types) {
  const base::Value::List* value = json_root.FindList(kKeyUserType);
  if (!value) {
    if (!default_user_types) {
      LOG(ERROR) << "Json has no user type filter for " << app_id << ".";
      return false;
    }
    value = default_user_types;
    LOG(WARNING) << "No user type filter specified for " << app_id
                 << ". Using default user type filter, please update the app.";
  }

  bool user_type_match = false;
  for (const auto& it : *value) {
    if (!it.is_string()) {
      LOG(ERROR) << "Invalid user type value for " << app_id << ".";
      return false;
    }
    if (it.GetString() != user_type)
      continue;
    user_type_match = true;
    break;
  }
  if (!user_type_match) {
    VLOG(1) << "Skip " << app_id << ". It does not match user type "
            << user_type << ".";
    return false;
  }

  return true;
}

}  // namespace apps

// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/user_type_filter.h"

#include "base/values.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_thread.h"

namespace apps {

// kUserType is required key that specifies enumeration of user types for which
// web app is visible. See kUserType* constants
const char kKeyUserType[] = "user_type";

const char kUserTypeChild[] = "child";
const char kUserTypeGuest[] = "guest";
const char kUserTypeManaged[] = "managed";
const char kUserTypeSupervised[] = "supervised";
const char kUserTypeUnmanaged[] = "unmanaged";

std::string DetermineUserType(Profile* profile) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(!profile->IsOffTheRecord());
  if (profile->IsGuestSession())
    return kUserTypeGuest;
  if (profile->IsChild())
    return kUserTypeChild;
  if (profile->IsLegacySupervised())
    return kUserTypeSupervised;
  if (profile->GetProfilePolicyConnector()->IsManaged()) {
    return kUserTypeManaged;
  }
  return kUserTypeUnmanaged;
}

bool UserTypeMatchesJsonUserType(const std::string& user_type,
                                 const std::string& app_id,
                                 const base::Value* json_root,
                                 const base::ListValue* default_user_types) {
  DCHECK(json_root);

  if (!json_root->is_dict()) {
    LOG(ERROR) << "Non-dictionary Json is passed to user type filter for "
               << app_id << ".";
    return false;
  }

  const base::Value* value =
      json_root->FindKeyOfType(kKeyUserType, base::Value::Type::LIST);
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
  for (const auto& it : value->GetList()) {
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

// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_USER_TYPE_FILTER_H_
#define CHROME_BROWSER_APPS_USER_TYPE_FILTER_H_

#include <string>
#include "base/values.h"

class Profile;

namespace apps {

extern const char kKeyUserType[];

extern const char kUserTypeChild[];
extern const char kUserTypeGuest[];
extern const char kUserTypeManaged[];
extern const char kUserTypeManagedGuest[];
extern const char kUserTypeUnmanaged[];

// Returns user type based on |profile|. Must be called on UI thread. List of
// possible values are |kUserTypeChild|, |kUserTypeGuest|, |kUserTypeManaged|,
// |kUserTypeManagedGuest|, |kUserTypeSupervised| and |kUserTypeUnmanaged|.
std::string DetermineUserType(Profile* profile);

// This filter is used to verify that profile's user type |user_type| matches
// the user type filter defined in root Json element |json_root|. |json_root|
// should have the list of acceptable user types. Following values are valid:
// |kUserTypeChild|, |kUserTypeGuest|, |kUserTypeManaged|, |kUserTypeSupervised|
// and |kUserTypeUnmanaged|. Primary use of this filter is to determine if
// particular default webapp or extension has to be installed for the current
// user.
// Returns true if profile is accepted for this filter. |default_user_types| is
// optional and used to support transition for the extension based default apps.
// |app_id| is used for error logging purpose.
// Safe to call on non-UI thread.
bool UserTypeMatchesJsonUserType(const std::string& user_type,
                                 const std::string& app_id,
                                 const base::Value::Dict& json_root,
                                 const base::Value::List* default_user_types);

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_USER_TYPE_FILTER_H_

// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SERVICES_CHROMEBOX_FOR_MEETINGS_PUBLIC_CPP_APPID_UTIL_H_
#define ASH_SERVICES_CHROMEBOX_FOR_MEETINGS_PUBLIC_CPP_APPID_UTIL_H_

#include <string>

namespace ash {
namespace cfm {

// Returns true if the id provided matches a valid CfM PA/PWA appid.
bool IsChromeboxForMeetingsAppId(const std::string& app_id);

}  // namespace cfm
}  // namespace ash

#endif  // ASH_SERVICES_CHROMEBOX_FOR_MEETINGS_PUBLIC_CPP_APPID_UTIL_H_

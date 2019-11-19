// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/login_types.h"

#include "chromeos/components/proximity_auth/public/mojom/auth_type.mojom.h"

namespace ash {

EasyUnlockIconOptions::EasyUnlockIconOptions() = default;
EasyUnlockIconOptions::EasyUnlockIconOptions(
    const EasyUnlockIconOptions& other) = default;
EasyUnlockIconOptions::EasyUnlockIconOptions(EasyUnlockIconOptions&& other) =
    default;
EasyUnlockIconOptions::~EasyUnlockIconOptions() = default;

EasyUnlockIconOptions& EasyUnlockIconOptions::operator=(
    const EasyUnlockIconOptions& other) = default;
EasyUnlockIconOptions& EasyUnlockIconOptions::operator=(
    EasyUnlockIconOptions&& other) = default;

InputMethodItem::InputMethodItem() = default;
InputMethodItem::InputMethodItem(const InputMethodItem& other) = default;
InputMethodItem::InputMethodItem(InputMethodItem&& other) = default;
InputMethodItem::~InputMethodItem() = default;

InputMethodItem& InputMethodItem::operator=(const InputMethodItem& other) =
    default;
InputMethodItem& InputMethodItem::operator=(InputMethodItem&& other) = default;

LocaleItem::LocaleItem() = default;
LocaleItem::LocaleItem(const LocaleItem& other) = default;
LocaleItem::LocaleItem(LocaleItem&& other) = default;
LocaleItem::~LocaleItem() = default;

LocaleItem& LocaleItem::operator=(const LocaleItem& other) = default;
LocaleItem& LocaleItem::operator=(LocaleItem&& other) = default;

PublicAccountInfo::PublicAccountInfo() = default;
PublicAccountInfo::PublicAccountInfo(const PublicAccountInfo& other) = default;
PublicAccountInfo::PublicAccountInfo(PublicAccountInfo&& other) = default;
PublicAccountInfo::~PublicAccountInfo() = default;

PublicAccountInfo& PublicAccountInfo::operator=(
    const PublicAccountInfo& other) = default;
PublicAccountInfo& PublicAccountInfo::operator=(PublicAccountInfo&& other) =
    default;

LoginUserInfo::LoginUserInfo()
    : auth_type(proximity_auth::mojom::AuthType::OFFLINE_PASSWORD) {}
LoginUserInfo::LoginUserInfo(const LoginUserInfo& other) = default;
LoginUserInfo::LoginUserInfo(LoginUserInfo&& other) = default;
LoginUserInfo::~LoginUserInfo() = default;

LoginUserInfo& LoginUserInfo::operator=(const LoginUserInfo& other) = default;
LoginUserInfo& LoginUserInfo::operator=(LoginUserInfo&& other) = default;

AuthDisabledData::AuthDisabledData() = default;
AuthDisabledData::AuthDisabledData(AuthDisabledReason reason,
                                   const base::Time& auth_reenabled_time,
                                   const base::TimeDelta& device_used_time,
                                   bool disable_lock_screen_media)
    : reason(reason),
      auth_reenabled_time(auth_reenabled_time),
      device_used_time(device_used_time),
      disable_lock_screen_media(disable_lock_screen_media) {}
AuthDisabledData::AuthDisabledData(const AuthDisabledData& other) = default;
AuthDisabledData::AuthDisabledData(AuthDisabledData&& other) = default;
AuthDisabledData::~AuthDisabledData() = default;

AuthDisabledData& AuthDisabledData::operator=(const AuthDisabledData& other) =
    default;
AuthDisabledData& AuthDisabledData::operator=(AuthDisabledData&& other) =
    default;

SecurityTokenPinRequest::SecurityTokenPinRequest() = default;
SecurityTokenPinRequest::SecurityTokenPinRequest(SecurityTokenPinRequest&&) =
    default;
SecurityTokenPinRequest& SecurityTokenPinRequest::operator=(
    SecurityTokenPinRequest&&) = default;
SecurityTokenPinRequest::~SecurityTokenPinRequest() = default;

}  // namespace ash

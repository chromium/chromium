// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/extensions/users_private/users_private_delegate.h"

#include <memory>

#include "chrome/browser/profiles/profile.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"

namespace extensions {

namespace users_private = api::users_private;

UsersPrivateDelegate::UsersPrivateDelegate(Profile* profile)
    : profile_(profile) {}

UsersPrivateDelegate::~UsersPrivateDelegate() = default;

PrefsUtil* UsersPrivateDelegate::GetPrefsUtil() {
  if (!prefs_util_)
    prefs_util_ = std::make_unique<PrefsUtil>(profile_);

  return prefs_util_.get();
}

}  // namespace extensions

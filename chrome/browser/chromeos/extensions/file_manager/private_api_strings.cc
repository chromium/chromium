// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/file_manager/private_api_strings.h"

#include <memory>
#include <utility>

#include "base/values.h"
#include "chrome/browser/chromeos/file_manager/file_manager_string_util.h"
#include "chrome/browser/profiles/profile.h"
#include "extensions/common/extension_l10n_util.h"

namespace extensions {

FileManagerPrivateGetStringsFunction::FileManagerPrivateGetStringsFunction() =
    default;

FileManagerPrivateGetStringsFunction::~FileManagerPrivateGetStringsFunction() =
    default;

ExtensionFunction::ResponseAction FileManagerPrivateGetStringsFunction::Run() {
  auto dict = GetFileManagerStrings();

  const std::string locale = extension_l10n_util::CurrentLocaleOrDefault();
  AddFileManagerFeatureStrings(
      locale, Profile::FromBrowserContext(browser_context()), dict.get());

  return RespondNow(
      OneArgument(base::Value::FromUniquePtrValue(std::move(dict))));
}

}  // namespace extensions

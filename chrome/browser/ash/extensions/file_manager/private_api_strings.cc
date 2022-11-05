// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/extensions/file_manager/private_api_strings.h"

#include <memory>
#include <utility>

#include "base/values.h"
#include "chrome/browser/ash/file_manager/file_manager_string_util.h"
#include "chrome/browser/profiles/profile.h"
#include "extensions/common/extension_l10n_util.h"

namespace extensions {

FileManagerPrivateGetStringsFunction::FileManagerPrivateGetStringsFunction() =
    default;

FileManagerPrivateGetStringsFunction::~FileManagerPrivateGetStringsFunction() =
    default;

ExtensionFunction::ResponseAction FileManagerPrivateGetStringsFunction::Run() {
  base::Value::Dict dict = GetFileManagerStrings();

  const std::string locale = extension_l10n_util::CurrentLocaleOrDefault();
  AddFileManagerFeatureStrings(
      locale, Profile::FromBrowserContext(browser_context()), &dict);

  return RespondNow(WithArguments(std::move(dict)));
}

}  // namespace extensions

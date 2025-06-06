// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/extensions/file_manager/private_api_strings.h"

#include <memory>
#include <utility>

#include "base/check_deref.h"
#include "base/values.h"
#include "chrome/browser/ash/file_manager/file_manager_string_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "extensions/common/extension_l10n_util.h"

namespace extensions {

FileManagerPrivateGetStringsFunction::FileManagerPrivateGetStringsFunction() =
    default;

FileManagerPrivateGetStringsFunction::~FileManagerPrivateGetStringsFunction() =
    default;

ExtensionFunction::ResponseAction FileManagerPrivateGetStringsFunction::Run() {
  // TODO(crbug.com/404131876): Remove g_browser_process usage.
  const std::string& application_locale =
      g_browser_process->GetApplicationLocale();
  base::Value::Dict dict = GetFileManagerStrings(application_locale);

  const std::string locale = extension_l10n_util::CurrentLocaleOrDefault();
  AddFileManagerFeatureStrings(
      locale, application_locale,
      CHECK_DEREF(g_browser_process->variations_service()),
      Profile::FromBrowserContext(browser_context()), &dict);

  return RespondNow(WithArguments(std::move(dict)));
}

}  // namespace extensions

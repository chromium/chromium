// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/system_log/system_log_api.h"

#include "base/strings/stringprintf.h"
#include "base/syslog_logging.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/system_log.h"
#include "components/device_event_log/device_event_log.h"
#include "extensions/common/extension.h"
#include "extensions/common/features/behavior_feature.h"
#include "extensions/common/features/feature.h"
#include "extensions/common/features/feature_provider.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chromeos/ash/components/browser_context_helper/browser_context_types.h"
#endif

namespace extensions {

namespace {

bool IsSigninProfileCheck(Profile* profile) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  return ash::IsSigninBrowserContext(profile);
#else
  return false;
#endif
}

std::string FormatLogMessage(const std::string& extension_id,
                             Profile* profile,
                             const std::string& message) {
  return base::StringPrintf("[%s]%s: %s", extension_id.c_str(),
                            IsSigninProfileCheck(profile) ? "[signin]" : "",
                            message.c_str());
}

bool IsImprivataExtension(const Extension& extension) {
  const Feature* imprivata_feature = FeatureProvider::GetBehaviorFeature(
      behavior_feature::kImprivataExtension);
  return imprivata_feature->IsAvailableToExtension(&extension).is_available();
}

}  // namespace

SystemLogAddFunction::SystemLogAddFunction() = default;
SystemLogAddFunction::~SystemLogAddFunction() = default;

ExtensionFunction::ResponseAction SystemLogAddFunction::Run() {
  auto parameters = api::system_log::Add::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(parameters);
  const api::system_log::MessageOptions& options = parameters->options;

  Profile* profile = Profile::FromBrowserContext(browser_context());

  std::string device_event_log_message =
      FormatLogMessage(extension_id(), profile, options.message);
  if (IsImprivataExtension(*extension())) {
    SYSLOG(INFO) << base::StringPrintf("extensions: %s",
                                       device_event_log_message.c_str());
    // Will not be added to feedback reports to avoid duplication.
    EXTENSIONS_LOG(DEBUG) << device_event_log_message;
  } else {
    EXTENSIONS_LOG(EVENT) << device_event_log_message;
  }

  return RespondNow(NoArguments());
}

}  // namespace extensions

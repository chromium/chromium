// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/extension_telemetry/activity_log_ingester.h"

#include "base/functional/bind.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "chrome/browser/extensions/activity_log/activity_action_constants.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/extension_telemetry/dom_access_signal.h"
#include "chrome/browser/safe_browsing/extension_telemetry/extension_telemetry_service.h"
#include "chrome/browser/safe_browsing/extension_telemetry/script_injection_signal.h"
#include "chrome/common/extensions/activity_log_policy_util.h"
#include "extensions/common/dom_action_types.h"

namespace safe_browsing {

ActivityLogIngester::ActivityLogIngester(
    Profile* profile,
    ExtensionTelemetryService* telemetry_service)
    : profile_(profile), telemetry_service_(telemetry_service) {
  auto* activity_log = extensions::ActivityLog::GetInstance(profile_);
  if (activity_log) {
    activity_log->SetTelemetryLoggingEnabled(
        true, base::BindRepeating(&ActivityLogIngester::OnExtensionActivity,
                                  weak_factory_.GetWeakPtr()));
  }
}

ActivityLogIngester::~ActivityLogIngester() {
  auto* activity_log = extensions::ActivityLog::GetInstance(profile_);
  if (activity_log) {
    activity_log->SetTelemetryLoggingEnabled(false, base::NullCallback());
  }
}

void ActivityLogIngester::OnExtensionActivity(
    scoped_refptr<extensions::Action> action) {
  // Use an empty list if arguments are missing to avoid dereferencing a
  // null optional.
  static const base::NoDestructor<base::ListValue> kEmptyArgs;
  const base::ListValue& args =
      action->args().has_value() ? action->args().value() : *kEmptyArgs;

  // We omit the `action_type` parameter here, which defaults to `MODIFIED`
  // (catch-all). This is because browser-side telemetry logic trusts the
  // previous filtering that occurred in the renderer process.
  auto signal_type =
      extensions::activity_log_policy_util::GetTelemetrySignalType(
          action->api_name(), args);

  if (signal_type ==
      extensions::activity_log_policy_util::TelemetrySignalType::kNone) {
    return;
  }

  switch (signal_type) {
    case extensions::activity_log_policy_util::TelemetrySignalType::kDOMAccess:
      // The utility guarantees this is a GETTER if it returned kDOMAccess.
      telemetry_service_->AddSignal(std::make_unique<DOMAccessSignal>(
          action->extension_id(), action->api_name(), action->page_url().spec(),
          DOMAccessSignal::DOMAccess::READ, action->time()));
      break;
    case extensions::activity_log_policy_util::TelemetrySignalType::
        kScriptInjection:
      telemetry_service_->AddSignal(std::make_unique<ScriptInjectionSignal>(
          action->extension_id(), action->api_name(), action->page_url().spec(),
          extensions::activity_log_policy_util::GetArgumentsList(
              action->api_name(), args),
          action->arg_url().spec(), action->time()));
      break;
    case extensions::activity_log_policy_util::TelemetrySignalType::kNone:
      // We should never reach here because of the early return above, but
      // the compiler requires all enum values to be handled.
      NOTREACHED();
  }
}

}  // namespace safe_browsing

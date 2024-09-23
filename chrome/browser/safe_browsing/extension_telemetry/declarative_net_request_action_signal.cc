// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/extension_telemetry/declarative_net_request_action_signal.h"

#include <memory>

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"

namespace safe_browsing {

DeclarativeNetRequestActionSignal::DeclarativeNetRequestActionSignal(
    const extensions::ExtensionId& extension_id,
    const DeclarativeNetRequestActionInfo::ActionDetails& action_details)
    : ExtensionSignal(extension_id), action_details_(action_details) {}

DeclarativeNetRequestActionSignal::~DeclarativeNetRequestActionSignal() =
    default;

// static
std::unique_ptr<DeclarativeNetRequestActionSignal>
DeclarativeNetRequestActionSignal::
    CreateDeclarativeNetRequestRedirectActionSignal(
        const extensions::ExtensionId& extension_id,
        const GURL& request_url,
        const GURL& redirect_url) {
  DeclarativeNetRequestActionInfo::ActionDetails action_details;
  action_details.set_type(DeclarativeNetRequestActionInfo::REDIRECT);
  action_details.set_request_url(request_url.GetWithoutFilename().spec());
  action_details.set_redirect_url(redirect_url.GetWithoutFilename().spec());
  action_details.set_count(1);

  return std::make_unique<DeclarativeNetRequestActionSignal>(
      DeclarativeNetRequestActionSignal(extension_id,
                                        std::move(action_details)));
}

ExtensionSignalType DeclarativeNetRequestActionSignal::GetType() const {
  return ExtensionSignalType::kDeclarativeNetRequestAction;
}

std::string DeclarativeNetRequestActionSignal::GetUniqueActionDetailsId()
    const {
  return base::JoinString(
      {base::NumberToString(static_cast<int>(action_details_.type())),
       action_details_.request_url(), action_details_.redirect_url()},
      ",");
}

}  // namespace safe_browsing

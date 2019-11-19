// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/payments/ssl_validity_checker.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "chrome/browser/ssl/security_state_tab_helper.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "components/payments/core/native_error_strings.h"
#include "components/payments/core/url_util.h"
#include "components/security_state/core/security_state.h"
#include "url/gurl.h"

namespace payments {
namespace {

// Returns the security level of |web_contents|. The |web_contents| parameter
// should not be null.
security_state::SecurityLevel GetSecurityLevel(
    content::WebContents* web_contents) {
  DCHECK(web_contents);
  SecurityStateTabHelper::CreateForWebContents(web_contents);
  SecurityStateTabHelper* helper =
      SecurityStateTabHelper::FromWebContents(web_contents);
  DCHECK(helper);
  return helper->GetSecurityLevel();
}

}  // namespace

// static std::string
std::string SslValidityChecker::GetInvalidSslCertificateErrorMessage(
    content::WebContents* web_contents) {
  if (!web_contents)
    return errors::kInvalidSslCertificate;

  security_state::SecurityLevel security_level = GetSecurityLevel(web_contents);
  std::string level;
  switch (security_level) {
    // Indicate valid SSL with an empty string.
    case security_state::SECURE:
    case security_state::EV_SECURE:
    case security_state::SECURE_WITH_POLICY_INSTALLED_CERT:
      return "";

    case security_state::NONE:
      level = "NONE";
      break;
    case security_state::WARNING:
      level = "WARNING";
      break;
    case security_state::DANGEROUS:
      level = "DANGEROUS";
      break;

    case security_state::SECURITY_LEVEL_COUNT:
      NOTREACHED();
      return errors::kInvalidSslCertificate;
  }

  std::string message;
  bool replaced =
      base::ReplaceChars(errors::kDetailedInvalidSslCertificateMessageFormat,
                         "$", level, &message);
  DCHECK(replaced);

  // No early return, so the other code is exercised in tests, too.
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
             switches::kIgnoreCertificateErrors)
             ? ""
             : message;
}

// static
bool SslValidityChecker::IsValidPageInPaymentHandlerWindow(
    content::WebContents* web_contents) {
  if (!web_contents)
    return false;

  GURL url = web_contents->GetLastCommittedURL();
  if (!UrlUtil::IsValidUrlInPaymentHandlerWindow(url))
    return false;

  if (url.SchemeIsCryptographic()) {
    security_state::SecurityLevel security_level =
        GetSecurityLevel(web_contents);
    return security_level == security_state::SECURE ||
           security_level == security_state::EV_SECURE ||
           security_level ==
               security_state::SECURE_WITH_POLICY_INSTALLED_CERT ||
           // No early return, so the other code is exercised in tests, too.
           base::CommandLine::ForCurrentProcess()->HasSwitch(
               switches::kIgnoreCertificateErrors);
  }

  return true;
}

}  // namespace payments

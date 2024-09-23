// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autocomplete/chrome_autocomplete_scheme_classifier.h"

#include "base/notreached.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "chrome/browser/custom_handlers/protocol_handler_registry_factory.h"
#include "chrome/browser/external_protocol/external_protocol_handler.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_io_data.h"
#include "components/custom_handlers/protocol_handler_registry.h"
#include "content/public/common/url_constants.h"
#include "url/url_util.h"

#if BUILDFLAG(IS_ANDROID)
// Must come after other includes, because FromJniType() uses Profile.
#include "chrome/browser/ui/android/omnibox/jni_headers/ChromeAutocompleteSchemeClassifier_jni.h"
#endif

#if BUILDFLAG(IS_ANDROID)
static jlong
JNI_ChromeAutocompleteSchemeClassifier_CreateAutocompleteClassifier(
    JNIEnv* env,
    Profile* profile) {
  DCHECK(profile);

  return reinterpret_cast<intptr_t>(
      new ChromeAutocompleteSchemeClassifier(profile));
}

static void JNI_ChromeAutocompleteSchemeClassifier_DeleteAutocompleteClassifier(
    JNIEnv* env,
    jlong chrome_autocomplete_scheme_classifier) {
  delete reinterpret_cast<ChromeAutocompleteSchemeClassifier*>(
      chrome_autocomplete_scheme_classifier);
}
#endif

ChromeAutocompleteSchemeClassifier::ChromeAutocompleteSchemeClassifier(
    Profile* profile)
    : profile_(profile) {
}

ChromeAutocompleteSchemeClassifier::~ChromeAutocompleteSchemeClassifier() {
}

metrics::OmniboxInputType
ChromeAutocompleteSchemeClassifier::GetInputTypeForScheme(
    const std::string& scheme) const {
  if (scheme.empty()) {
    return metrics::OmniboxInputType::EMPTY;
  }
  if (base::IsStringASCII(scheme) &&
      (ProfileIOData::IsHandledProtocol(scheme) ||
       base::EqualsCaseInsensitiveASCII(scheme, content::kViewSourceScheme) ||
       base::EqualsCaseInsensitiveASCII(scheme, url::kJavaScriptScheme) ||
       base::EqualsCaseInsensitiveASCII(scheme, url::kDataScheme))) {
    return metrics::OmniboxInputType::URL;
  }

  // Also check for schemes registered via registerProtocolHandler(), which
  // can be handled by web pages/apps.
  custom_handlers::ProtocolHandlerRegistry* registry =
      profile_ ? ProtocolHandlerRegistryFactory::GetForBrowserContext(profile_)
               : nullptr;
  if (registry && registry->IsHandledProtocol(scheme))
    return metrics::OmniboxInputType::URL;

  // Not an internal protocol; check if it's an external protocol, i.e. one
  // that's registered on the user's OS and will shell out to another program.
  //
  // We need to do this after the checks above because some internally
  // handlable schemes (e.g. "javascript") may be treated as "blocked" by the
  // external protocol handler because we don't want pages to open them, but
  // users still can.
  const ExternalProtocolHandler::BlockState block_state =
      ExternalProtocolHandler::GetBlockState(scheme, nullptr, profile_);
  switch (block_state) {
    case ExternalProtocolHandler::DONT_BLOCK:
      return metrics::OmniboxInputType::URL;

    case ExternalProtocolHandler::BLOCK:
      // If we don't want the user to open the URL, don't let it be navigated
      // to at all.
      return metrics::OmniboxInputType::QUERY;

    case ExternalProtocolHandler::UNKNOWN: {
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
      // Linux impl of GetApplicationNameForScheme doesn't distinguish
      // between URL schemes with handers and those without. This will
      // make the default behaviour be search on Linux.
      return metrics::OmniboxInputType::EMPTY;
#else
      // If block state is unknown, check if there is an application registered
      // for the url scheme.
      GURL url(scheme + "://");
      std::u16string application_name =
          shell_integration::GetApplicationNameForScheme(url);
      return application_name.empty() ? metrics::OmniboxInputType::EMPTY
                                      : metrics::OmniboxInputType::URL;
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
    }
  }
  NOTREACHED_IN_MIGRATION();
  return metrics::OmniboxInputType::EMPTY;
}

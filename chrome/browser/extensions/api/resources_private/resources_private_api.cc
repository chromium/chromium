// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/resources_private/resources_private_api.h"

#include <string>
#include <utility>

#include "base/values.h"
#include "build/chromeos_buildflags.h"
#include "chrome/common/extensions/api/resources_private.h"
#include "chrome/grit/generated_resources.h"
#include "extensions/browser/extensions_browser_client.h"
#include "pdf/buildflags.h"
#include "printing/buildflags/buildflags.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/webui/web_ui_util.h"

#if BUILDFLAG(ENABLE_PDF)
#include "chrome/browser/pdf/pdf_extension_util.h"
#endif  // BUILDFLAG(ENABLE_PDF)

// To add a new component to this API, simply:
//
// 1. Add your component to the Component enum in
//    chrome/common/extensions/api/resources_private.idl
// 2. Create a `base::Value::Dict GetStringsForMyComponent()` method.
// 3. Tie in that method to the switch statement in `Run()`.

namespace extensions {

namespace {

base::Value::Dict GetStringsForIdentity() {
  return base::Value::Dict().Set(
      "window-title",
      l10n_util::GetStringUTF16(IDS_EXTENSION_CONFIRM_PERMISSIONS));
}

}  // namespace

namespace get_strings = api::resources_private::GetStrings;

ResourcesPrivateGetStringsFunction::ResourcesPrivateGetStringsFunction() =
    default;

ResourcesPrivateGetStringsFunction::~ResourcesPrivateGetStringsFunction() =
    default;

ExtensionFunction::ResponseAction ResourcesPrivateGetStringsFunction::Run() {
  get_strings::Params params = get_strings::Params::Create(args()).value();
  base::Value::Dict dict;

  switch (params.component) {
    case api::resources_private::Component::kIdentity:
      dict = GetStringsForIdentity();
      break;
    case api::resources_private::Component::kPdf: {
#if BUILDFLAG(ENABLE_PDF)
      dict = pdf_extension_util::GetStrings(
          pdf_extension_util::PdfViewerContext::kAll);
      dict.Merge(pdf_extension_util::GetAdditionalData(browser_context()));
#endif  // BUILDFLAG(ENABLE_PDF)
      break;
    }
    case api::resources_private::Component::kNone:
      NOTREACHED();
  }

  webui::SetLoadTimeDataDefaults(
      ExtensionsBrowserClient::Get()->GetApplicationLocale(), &dict);
  return RespondNow(WithArguments(std::move(dict)));
}

}  // namespace extensions

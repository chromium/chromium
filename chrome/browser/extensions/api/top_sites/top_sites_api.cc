// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/top_sites/top_sites_api.h"

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/values.h"
#include "chrome/browser/history/top_sites_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/history/core/browser/top_sites.h"

namespace extensions {

TopSitesGetFunction::TopSitesGetFunction() = default;
TopSitesGetFunction::~TopSitesGetFunction() = default;

ExtensionFunction::ResponseAction TopSitesGetFunction::Run() {
  scoped_refptr<history::TopSites> ts = TopSitesFactory::GetForProfile(
      Profile::FromBrowserContext(browser_context()));
  if (!ts) {
    return RespondNow(Error(kUnknownErrorDoNotUse));
  }

  ts->GetMostVisitedURLs(
      base::BindOnce(&TopSitesGetFunction::OnMostVisitedURLsAvailable, this));

  // GetMostVisitedURLs() will invoke the callback synchronously if the URLs are
  // already populated.
  return did_respond() ? AlreadyResponded() : RespondLater();
}

void TopSitesGetFunction::OnMostVisitedURLsAvailable(
    const history::MostVisitedURLList& data) {
  base::Value::List pages_value;
  for (const auto& url : data) {
    if (!url.url.is_empty()) {
      base::Value::Dict page_value;
      page_value.Set("url", url.url.spec());
      if (url.title.empty()) {
        page_value.Set("title", url.url.spec());
      } else {
        page_value.Set("title", url.title);
      }
      pages_value.Append(std::move(page_value));
    }
  }

  Respond(WithArguments(std::move(pages_value)));
}

}  // namespace extensions

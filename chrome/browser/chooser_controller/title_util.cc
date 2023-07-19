// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chooser_controller/title_util.h"

#include <string>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/url_identity.h"
#include "content/public/browser/render_frame_host.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

namespace {

constexpr UrlIdentity::TypeSet kUrlIdentityAllowedTypes = {
    UrlIdentity::Type::kDefault, UrlIdentity::Type::kFile,
    UrlIdentity::Type::kIsolatedWebApp, UrlIdentity::Type::kChromeExtension};
constexpr UrlIdentity::FormatOptions kUrlIdentityOptions{
    .default_options = {
        UrlIdentity::DefaultFormatOptions::kOmitCryptographicScheme}};

}  // namespace

std::u16string CreateChooserTitle(content::RenderFrameHost* render_frame_host,
                                  int title_string_id) {
  if (!render_frame_host) {
    return u"";
  }
  // Ensure the permission request is attributed to the main frame.
  render_frame_host = render_frame_host->GetMainFrame();

  const GURL& url = render_frame_host->GetLastCommittedOrigin().GetURL();
  Profile* profile =
      Profile::FromBrowserContext(render_frame_host->GetBrowserContext());

  UrlIdentity identity = UrlIdentity::CreateFromUrl(
      profile, url, kUrlIdentityAllowedTypes, kUrlIdentityOptions);

  return l10n_util::GetStringFUTF16(title_string_id, identity.name);
}

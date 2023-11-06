// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_manager/providers/web_contents/isolated_web_app_task.h"

#include <string>

#include "base/i18n/rtl.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/task_manager/providers/task.h"
#include "chrome/browser/task_manager/providers/web_contents/renderer_task.h"
#include "chrome/browser/ui/url_identity.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

namespace {

std::u16string PrefixTaskTitle(std::u16string title) {
  return l10n_util::GetStringFUTF16(IDS_TASK_MANAGER_APP_PREFIX, title);
}

// The only expected URL type for `UrlIdentity::CreateFromUrl()` is
// kIsolatedWebApp.
constexpr UrlIdentity::TypeSet kUrlIdentityAllowedTypes = {
    UrlIdentity::Type::kIsolatedWebApp};
constexpr UrlIdentity::FormatOptions kUrlIdentityOptions = {};

// Similar to RendererTask::GetTitleFromWebContents() but falls back to app name
// when title is empty.
std::u16string GetTitleOrAppNameFromWebContents(
    content::WebContents* web_contents) {
  DCHECK(web_contents);
  std::u16string title = web_contents->GetTitle();
  // Empty title falls back to navigation URL.
  if (base::StartsWith(title, chrome::kIsolatedAppSchemeUtf16,
                       base::CompareCase::SENSITIVE)) {
    const GURL& url = web_contents->GetLastCommittedURL();
    Profile* profile =
        Profile::FromBrowserContext(web_contents->GetBrowserContext());
    title = UrlIdentity::CreateFromUrl(profile, url, kUrlIdentityAllowedTypes,
                                       kUrlIdentityOptions)
                .name;
    title = base::i18n::GetDisplayStringInLTRDirectionality(title);
  } else {
    base::i18n::AdjustStringForLocaleDirection(&title);
  }
  return title;
}

}  // namespace

namespace task_manager {

IsolatedWebAppTask::IsolatedWebAppTask(content::WebContents* web_contents)
    : RendererTask(
          PrefixTaskTitle(RendererTask::GetTitleFromWebContents(web_contents)),
          RendererTask::GetFaviconFromWebContents(web_contents),
          web_contents) {}

IsolatedWebAppTask::~IsolatedWebAppTask() = default;

void IsolatedWebAppTask::UpdateTitle() {
  set_title(PrefixTaskTitle(GetTitleOrAppNameFromWebContents(web_contents())));
}

void IsolatedWebAppTask::UpdateFavicon() {
  const gfx::ImageSkia* icon =
      RendererTask::GetFaviconFromWebContents(web_contents());
  set_icon(icon ? *icon : gfx::ImageSkia());
}

}  // namespace task_manager

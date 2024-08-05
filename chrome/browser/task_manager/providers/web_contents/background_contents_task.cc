// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_manager/providers/web_contents/background_contents_task.h"

#include <string>

#include "base/i18n/rtl.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/background/background_contents.h"
#include "chrome/browser/background/background_contents_service.h"
#include "chrome/browser/background/background_contents_service_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/view_type_utils.h"
#include "extensions/common/extension_set.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/image/image_skia.h"

namespace task_manager {

namespace {

std::u16string AdjustAndLocalizeTitle(const std::u16string& title,
                                      const std::string& url_spec) {
  std::u16string localized_title(title);
  if (localized_title.empty()) {
    // No title (can't locate the parent app for some reason) so just display
    // the URL (properly forced to be LTR).
    localized_title = base::i18n::GetDisplayStringInLTRDirectionality(
        base::UTF8ToUTF16(url_spec));
  }

  // Ensure that the string has the appropriate direction markers.
  base::i18n::AdjustStringForLocaleDirection(&localized_title);
  return l10n_util::GetStringFUTF16(IDS_TASK_MANAGER_BACKGROUND_APP_PREFIX,
                                    localized_title);
}

}  // namespace

gfx::ImageSkia* BackgroundContentsTask::s_icon_ = nullptr;

BackgroundContentsTask::BackgroundContentsTask(
    const std::u16string& title,
    BackgroundContents* background_contents)
    : RendererTask(
          AdjustAndLocalizeTitle(title, background_contents->GetURL().spec()),
          FetchIcon(IDR_PLUGINS_FAVICON, &s_icon_),
          background_contents->web_contents()) {}

BackgroundContentsTask::~BackgroundContentsTask() {
}

void BackgroundContentsTask::UpdateTitle() {
  // TODO(afakhry): At the time of integration testing figure out whether we
  // need to change the title of the task here.
}

void BackgroundContentsTask::UpdateFavicon() {
  // We don't do anything here. For background contents we always use the
  // default icon.
}

}  // namespace task_manager

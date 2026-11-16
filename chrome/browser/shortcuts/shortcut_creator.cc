// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/shortcuts/shortcut_creator.h"

#include <string>

#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/page_type.h"
#include "extensions/buildflags/buildflags.h"
#include "ui/gfx/image/image_family.h"
#include "url/gurl.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/common/constants.h"
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

namespace shortcuts {

bool CanCreateShortcutForWebContents(content::WebContents* web_contents) {
  // Do not allow if the web_contents appear to be crashing.
  if (!web_contents || web_contents->IsCrashed()) {
    return false;
  }

  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());

  // Only allow regular profiles.
  if (!profile || !profile->IsRegularProfile()) {
    return false;
  }

  // Do not allow for error pages (like network errors etc).
  content::NavigationEntry* entry =
      web_contents->GetController().GetLastCommittedEntry();
  if (entry && entry->GetPageType() == content::PAGE_TYPE_ERROR) {
    return false;
  }

  // Do not allow if the site_url is invalid.
  const GURL site_url = web_contents->GetLastCommittedURL();
  if (!site_url.is_valid()) {
    return false;
  }

  // Only URLs that have a scheme of `HTTP/HTTPs` or `chrome-extension` are
  // allowed.
  bool is_valid_for_shortcuts = site_url.SchemeIsHTTPOrHTTPS();
#if BUILDFLAG(ENABLE_EXTENSIONS)
  is_valid_for_shortcuts =
      is_valid_for_shortcuts || site_url.SchemeIs(extensions::kExtensionScheme);
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

  return is_valid_for_shortcuts;
}

ShortcutMetadata::ShortcutMetadata() = default;
ShortcutMetadata::ShortcutMetadata(base::FilePath profile_path,
                                   GURL shortcut_url,
                                   std::u16string shortcut_title,
                                   gfx::ImageFamily shortcut_images)
    : profile_path(profile_path),
      shortcut_url(shortcut_url),
      shortcut_title(shortcut_title),
      shortcut_images(std::move(shortcut_images)) {
  CHECK(IsValid());
}
ShortcutMetadata::~ShortcutMetadata() = default;

ShortcutMetadata::ShortcutMetadata(ShortcutMetadata&& metadata) = default;
ShortcutMetadata& ShortcutMetadata::operator=(ShortcutMetadata&& metadata) =
    default;

bool ShortcutMetadata::IsValid() {
  return !profile_path.empty() && shortcut_url.is_valid() &&
         !shortcut_title.empty() && profile_path.BaseName() != profile_path &&
         !shortcut_images.empty();
}

void EmitIconStorageCountMetric(const base::FilePath& icon_directory) {
  size_t num_files = 0;
  base::FileEnumerator file_iter(icon_directory, /*recursive=*/false,
                                 base::FileEnumerator::FILES);
  while (!file_iter.Next().empty()) {
    ++num_files;
  }
  // Note: If this metric shows that the icon directory is getting too full,
  // then a new design or cleanup may be required.
  base::UmaHistogramCounts100000("Shortcuts.Icons.StorageCount", num_files);
}

}  // namespace shortcuts

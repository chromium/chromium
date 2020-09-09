// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/shortcut_info.h"

#include "base/feature_list.h"
#include "base/optional.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/android/shortcut_helper.h"
#include "third_party/blink/public/common/manifest/manifest_icon_selector.h"

namespace {

// The maximum number of shortcuts an Android launcher supports.
// https://developer.android.com/guide/topics/ui/shortcuts#shortcut-limitations
constexpr size_t kMaxShortcuts = 4;

}  // namespace

ShareTargetParamsFile::ShareTargetParamsFile() {}

ShareTargetParamsFile::ShareTargetParamsFile(
    const ShareTargetParamsFile& other) = default;

ShareTargetParamsFile::~ShareTargetParamsFile() {}

ShareTargetParams::ShareTargetParams() {}

ShareTargetParams::ShareTargetParams(const ShareTargetParams& other) = default;

ShareTargetParams::~ShareTargetParams() {}

ShareTarget::ShareTarget() {}

ShareTarget::~ShareTarget() {}

ShortcutInfo::ShortcutInfo(const GURL& shortcut_url)
    : url(shortcut_url),
      display(blink::mojom::DisplayMode::kBrowser),
      orientation(device::mojom::ScreenOrientationLockType::DEFAULT),
      source(SOURCE_ADD_TO_HOMESCREEN_SHORTCUT),
      ideal_splash_image_size_in_px(0),
      minimum_splash_image_size_in_px(0) {}

ShortcutInfo::ShortcutInfo(const ShortcutInfo& other) = default;

ShortcutInfo::~ShortcutInfo() {
}

void ShortcutInfo::UpdateFromManifest(const blink::Manifest& manifest) {
  base::string16 s_name = manifest.short_name.value_or(base::string16());
  base::string16 f_name = manifest.name.value_or(base::string16());
  if (!s_name.empty() || !f_name.empty()) {
    short_name = s_name;
    name = f_name;
    if (short_name.empty())
      short_name = name;
    else if (name.empty())
      name = short_name;
  }
  user_title = short_name;

  // Set the url based on the manifest value, if any.
  if (manifest.start_url.is_valid())
    url = manifest.start_url;

  scope = manifest.scope;

  // Set the display based on the manifest value, if any.
  if (manifest.display != blink::mojom::DisplayMode::kUndefined)
    display = manifest.display;

  if (display == blink::mojom::DisplayMode::kStandalone ||
      display == blink::mojom::DisplayMode::kFullscreen ||
      display == blink::mojom::DisplayMode::kMinimalUi) {
    source = SOURCE_ADD_TO_HOMESCREEN_STANDALONE;
    // Set the orientation based on the manifest value, or ignore if the display
    // mode is different from 'standalone', 'fullscreen' or 'minimal-ui'.
    if (manifest.orientation !=
        device::mojom::ScreenOrientationLockType::DEFAULT) {
      // TODO(mlamouri): Send a message to the developer console if we ignored
      // Manifest orientation because display property is not set.
      orientation = manifest.orientation;
    }
  }

  // Set the theme color based on the manifest value, if any.
  if (manifest.theme_color)
    theme_color = manifest.theme_color;

  // Set the background color based on the manifest value, if any.
  if (manifest.background_color)
    background_color = manifest.background_color;

  // Set the icon urls based on the icons in the manifest, if any.
  icon_urls.clear();
  for (const auto& icon : manifest.icons)
    icon_urls.push_back(icon.src.spec());

  if (manifest.share_target) {
    share_target = ShareTarget();
    share_target->action = manifest.share_target->action;
    share_target->method = manifest.share_target->method;
    share_target->enctype = manifest.share_target->enctype;
    if (manifest.share_target->params.text)
      share_target->params.text = *manifest.share_target->params.text;
    if (manifest.share_target->params.title)
      share_target->params.title = *manifest.share_target->params.title;
    if (manifest.share_target->params.url)
      share_target->params.url = *manifest.share_target->params.url;

    for (blink::Manifest::FileFilter manifest_share_target_file :
         manifest.share_target->params.files) {
      ShareTargetParamsFile share_target_params_file;
      share_target_params_file.name = manifest_share_target_file.name;
      share_target_params_file.accept = manifest_share_target_file.accept;
      share_target->params.files.push_back(share_target_params_file);
    }
  }

  shortcut_items = manifest.shortcuts;
  if (shortcut_items.size() > kMaxShortcuts)
    shortcut_items.resize(kMaxShortcuts);

  for (auto& shortcut_item : shortcut_items) {
    if (!shortcut_item.short_name || shortcut_item.short_name->empty())
      shortcut_item.short_name = shortcut_item.name;
  }

  int ideal_shortcut_icons_size_px =
      ShortcutHelper::GetIdealShortcutIconSizeInPx();
  for (const auto& manifest_shortcut : shortcut_items) {
    GURL best_url = blink::ManifestIconSelector::FindBestMatchingSquareIcon(
        manifest_shortcut.icons, ideal_shortcut_icons_size_px,
        /* minimum_icon_size_in_px= */ ideal_shortcut_icons_size_px / 2,
        blink::Manifest::ImageResource::Purpose::ANY);
    best_shortcut_icon_urls.push_back(std::move(best_url));
  }
}

void ShortcutInfo::UpdateSource(const Source new_source) {
  source = new_source;
}

// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_action_icon_factory.h"

#include "base/metrics/histogram_macros.h"
#include "chrome/browser/extensions/extension_ui_util.h"
#include "chrome/browser/profiles/profile.h"
#include "extensions/browser/extension_action.h"
#include "extensions/common/extension.h"
#include "extensions/common/image_util.h"
#include "extensions/common/manifest.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"

using extensions::Extension;
using extensions::IconImage;

namespace {

bool g_allow_invisible_icons = true;

}  // namespace

// static
void ExtensionActionIconFactory::SetAllowInvisibleIconsForTest(bool value) {
  g_allow_invisible_icons = value;
}

ExtensionActionIconFactory::ExtensionActionIconFactory(
    Profile* profile,
    const Extension* extension,
    extensions::ExtensionAction* action,
    Observer* observer)
    : profile_(profile),
      action_(action),
      observer_(observer),
      should_check_icons_(extension->location() !=
                          extensions::mojom::ManifestLocation::kUnpacked) {
  if (action->default_icon_image())
    icon_image_observation_.Observe(action->default_icon_image());
}

ExtensionActionIconFactory::~ExtensionActionIconFactory() {}

// extensions::IconImage::Observer overrides.
void ExtensionActionIconFactory::OnExtensionIconImageChanged(IconImage* image) {
  if (observer_)
    observer_->OnIconUpdated();
}

void ExtensionActionIconFactory::OnExtensionIconImageDestroyed(
    IconImage* image) {
  icon_image_observation_.Reset();
}

gfx::Image ExtensionActionIconFactory::GetIcon(int tab_id) {
  gfx::Image icon = action_->GetExplicitlySetIcon(tab_id);
  if (!icon.IsEmpty())
    return icon;

  icon = action_->GetDeclarativeIcon(tab_id);
  if (!icon.IsEmpty())
    return icon;

  if (cached_default_icon_image_.IsEmpty()) {
    icon = action_->GetDefaultIconImage();
    // If the extension is packed, then check the icon for visibility. Icons
    // for unpacked extensions are checked at load time, so we ignore them
    // here.
    if (should_check_icons_) {
      const SkBitmap* const bitmap = icon.ToSkBitmap();
      const bool is_sufficiently_visible =
          extensions::image_util::IsIconSufficientlyVisible(*bitmap);
      UMA_HISTOGRAM_BOOLEAN("Extensions.ManifestIconSetIconWasVisibleForPacked",
                            is_sufficiently_visible);
      if (!is_sufficiently_visible && !g_allow_invisible_icons) {
        icon = action_->GetPlaceholderIconImage();
      }
    }
    cached_default_icon_image_ = icon;
  }

  return cached_default_icon_image_;
}

// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/chrome_app_icon.h"

#include <algorithm>

#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/chrome_app_icon_delegate.h"
#include "chrome/browser/extensions/extension_util.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_util.h"
#include "extensions/common/manifest_handlers/icons_handler.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/image/canvas_image_source.h"
#include "ui/gfx/image/image_skia_operations.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/extensions/gfx_utils.h"
#endif

namespace extensions {

namespace {

// Rounds the corners of a given image.
// TODO(khmel): avoid sub-classing CanvasImageSource.
class RoundedCornersImageSource : public gfx::CanvasImageSource {
 public:
  explicit RoundedCornersImageSource(const gfx::ImageSkia& icon)
      : gfx::CanvasImageSource(icon.size()), icon_(icon) {}

  RoundedCornersImageSource(const RoundedCornersImageSource&) = delete;
  RoundedCornersImageSource& operator=(const RoundedCornersImageSource&) =
      delete;

  ~RoundedCornersImageSource() override {}

 private:
  // gfx::CanvasImageSource overrides:
  void Draw(gfx::Canvas* canvas) override {
    // The radius used to round the app icon, based on 2 pixel per 48 pixels
    // icon size.
    const int rounding_radius =
        std::max<int>(std::round(2.0 * icon_.width() / 48.0), 1);

    canvas->DrawImageInt(icon_, 0, 0);

    cc::PaintFlags masking_flags;
    masking_flags.setBlendMode(SkBlendMode::kDstIn);
    canvas->SaveLayerWithFlags(masking_flags);

    cc::PaintFlags mask_flags;
    mask_flags.setAntiAlias(true);
    mask_flags.setColor(SK_ColorWHITE);
    canvas->DrawRoundRect(gfx::Rect(icon_.width(), icon_.height()),
                          rounding_radius, mask_flags);

    canvas->Restore();
  }

  gfx::ImageSkia icon_;
};

}  // namespace

// static
void ChromeAppIcon::ApplyEffects(int resource_size_in_dip,
                                 const ResizeFunction& resize_function,
                                 bool app_launchable,
                                 bool rounded_corners,
                                 Badge badge_type,
                                 gfx::ImageSkia* image_skia) {
  if (!resize_function.is_null()) {
    resize_function.Run(gfx::Size(resource_size_in_dip, resource_size_in_dip),
                        image_skia);
  }

  if (!app_launchable) {
    constexpr color_utils::HSL shift = {-1, 0, 0.6};
    *image_skia =
        gfx::ImageSkiaOperations::CreateHSLShiftedImage(*image_skia, shift);
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Badge should be added after graying out the icon to have a crisp look.
  if (badge_type != Badge::kNone) {
    util::ApplyBadge(image_skia, badge_type);
  }
#endif

  if (rounded_corners) {
    *image_skia =
        gfx::ImageSkia(std::make_unique<RoundedCornersImageSource>(*image_skia),
                       image_skia->size());
  }
}

ChromeAppIcon::ChromeAppIcon(ChromeAppIconDelegate* delegate,
                             content::BrowserContext* browser_context,
                             DestroyedCallback destroyed_callback,
                             const std::string& app_id,
                             int resource_size_in_dip,
                             const ResizeFunction& resize_function)
    : delegate_(delegate),
      browser_context_(browser_context),
      destroyed_callback_(std::move(destroyed_callback)),
      app_id_(app_id),
      resource_size_in_dip_(resource_size_in_dip),
      resize_function_(resize_function) {
  DCHECK(delegate_);
  DCHECK(browser_context_);
  DCHECK(!destroyed_callback_.is_null());
  DCHECK_GE(resource_size_in_dip, 0);
  Reload();
}

ChromeAppIcon::~ChromeAppIcon() {
  std::move(destroyed_callback_).Run(this);
}

const Extension* ChromeAppIcon::GetExtension() {
  return ExtensionRegistry::Get(browser_context_)
      ->GetInstalledExtension(app_id_);
}

void ChromeAppIcon::Reload() {
  const Extension* extension = GetExtension();
  const gfx::ImageSkia default_icon = extension && extension->is_app()
                                          ? util::GetDefaultAppIcon()
                                          : util::GetDefaultExtensionIcon();
  icon_ = std::make_unique<IconImage>(
      browser_context_, extension,
      extension ? IconsInfo::GetIcons(extension) : ExtensionIconSet(),
      resource_size_in_dip_, !resize_function_.is_null(), default_icon, this);
  UpdateIcon();
}

bool ChromeAppIcon::IsValid() const {
  DCHECK(icon_);
  return icon_->is_valid();
}

void ChromeAppIcon::UpdateIcon() {
  DCHECK(icon_);

  image_skia_ = icon_->image_skia();

  Badge badge_type = Badge::kNone;
  bool app_launchable = util::IsAppLaunchable(app_id_, browser_context_);
#if BUILDFLAG(IS_CHROMEOS_ASH)
  has_chrome_badge_ = util::ShouldApplyChromeBadge(browser_context_, app_id_);
  if (!app_launchable) {
    badge_type = Badge::kBlocked;
  } else if (has_chrome_badge_) {
    badge_type = Badge::kChrome;
  }
#endif

  ApplyEffects(resource_size_in_dip_, resize_function_, app_launchable,
               /*rounded_corners=*/false, badge_type, &image_skia_);

  delegate_->OnIconUpdated(this);
}

void ChromeAppIcon::OnExtensionIconImageChanged(IconImage* icon) {
  DCHECK_EQ(icon_.get(), icon);
  UpdateIcon();
}

}  // namespace extensions

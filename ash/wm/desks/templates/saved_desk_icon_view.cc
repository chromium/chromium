// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/templates/saved_desk_icon_view.h"

#include <cstddef>
#include <utility>

#include "ash/public/cpp/rounded_image_view.h"
#include "ash/public/cpp/saved_desk_delegate.h"
#include "ash/shell.h"
#include "ash/style/ash_color_provider.h"
#include "base/check.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/native_theme/native_theme.h"
#include "ui/resources/grit/ui_resources.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/label.h"
#include "url/gurl.h"

namespace ash {

namespace {

// The size of the insets for the `count_label_`.
constexpr int kCountLabelInsetSize = 4;

// When fetching images from the app service, request one of size 64, which is a
// common cached image size. With a higher resolution it will look better after
// resizing.
constexpr int kAppIdImageSize = 64;

// Make the icons 27x27 so that they appear 26x26, to account for the
// invisible padding added when standardizing icons.
constexpr int kIconSize = 27;

// Size the default icon to 22x22 so that it appears 20x20 accounting for the
// invisible padding.
constexpr int kDefaultIconSize = 22;

// The size of the background the icon sits inside of.
constexpr int kIconViewSize = 28;

constexpr size_t kDefaultIconSortingKey = SIZE_MAX - 1;
constexpr size_t kOverflowIconSortingKey = SIZE_MAX;

// Return the formatted string for `count`. If `count` is <=99, the string will
// be "+<count>". If `count` is >99, the string will be "+99". If `show_plus` is
// false, the string will be just the count.
std::u16string GetCountString(int count, bool show_plus) {
  if (show_plus) {
    return base::UTF8ToUTF16(count > 99 ? "+99"
                                        : base::StringPrintf("+%i", count));
  }
  return base::NumberToString16(count);
}

gfx::ImageSkia CreateResizedImageToIconSize(const gfx::ImageSkia& icon,
                                            bool is_default) {
  const int diameter = is_default ? kDefaultIconSize : kIconSize;
  return gfx::ImageSkiaOperations::CreateResizedImage(
      icon, skia::ImageOperations::RESIZE_BEST, gfx::Size(diameter, diameter));
}

}  // namespace

// -----------------------------------------------------------------------------
// SavedDeskIconView:
SavedDeskIconView::SavedDeskIconView(int count, size_t sorting_key)
    : count_(count), sorting_key_(sorting_key) {}

SavedDeskIconView::~SavedDeskIconView() = default;

gfx::Size SavedDeskIconView::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  // The width for the icon. The overflow icon doesn't have an icon so it's
  // zero.
  int width = (IsOverflowIcon() ? 0 : kIconViewSize);

  if (count_label_) {
    views::SizeBound label_width = std::max<views::SizeBound>(
        kIconViewSize, available_size.width() - width);

    // Add the label width if the label view exists. The reason for having the
    // max is to have a minimum width.
    width += std::max(
        kIconViewSize,
        count_label_->CalculatePreferredSize(views::SizeBounds(label_width, {}))
            .width());
  }

  return gfx::Size(width, kIconViewSize);
}

void SavedDeskIconView::UpdateCount(int count) {
  // We should never get there. We only update `count_` for the overflow icon.
  // For the regular icon, `count_` remains unchanged after initializing it.
  NOTREACHED();
}

void SavedDeskIconView::CreateCountLabelChildView(bool show_plus,
                                                  int inset_size) {
  DCHECK(!count_label_);
  count_label_ =
      AddChildView(views::Builder<views::Label>()
                       .SetText(GetCountString(GetCountToShow(), show_plus))
                       .SetBorder(views::CreateEmptyBorder(gfx::Insets::TLBR(
                           kCountLabelInsetSize, kCountLabelInsetSize,
                           kCountLabelInsetSize, inset_size)))
                       .SetEnabledColorId(cros_tokens::kCrosSysSecondary)
                       .SetBackgroundColorId(cros_tokens::kCrosSysSystemOnBase)
                       .SetAutoColorReadabilityEnabled(false)
                       .Build());
}

BEGIN_METADATA(SavedDeskIconView)
END_METADATA

// -----------------------------------------------------------------------------
// SavedDeskRegularIconView:
SavedDeskRegularIconView::SavedDeskRegularIconView(
    const ui::ColorProvider* incognito_window_color_provider,
    const SavedDeskIconIdentifier& icon_identifier,
    const std::string& app_title,
    int count,
    size_t sorting_key,
    base::OnceCallback<void(views::View*)> on_icon_loaded)
    : SavedDeskIconView(count, sorting_key),
      icon_identifier_(icon_identifier),
      on_icon_loaded_(std::move(on_icon_loaded)) {
  if (GetCountToShow()) {
    SetBackground(views::CreateThemedRoundedRectBackground(
        cros_tokens::kCrosSysSystemOnBase,
        /*radius=*/kIconViewSize / 2.0f));
  }

  CreateChildViews(incognito_window_color_provider, app_title);
}

SavedDeskRegularIconView::~SavedDeskRegularIconView() = default;

void SavedDeskRegularIconView::Layout(PassKey) {
  DCHECK(icon_view_);
  gfx::Size icon_preferred_size = icon_view_->CalculatePreferredSize({});
  icon_view_->SetBoundsRect(gfx::Rect(
      base::ClampFloor((kIconViewSize - icon_preferred_size.width()) / 2.0),
      base::ClampFloor((kIconViewSize - icon_preferred_size.height()) / 2.0),
      icon_preferred_size.width(), icon_preferred_size.height()));

  if (count_label_) {
    count_label_->SetBoundsRect(
        gfx::Rect(kIconViewSize, 0, width() - kIconViewSize, kIconViewSize));
  }
}

void SavedDeskRegularIconView::OnThemeChanged() {
  SavedDeskIconView::OnThemeChanged();

  // The default icon is theme dependent, so it needs to be reloaded.
  if (is_showing_default_icon_)
    LoadDefaultIcon();
}

size_t SavedDeskRegularIconView::GetSortingKey() const {
  return is_showing_default_icon_ ? kDefaultIconSortingKey : sorting_key_;
}

int SavedDeskRegularIconView::GetCount() const {
  DCHECK(count_ >= 1);
  return count_;
}

int SavedDeskRegularIconView::GetCountToShow() const {
  DCHECK(count_ >= 1);
  return count_ - 1;
}

bool SavedDeskRegularIconView::IsOverflowIcon() const {
  return false;
}

void SavedDeskRegularIconView::CreateChildViews(
    const ui::ColorProvider* incognito_window_color_provider,
    const std::string& app_title) {
  if (GetCountToShow())
    CreateCountLabelChildView(/*show_plush*/ true, 2 * kCountLabelInsetSize);

  // Add the icon to the front so that it gets read out before `count_label_` by
  // spoken feedback.
  DCHECK(!icon_view_);
  icon_view_ = AddChildViewAt(views::Builder<RoundedImageView>()
                                  .SetCornerRadius(kIconSize / 2.0f)
                                  .Build(),
                              0);

  // First check if the `icon_identifier_` is a special value, i.e. NTP url or
  // incognito window. If it is, use the corresponding icon for the special
  // value.
  auto* delegate = Shell::Get()->saved_desk_delegate();
  std::optional<gfx::ImageSkia> chrome_icon =
      delegate->MaybeRetrieveIconForSpecialIdentifier(
          icon_identifier_.url_or_id, incognito_window_color_provider);

  icon_view_->GetViewAccessibility().SetRole(ax::mojom::Role::kImage);
  if (!app_title.empty())
    icon_view_->GetViewAccessibility().SetName(app_title,
                                               ax::mojom::NameFrom::kAttribute);

  // PWAs (e.g. Messages) should use icon identifier as they share the same app
  // id as Chrome and would return short name for app id as "Chromium" (see
  // https://crbug.com/1281394). This is unlike Chrome browser apps which should
  // use `app_id` as their icon identifiers have been stripped to avoid
  // duplicate favicons (see https://crbug.com/1281391).
  if (chrome_icon.has_value()) {
    icon_view_->SetImage(CreateResizedImageToIconSize(chrome_icon.value(),
                                                      /*is_default=*/false));
    return;
  }

  // It's not a special value so `icon_identifier_` is either a favicon or an
  // app id. If `icon_identifier_.url_or_id` is not a valid url then it's an app
  // id.
  GURL potential_url{icon_identifier_.url_or_id};
  if (!potential_url.is_valid()) {
    delegate->GetIconForAppId(
        icon_identifier_.url_or_id, kAppIdImageSize,
        base::BindOnce(&SavedDeskRegularIconView::OnIconLoaded,
                       weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  delegate->GetFaviconForUrl(
      icon_identifier_.url_or_id, icon_identifier_.lacros_profile_id,
      base::BindOnce(&SavedDeskRegularIconView::OnIconLoaded,
                     weak_ptr_factory_.GetWeakPtr()),
      &cancelable_task_tracker_);
}

void SavedDeskRegularIconView::OnIconLoaded(const gfx::ImageSkia& icon) {
  if (!icon.isNull()) {
    icon_view_->SetImage(
        CreateResizedImageToIconSize(icon, /*is_default=*/false));
    return;
  }

  LoadDefaultIcon();

  // Notify the icon container to update the icon order and visibility.
  if (parent() && on_icon_loaded_)
    std::move(on_icon_loaded_).Run(this);
}

void SavedDeskRegularIconView::LoadDefaultIcon() {
  is_showing_default_icon_ = true;

  const ui::NativeTheme* native_theme =
      ui::NativeTheme::GetInstanceForNativeUi();
  // Use a higher resolution image as it will look better after resizing.
  const int resource_id = native_theme && native_theme->ShouldUseDarkColors()
                              ? IDR_DEFAULT_FAVICON_DARK_64
                              : IDR_DEFAULT_FAVICON_64;

  // `color_provider` only exist when view is created, otherwise it will be a
  // nullptr. This will be called on `OnThemeChanged` again to ensure `SetImage`
  // is done.
  if (auto* color_provider = GetColorProvider()) {
    icon_view_->SetImage(CreateResizedImageToIconSize(
        gfx::ImageSkiaOperations::CreateColorMask(
            ui::ResourceBundle::GetSharedInstance()
                .GetImageNamed(resource_id)
                .AsImageSkia(),
            color_provider->GetColor(cros_tokens::kCrosSysOnSurface)),
        /*is_default=*/true));
  }
}

BEGIN_METADATA(SavedDeskRegularIconView)
END_METADATA

// -----------------------------------------------------------------------------
// SavedDeskOverflowIconView:
SavedDeskOverflowIconView::SavedDeskOverflowIconView(int count, bool show_plus)
    : SavedDeskIconView(count, kOverflowIconSortingKey) {
  SetBackground(views::CreateThemedRoundedRectBackground(
      cros_tokens::kCrosSysSystemOnBase,
      /*radius=*/kIconViewSize / 2.0f));

  CreateCountLabelChildView(show_plus, kCountLabelInsetSize);
}

SavedDeskOverflowIconView::~SavedDeskOverflowIconView() = default;

void SavedDeskOverflowIconView::Layout(PassKey) {
  DCHECK(count_label_);
  count_label_->SetBoundsRect(gfx::Rect(0, 0, width(), kIconViewSize));
}

void SavedDeskOverflowIconView::UpdateCount(int count) {
  DCHECK(count_label_);
  count_ = count;
  count_label_->SetText(GetCountString(GetCountToShow(), /*show_plus=*/true));
}

size_t SavedDeskOverflowIconView::GetSortingKey() const {
  return kOverflowIconSortingKey;
}

int SavedDeskOverflowIconView::GetCount() const {
  DCHECK(count_ >= 0);
  return count_;
}

int SavedDeskOverflowIconView::GetCountToShow() const {
  DCHECK(count_ >= 0);
  return count_;
}

bool SavedDeskOverflowIconView::IsOverflowIcon() const {
  return true;
}

BEGIN_METADATA(SavedDeskOverflowIconView)
END_METADATA

}  // namespace ash

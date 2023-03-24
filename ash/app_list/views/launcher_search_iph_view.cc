// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/launcher_search_iph_view.h"

#include <memory>
#include <utility>

#include "ash/public/cpp/app_list/app_list_client.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"

namespace ash {
namespace {
constexpr int kVerticalInset = 20;
constexpr int kHorizontalInset = 24;

constexpr int kTitleTextFontSize = 20;
constexpr int kDescriptionTextFontSize = 16;

constexpr char16_t kTitleTextPlaceholder[] = u"Title text";
constexpr char16_t kDescriptionTextPlaceholder[] = u"Description text";
}  // namespace

LauncherSearchIphView::LauncherSearchIphView(
    std::unique_ptr<ScopedIphSession> scoped_iph_session)
    : scoped_iph_session_(std::move(scoped_iph_session)) {
  SetID(kViewId);

  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical,
      gfx::Insets::VH(kVerticalInset, kHorizontalInset)));

  raw_ptr<views::Label> title_label =
      AddChildView(std::make_unique<views::Label>(kTitleTextPlaceholder));
  title_label->SetFontList(gfx::FontList().DeriveWithSizeDelta(
      kTitleTextFontSize - gfx::FontList().GetFontSize()));
  title_label->SetLineHeight(kTitleTextFontSize);
  title_label->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_TO_HEAD);

  raw_ptr<views::Label> description_label =
      AddChildView(std::make_unique<views::Label>(kDescriptionTextPlaceholder));
  description_label->SetFontList(gfx::FontList().DeriveWithSizeDelta(
      kDescriptionTextFontSize - gfx::FontList().GetFontSize()));
  description_label->SetLineHeight(kDescriptionTextFontSize);
  description_label->SetHorizontalAlignment(
      gfx::HorizontalAlignment::ALIGN_TO_HEAD);
}

LauncherSearchIphView::~LauncherSearchIphView() = default;

}  // namespace ash

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_restore/informed_restore_dialog.h"

#include <string>

#include "ash/public/cpp/saved_desk_delegate.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/wm/desks/desks_util.h"
#include "components/account_id/account_id.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/cpp/app_registry_cache_wrapper.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/table_layout_view.h"

namespace ash {

namespace {

// TODO(sammiequon|zxdan): Match specs.
constexpr gfx::Size kItemIconPreferredSize(30, 30);
constexpr gfx::Size kItemPreferredSize(160, 100);
constexpr int kSettingsIconSize = 24;
constexpr int kTableNumColumns = 3;
constexpr int kTablePaddingDp = 8;

constexpr int kAppIdImageSize = 64;

}  // namespace

// Represents an app that will be restored by full restore. Contains the app
// title and app icon.
// TODO(sammiequon|zxdan): Match specs.
class InformedRestoreItemView : public views::BoxLayoutView {
 public:
  METADATA_HEADER(InformedRestoreItemView);

  explicit InformedRestoreItemView(const std::string& app_title) {
    SetBackground(views::CreateSolidBackground(SK_ColorBLACK));
    SetCrossAxisAlignment(views::BoxLayout::CrossAxisAlignment::kStart);
    SetOrientation(views::BoxLayout::Orientation::kVertical);
    SetPreferredSize(kItemPreferredSize);

    AddChildView(views::Builder<views::ImageView>()
                     .CopyAddressTo(&image_view_)
                     .SetImageSize(kItemIconPreferredSize)
                     .Build());
    AddChildView(views::Builder<views::Label>()
                     .SetEnabledColor(SK_ColorWHITE)
                     .SetFontList(gfx::FontList({"Roboto"}, gfx::Font::NORMAL,
                                                14, gfx::Font::Weight::NORMAL))
                     .SetHorizontalAlignment(gfx::ALIGN_LEFT)
                     .SetText(base::ASCIIToUTF16(app_title))
                     .Build());
  }
  InformedRestoreItemView(const InformedRestoreItemView&) = delete;
  InformedRestoreItemView& operator=(const InformedRestoreItemView&) = delete;
  ~InformedRestoreItemView() override = default;

  views::ImageView* image_view() { return image_view_; }

  base::WeakPtr<InformedRestoreItemView> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  raw_ptr<views::ImageView> image_view_;

  base::WeakPtrFactory<InformedRestoreItemView> weak_ptr_factory_{this};
};

BEGIN_METADATA(InformedRestoreItemView, views::BoxLayoutView)
END_METADATA

// The contents of the informed restore dialog. It is a table that holds a
// couple `InformedItemView`'s. One item per window entry in the full restore
// file.
class InformedRestoreContentsView : public views::TableLayoutView {
 public:
  METADATA_HEADER(InformedRestoreContentsView);

  explicit InformedRestoreContentsView(
      const InformedRestoreDialog::AppIds& app_ids) {
    const int elements = static_cast<int>(app_ids.size());
    CHECK_GT(elements, 0);

    for (int i = 0; i < kTableNumColumns; ++i) {
      if (i != 0) {
        AddPaddingColumn(views::TableLayout::kFixedSize, kTablePaddingDp);
      }
      AddColumn(views::LayoutAlignment::kStart, views::LayoutAlignment::kStart,
                views::TableLayout::kFixedSize,
                views::TableLayout::ColumnSize::kUsePreferred, 0, 0);
    }

    // TODO(sammiequon|zxdan): Add a scroll view for if we have many items.
    for (int i = 0; i < elements; i += kTableNumColumns) {
      if (i != 0) {
        AddPaddingRow(views::TableLayout::kFixedSize, kTablePaddingDp);
      }
      AddRows(/*n=*/1, views::TableLayout::kFixedSize);
    }

    // TODO: Handle case where the app is not ready or installed.
    apps::AppRegistryCache* cache =
        apps::AppRegistryCacheWrapper::Get().GetAppRegistryCache(
            Shell::Get()->session_controller()->GetActiveAccountId());
    auto* delegate = Shell::Get()->saved_desk_delegate();
    for (const std::string& app_id : app_ids) {
      std::string title;
      // `cache` might be null in a test environment. In that case, we will use
      // an empty title.
      if (cache) {
        cache->ForOneApp(app_id, [&title](const apps::AppUpdate& update) {
          title = update.Name();
        });
      }

      auto item_view = std::make_unique<InformedRestoreItemView>(title);
      auto* item_view_ptr = item_view.get();
      AddChildView(std::move(item_view));

      // The callback may be called synchronously.
      delegate->GetIconForAppId(
          app_id, kAppIdImageSize,
          base::BindOnce(
              [](base::WeakPtr<InformedRestoreItemView> item_view,
                 const gfx::ImageSkia& icon) {
                if (item_view) {
                  item_view->image_view()->SetImage(
                      ui::ImageModel::FromImageSkia(icon));
                }
              },
              item_view_ptr->GetWeakPtr()));
    }
  }
  InformedRestoreContentsView(const InformedRestoreContentsView&) = delete;
  InformedRestoreContentsView& operator=(const InformedRestoreContentsView&) =
      delete;
  ~InformedRestoreContentsView() override = default;
};

BEGIN_METADATA(InformedRestoreContentsView, views::View)
END_METADATA

InformedRestoreDialog::~InformedRestoreDialog() = default;

// static
std::unique_ptr<views::Widget> InformedRestoreDialog::Create(
    aura::Window* root) {
  // TODO(sammiequon|zxdan): Remove this temporary data used for testing.
  AppIds kTestingAppsData = {
      "mgndgikekgjfcpckkfioiadnlibdjbkf",  // Chrome
      "njfbnohfdkmbmnjapinfcopialeghnmh",  // Camera
      "odknhmnlageboeamepcngndbggdpaobj",  // Settings
      "fkiggjmkendpmbegkagpmagjepfkpmeb",  // Files
      "nbljnnecbjbmifnoehiemkgefbnpoeak"   // Explore
  };

  views::Widget::InitParams params;
  params.delegate = new InformedRestoreDialog(kTestingAppsData);
  params.name = "InformedRestoreDialog";
  params.parent = desks_util::GetActiveDeskContainerForRoot(root);
  params.type = views::Widget::InitParams::TYPE_WINDOW_FRAMELESS;

  return std::make_unique<views::Widget>(std::move(params));
}

InformedRestoreDialog::InformedRestoreDialog(const AppIds& app_ids) {
  // TODO(sammiequon|zxdan): Localize all these strings.
  views::Builder<SystemDialogDelegateView>(this)
      .SetAcceptButtonText(u"Restore")
      .SetCancelButtonText(u"No Thanks")
      .SetDescription(u"Continue where you left off?")
      .SetModalType(ui::ModalType::MODAL_TYPE_SYSTEM)
      .SetTitleText(u"Welcome Back")
      .SetAdditionalViewInButtonRow(
          views::Builder<views::ImageButton>(
              views::CreateVectorImageButtonWithNativeTheme(
                  views::Button::PressedCallback(), kSettingsIcon,
                  kSettingsIconSize))
              .SetTooltipText(u"Settings"))
      .BuildChildren();

  SetMiddleContentView(std::make_unique<InformedRestoreContentsView>(app_ids));
}

BEGIN_METADATA(InformedRestoreDialog, SystemDialogDelegateView)
END_METADATA

}  // namespace ash

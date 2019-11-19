// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/locale/locale_detailed_view.h"

#include "ash/public/cpp/system_tray_client.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/style/default_color_constants.h"
#include "ash/system/model/locale_model.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/tray/actionable_view.h"
#include "ash/system/tray/tray_popup_item_style.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "base/i18n/case_conversion.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/layout/fill_layout.h"

namespace ash {

namespace {

// The item that corresponds to a single locale in the locale list. The language
// portion of |iso_code| is shown at the beginning of the row, and
// |display_name| is shown in the middle. A checkmark is shown in the end if
// |checked| is true.
class LocaleItem : public ActionableView {
 public:
  LocaleItem(tray::LocaleDetailedView* locale_detailed_view,
             const std::string& iso_code,
             const base::string16& display_name,
             bool checked)
      : ActionableView(TrayPopupInkDropStyle::FILL_BOUNDS),
        locale_detailed_view_(locale_detailed_view),
        checked_(checked) {
    SetInkDropMode(InkDropMode::ON);

    TriView* tri_view = TrayPopupUtils::CreateDefaultRowView();
    AddChildView(tri_view);
    SetLayoutManager(std::make_unique<views::FillLayout>());

    views::Label* iso_code_label = TrayPopupUtils::CreateDefaultLabel();
    iso_code_label->SetEnabledColor(
        AshColorProvider::Get()->DeprecatedGetContentLayerColor(
            AshColorProvider::ContentLayerType::kTextPrimary,
            kUnifiedMenuTextColor));
    iso_code_label->SetAutoColorReadabilityEnabled(false);
    iso_code_label->SetText(base::i18n::ToUpper(
        base::UTF8ToUTF16(l10n_util::GetLanguage(iso_code))));
    ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
    const gfx::FontList& base_font_list =
        rb.GetFontList(ui::ResourceBundle::MediumBoldFont);
    iso_code_label->SetFontList(base_font_list);
    tri_view->AddView(TriView::Container::START, iso_code_label);

    auto* display_name_view = TrayPopupUtils::CreateDefaultLabel();
    display_name_view->SetText(display_name);
    TrayPopupItemStyle style(TrayPopupItemStyle::FontStyle::DETAILED_VIEW_LABEL,
                             true /* use_unified_theme */);
    style.SetupLabel(display_name_view);

    display_name_view->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    tri_view->AddView(TriView::Container::CENTER, display_name_view);

    if (checked_) {
      views::ImageView* checked_image = TrayPopupUtils::CreateMainImageView();
      checked_image->SetImage(gfx::CreateVectorIcon(
          kCheckCircleIcon, kMenuIconSize,
          AshColorProvider::Get()->DeprecatedGetContentLayerColor(
              AshColorProvider::ContentLayerType::kProminentIconButton,
              kProminentIconButtonColor)));
      tri_view->AddView(TriView::Container::END, checked_image);
    }
    SetAccessibleName(display_name_view->GetText());
  }

  ~LocaleItem() override = default;

  // ActionableView:
  bool PerformAction(const ui::Event& event) override {
    locale_detailed_view_->HandleViewClicked(this);
    return true;
  }

  // views::View:
  void OnFocus() override {
    ActionableView::OnFocus();
    ScrollViewToVisible();
  }

  const char* GetClassName() const override { return "LocaleItem"; }

  void GetAccessibleNodeData(ui::AXNodeData* node_data) override {
    ActionableView::GetAccessibleNodeData(node_data);
    node_data->role = ax::mojom::Role::kCheckBox;
    node_data->SetCheckedState(checked_ ? ax::mojom::CheckedState::kTrue
                                        : ax::mojom::CheckedState::kFalse);
  }

 private:
  tray::LocaleDetailedView* locale_detailed_view_;
  const bool checked_;

  DISALLOW_COPY_AND_ASSIGN(LocaleItem);
};

}  // namespace

namespace tray {

LocaleDetailedView::LocaleDetailedView(DetailedViewDelegate* delegate)
    : TrayDetailedView(delegate) {
  CreateItems();
}

LocaleDetailedView::~LocaleDetailedView() = default;

void LocaleDetailedView::CreateItems() {
  CreateTitleRow(IDS_ASH_STATUS_TRAY_LOCALE_TITLE);
  CreateScrollableList();

  const std::vector<LocaleInfo>& locales =
      Shell::Get()->system_tray_model()->locale()->locale_list();
  int id = 0;
  for (auto& entry : locales) {
    const bool checked =
        entry.iso_code ==
        Shell::Get()->system_tray_model()->locale()->current_locale_iso_code();
    LocaleItem* item =
        new LocaleItem(this, entry.iso_code, entry.display_name, checked);
    scroll_content()->AddChildView(item);
    item->SetID(id);
    id_to_locale_[id] = entry.iso_code;
    ++id;
  }
  Layout();
}

void LocaleDetailedView::HandleViewClicked(views::View* view) {
  auto it = id_to_locale_.find(view->GetID());
  DCHECK(it != id_to_locale_.end());
  const std::string locale_iso_code = it->second;
  if (locale_iso_code !=
      Shell::Get()->system_tray_model()->locale()->current_locale_iso_code()) {
    Shell::Get()->system_tray_model()->client()->SetLocaleAndExit(
        locale_iso_code);
  }
}

const char* LocaleDetailedView::GetClassName() const {
  return "LocaleDetailedView";
}

}  // namespace tray
}  // namespace ash

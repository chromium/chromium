// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/locale/locale_detailed_view.h"

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/system_tray_client.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/rounded_container.h"
#include "ash/style/typography.h"
#include "ash/system/model/locale_model.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/tray/actionable_view.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "base/check.h"
#include "base/i18n/case_conversion.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/constants/chromeos_features.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/animation/ink_drop.h"
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
class LocaleItemView : public ActionableView {
 public:
  METADATA_HEADER(LocaleItemView);

  LocaleItemView(LocaleDetailedView* locale_detailed_view,
                 const std::string& iso_code,
                 const std::u16string& display_name,
                 bool checked)
      : ActionableView(TrayPopupInkDropStyle::FILL_BOUNDS),
        locale_detailed_view_(locale_detailed_view),
        checked_(checked) {
    views::InkDrop::Get(this)->SetMode(views::InkDropHost::InkDropMode::ON);

    TriView* tri_view = TrayPopupUtils::CreateDefaultRowView(
        /*use_wide_layout=*/false);
    AddChildView(tri_view);
    SetLayoutManager(std::make_unique<views::FillLayout>());

    const bool is_jelly_enabled = chromeos::features::IsJellyEnabled();
    views::Label* iso_code_label = TrayPopupUtils::CreateDefaultLabel();
    iso_code_label->SetEnabledColorId(
        is_jelly_enabled
            ? static_cast<ui::ColorId>(cros_tokens::kCrosSysOnSurface)
            : kColorAshTextColorPrimary);
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
    if (is_jelly_enabled) {
      display_name_view->SetEnabledColorId(cros_tokens::kCrosSysOnSurface);
      TypographyProvider::Get()->StyleLabel(TypographyToken::kCrosButton2,
                                            *display_name_view);
    } else {
      display_name_view->SetEnabledColorId(kColorAshTextColorPrimary);
      TrayPopupUtils::SetLabelFontList(
          display_name_view, TrayPopupUtils::FontStyle::kDetailedViewLabel);
    }
    display_name_view->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    tri_view->AddView(TriView::Container::CENTER, display_name_view);

    if (checked_) {
      views::ImageView* checked_image = TrayPopupUtils::CreateMainImageView(
          /*use_wide_layout=*/false);
      checked_image->SetImage(ui::ImageModel::FromVectorIcon(
          kCheckCircleIcon,
          is_jelly_enabled
              ? static_cast<ui::ColorId>(cros_tokens::kCrosSysPrimary)
              : kColorAshIconColorProminent,
          kMenuIconSize));
      tri_view->AddView(TriView::Container::END, checked_image);
    }
    SetAccessibleName(display_name_view->GetText());
  }
  LocaleItemView(const LocaleItemView&) = delete;
  LocaleItemView& operator=(const LocaleItemView&) = delete;
  ~LocaleItemView() override = default;

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

  void GetAccessibleNodeData(ui::AXNodeData* node_data) override {
    ActionableView::GetAccessibleNodeData(node_data);
    node_data->role = ax::mojom::Role::kCheckBox;
    node_data->SetCheckedState(checked_ ? ax::mojom::CheckedState::kTrue
                                        : ax::mojom::CheckedState::kFalse);
  }

 private:
  raw_ptr<LocaleDetailedView, ExperimentalAsh> locale_detailed_view_;
  const bool checked_;
};

BEGIN_METADATA(LocaleItemView, ActionableView)
END_METADATA

}  // namespace

LocaleDetailedView::LocaleDetailedView(DetailedViewDelegate* delegate)
    : TrayDetailedView(delegate) {
  CreateItems();
}

LocaleDetailedView::~LocaleDetailedView() = default;

void LocaleDetailedView::CreateItems() {
  CreateTitleRow(IDS_ASH_STATUS_TRAY_LOCALE_TITLE);
  CreateScrollableList();

  // Setup the container for the locale list views.
  views::View* container =
      features::IsQsRevampEnabled()
          ? scroll_content()->AddChildView(std::make_unique<RoundedContainer>())
          : scroll_content();

  const std::vector<LocaleInfo>& locales =
      Shell::Get()->system_tray_model()->locale()->locale_list();
  int id = 0;
  for (auto& entry : locales) {
    const bool checked =
        entry.iso_code ==
        Shell::Get()->system_tray_model()->locale()->current_locale_iso_code();
    auto* item =
        new LocaleItemView(this, entry.iso_code, entry.display_name, checked);
    container->AddChildView(item);
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

views::View* LocaleDetailedView::GetScrollContentForTest() {
  // Provide access to the protected scroll_content() in the base class.
  return scroll_content();
}

BEGIN_METADATA(LocaleDetailedView, TrayDetailedView)
END_METADATA

}  // namespace ash

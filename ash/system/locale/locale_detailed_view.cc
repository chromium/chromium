// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/locale/locale_detailed_view.h"

#include "ash/public/cpp/system_tray_client.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/rounded_container.h"
#include "ash/style/typography.h"
#include "ash/system/model/locale_model.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/i18n/case_conversion.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/accessibility/view_accessibility.h"
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
class LocaleItemView : public views::Button {
  METADATA_HEADER(LocaleItemView, views::Button)

 public:
  LocaleItemView(LocaleDetailedView* locale_detailed_view,
                 const std::string& iso_code,
                 const std::u16string& display_name,
                 bool checked)
      : locale_detailed_view_(locale_detailed_view), checked_(checked) {
    SetCallback(base::BindRepeating(&LocaleItemView::PerformAction,
                                    base::Unretained(this)));
    TrayPopupUtils::ConfigureRowButtonInkdrop(views::InkDrop::Get(this));

    TriView* tri_view = TrayPopupUtils::CreateDefaultRowView(
        /*use_wide_layout=*/false);
    AddChildView(tri_view);
    SetLayoutManager(std::make_unique<views::FillLayout>());

    views::Label* iso_code_label = TrayPopupUtils::CreateDefaultLabel();
    iso_code_label->SetEnabledColorId(
        static_cast<ui::ColorId>(cros_tokens::kCrosSysOnSurface));
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
    display_name_view->SetEnabledColorId(cros_tokens::kCrosSysOnSurface);
    TypographyProvider::Get()->StyleLabel(TypographyToken::kCrosButton2,
                                          *display_name_view);
    display_name_view->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    tri_view->AddView(TriView::Container::CENTER, display_name_view);

    if (checked_) {
      views::ImageView* checked_image = TrayPopupUtils::CreateMainImageView(
          /*use_wide_layout=*/false);
      checked_image->SetImage(ui::ImageModel::FromVectorIcon(
          kCheckCircleIcon,
          static_cast<ui::ColorId>(cros_tokens::kCrosSysPrimary),
          kMenuIconSize));
      tri_view->AddView(TriView::Container::END, checked_image);
    }
    GetViewAccessibility().SetName(display_name_view->GetText());
    GetViewAccessibility().SetRole(ax::mojom::Role::kCheckBox);
    GetViewAccessibility().SetCheckedState(
        checked_ ? ax::mojom::CheckedState::kTrue
                 : ax::mojom::CheckedState::kFalse);
  }
  LocaleItemView(const LocaleItemView&) = delete;
  LocaleItemView& operator=(const LocaleItemView&) = delete;
  ~LocaleItemView() override = default;

  void PerformAction(const ui::Event& event) {
    locale_detailed_view_->HandleViewClicked(this);
  }

  // views::View:
  void OnFocus() override {
    views::Button::OnFocus();
    ScrollViewToVisible();
  }

 private:
  raw_ptr<LocaleDetailedView> locale_detailed_view_;
  const bool checked_;
};

BEGIN_METADATA(LocaleItemView)
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
      scroll_content()->AddChildView(std::make_unique<RoundedContainer>());

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
  DeprecatedLayoutImmediately();
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

BEGIN_METADATA(LocaleDetailedView)
END_METADATA

}  // namespace ash

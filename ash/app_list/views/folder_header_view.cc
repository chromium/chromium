// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/folder_header_view.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "ash/app_list/app_list_util.h"
#include "ash/app_list/model/app_list_folder_item.h"
#include "ash/app_list/views/app_list_folder_view.h"
#include "ash/public/cpp/app_list/app_list_config.h"
#include "ash/public/cpp/app_list/app_list_features.h"
#include "ash/public/cpp/style/color_provider.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/system_textfield.h"
#include "ash/style/system_textfield_controller.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/constants/chromeos_features.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/text_elider.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/painter.h"
#include "ui/views/view.h"
#include "ui/views/view_targeter_delegate.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

// The max folder name length.
constexpr int kMaxFolderNameChars = 28;

// Folder header dimensions. The max header width is based on the width of a
// folder with 2 items.
constexpr int kMinFolderHeaderWidth = 24;
constexpr int kMaxFolderHeaderWidth = 168;
constexpr int kFolderHeaderHeight = 32;

// The min width of folder name - ensures the folder name is easily tappable.
constexpr int kFolderHeaderMinTapWidth = 32;

// The border radius for folder name.
constexpr int kFolderNameBorderRadius = 4;

// The border thickness for folder name.
constexpr int kFolderNameBorderThickness = 2;

// The inner padding for folder name.
constexpr int kFolderNamePadding = 8;

SkColor GetFolderBackgroundColor(bool is_active) {
  if (!is_active) {
    return SK_ColorTRANSPARENT;
  }

  const std::pair<SkColor, float> base_color_and_opacity =
      ash::ColorProvider::Get()->GetInkDropBaseColorAndOpacity();

  return SkColorSetA(base_color_and_opacity.first,
                     base_color_and_opacity.second * 255);
}

}  // namespace

class FolderHeaderView::FolderNameView : public views::Textfield,
                                         public views::ViewTargeterDelegate {
  METADATA_HEADER(FolderNameView, views::Textfield)

 public:
  explicit FolderNameView(FolderHeaderView* folder_header_view)
      : folder_header_view_(folder_header_view) {
    DCHECK(folder_header_view_);
    // Make folder name font size 14px.
    SetFontList(
        ui::ResourceBundle::GetSharedInstance().GetFontListWithDelta(2));

    SetEventTargeter(std::make_unique<views::ViewTargeter>(this));
    SetBorder(views::CreateEmptyBorder(gfx::Insets::VH(0, kFolderNamePadding)));
    views::FocusRing::Install(this);
    views::FocusRing::Get(this)->SetOutsetFocusRingDisabled(true);
    views::FocusRing::Get(this)->SetColorId(ui::kColorAshFocusRing);
    views::InstallRoundRectHighlightPathGenerator(this, gfx::Insets(),
                                                  kFolderNameBorderRadius);
  }

  FolderNameView(const FolderNameView&) = delete;
  FolderNameView& operator=(const FolderNameView&) = delete;

  ~FolderNameView() override = default;

  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override {
    return gfx::Size(kMaxFolderHeaderWidth, kFolderHeaderHeight);
  }

  void OnThemeChanged() override {
    Textfield::OnThemeChanged();

    const bool is_active = has_mouse_already_entered_ || HasFocus();
    SetBackground(views::CreateRoundedRectBackground(
        GetFolderBackgroundColor(is_active), kFolderNameBorderRadius,
        kFolderNameBorderThickness));

    const ui::ColorProvider* const color_provider = GetColorProvider();
    set_placeholder_text_color(
        color_provider->GetColor(kColorAshTextColorSecondary));
    const SkColor text_color =
        color_provider->GetColor(kColorAshTextColorPrimary);
    SetTextColor(text_color);
    SetSelectionTextColor(text_color);
    SetSelectionBackgroundColor(
        color_provider->GetColor(kColorAshFocusAuraColor));
    UpdateBackgroundColor(is_active);
  }

  ui::Cursor GetCursor(const ui::MouseEvent& event) override {
    return ui::mojom::CursorType::kIBeam;
  }

  void OnFocus() override {
    UpdateBackgroundColor(/*is_active=*/true);
    SetText(folder_header_view_->GetFolderName());
    starting_name_ = GetText();
    folder_header_view_->previous_folder_name_ = starting_name_;

    if (!defer_select_all_) {
      SelectAll(false);
    }

    Textfield::OnFocus();
  }

  void OnBlur() override {
    UpdateBackgroundColor(/*is_active=*/false);

    folder_header_view_->ContentsChanged(this, GetText());

    // Ensure folder name is truncated when FolderNameView loses focus.
    SetText(folder_header_view_->GetElidedFolderName());

    // Record metric each time a folder is renamed.
    if (GetText() != starting_name_) {
      if (folder_header_view_->is_tablet_mode()) {
        UMA_HISTOGRAM_COUNTS_100("Apps.AppListFolderNameLength.TabletMode",
                                 GetText().length());
      } else {
        UMA_HISTOGRAM_COUNTS_100("Apps.AppListFolderNameLength.ClamshellMode",
                                 GetText().length());
      }
    }

    defer_select_all_ = false;

    Textfield::OnBlur();
  }

  bool DoesMouseEventActuallyIntersect(const ui::MouseEvent& event) {
    // Since hitbox for this view is extended for tap, we need to manually
    // calculate this when checking for mouse events.
    return GetLocalBounds().Contains(event.location());
  }

  bool OnMousePressed(const ui::MouseEvent& event) override {
    // Since hovering changes the background color, only taps should be
    // triggered using the extended event target.
    if (!DoesMouseEventActuallyIntersect(event)) {
      return false;
    }

    if (!HasFocus()) {
      defer_select_all_ = true;
    }

    return Textfield::OnMousePressed(event);
  }

  void OnMouseExited(const ui::MouseEvent& event) override {
    if (!HasFocus()) {
      UpdateBackgroundColor(/*is_active=*/false);
    }

    has_mouse_already_entered_ = false;
  }

  void OnMouseMoved(const ui::MouseEvent& event) override {
    if (DoesMouseEventActuallyIntersect(event) && !has_mouse_already_entered_) {
      // If this is reached, the mouse is entering the view.
      // Recreate border to have custom corner radius.
      UpdateBackgroundColor(/*is_active=*/true);
      has_mouse_already_entered_ = true;
    } else if (!DoesMouseEventActuallyIntersect(event) &&
               has_mouse_already_entered_ && !HasFocus()) {
      // If this is reached, the mouse is exiting the view on its horizontal
      // edges.
      UpdateBackgroundColor(/*is_active=*/false);
      has_mouse_already_entered_ = false;
    }
  }

  void OnMouseReleased(const ui::MouseEvent& event) override {
    if (defer_select_all_) {
      defer_select_all_ = false;

      if (!HasSelection()) {
        SelectAll(false);
      }
    }

    Textfield::OnMouseReleased(event);
  }

  bool DoesIntersectRect(const views::View* target,
                         const gfx::Rect& rect) const override {
    DCHECK_EQ(target, this);
    gfx::Rect textfield_bounds = target->GetLocalBounds();

    // Ensure that the tap target for this view is always at least the view's
    // minimum width.
    int min_width =
        std::max(kFolderHeaderMinTapWidth, textfield_bounds.width());
    int horizontal_padding = -((min_width - textfield_bounds.width()) / 2);
    textfield_bounds.Inset(gfx::Insets::VH(0, horizontal_padding));

    return textfield_bounds.Intersects(rect);
  }

 private:
  void UpdateBackgroundColor(bool is_active) {
    background()->SetNativeControlColor(GetFolderBackgroundColor(is_active));
    SchedulePaint();
  }

  // The parent FolderHeaderView, owns this.
  const raw_ptr<FolderHeaderView> folder_header_view_;

  // Name of the folder when FolderNameView is focused, used to track folder
  // rename metric.
  std::u16string starting_name_;

  // If the view is focused via a mouse press event, then selection will be
  // cleared by its mouse release. To address this, defer selecting all
  // until we receive mouse release.
  bool defer_select_all_ = false;

  // Because of this view's custom event target, this view receives mouse enter
  // events in areas where the view isn't actually occupying. To check whether a
  // user has entered/exited this, we must check every mouse move event. This
  // bool tracks whether the mouse has entered the view, avoiding repainting the
  // background on each mouse move event.
  bool has_mouse_already_entered_ = false;
};

BEGIN_METADATA(FolderHeaderView, FolderNameView)
END_METADATA

class FolderHeaderView::FolderNameJellyView
    : public ash::SystemTextfield,
      public views::ViewTargeterDelegate {
  METADATA_HEADER(FolderNameJellyView, ash::SystemTextfield)

 public:
  explicit FolderNameJellyView(bool tablet_mode)
      : ash::SystemTextfield(ash::SystemTextfield::Type::kMedium),
        tablet_mode_(tablet_mode) {
    SetEventTargeter(std::make_unique<views::ViewTargeter>(this));
  }

  FolderNameJellyView(const FolderNameJellyView&) = delete;
  FolderNameJellyView& operator=(const FolderNameJellyView&) = delete;

  ~FolderNameJellyView() override = default;

  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override {
    return gfx::Size(kMaxFolderHeaderWidth, kFolderHeaderHeight);
  }

  ui::Cursor GetCursor(const ui::MouseEvent& event) override {
    return ui::mojom::CursorType::kIBeam;
  }

  void OnFocus() override {
    starting_name_ = GetText();
    SystemTextfield::OnFocus();
  }

  void OnBlur() override {
    // Record metric each time a folder is renamed.
    if (GetText() != starting_name_) {
      if (tablet_mode_) {
        UMA_HISTOGRAM_COUNTS_100("Apps.AppListFolderNameLength.TabletMode",
                                 GetText().length());
      } else {
        UMA_HISTOGRAM_COUNTS_100("Apps.AppListFolderNameLength.ClamshellMode",
                                 GetText().length());
      }
    }

    SystemTextfield::OnBlur();

    // OnBlur updates background ONLY if the ActiveState is changed. Since the
    // SystemTextField component does not clear focus after changing the
    // ActiveState, there are some instances where removing focus will not
    // trigger a background update.
    // TODO(b/323054951): Clean this code once the SystemTextfield has
    // implemented clearing focus.
    UpdateBackground();
  }

  bool DoesMouseEventActuallyIntersect(const ui::MouseEvent& event) {
    // Since hitbox for this view is extended for tap, we need to manually
    // calculate this when checking for mouse events.
    return GetLocalBounds().Contains(event.location());
  }

  bool DoesIntersectRect(const views::View* target,
                         const gfx::Rect& rect) const override {
    DCHECK_EQ(target, this);
    gfx::Rect textfield_bounds = target->GetLocalBounds();

    // Ensure that the tap target for this view is always at least the view's
    // minimum width.
    int min_width =
        std::max(kFolderHeaderMinTapWidth, textfield_bounds.width());
    int horizontal_padding = -((min_width - textfield_bounds.width()) / 2);
    textfield_bounds.Inset(gfx::Insets::VH(0, horizontal_padding));

    return textfield_bounds.Intersects(rect);
  }

 private:
  const bool tablet_mode_;

  // Name of the folder when FolderNameView is focused, used to track folder
  // rename metric.
  std::u16string starting_name_;
};

BEGIN_METADATA(FolderHeaderView, FolderNameJellyView)
END_METADATA

class FolderHeaderView::FolderNameViewController
    : public SystemTextfieldController {
 public:
  using ContentsChangedCallback =
      base::RepeatingCallback<void(const std::u16string& new_contents)>;
  FolderNameViewController(
      SystemTextfield* textfield,
      const ContentsChangedCallback& contents_changed_callback)
      : SystemTextfieldController(textfield),
        textfield_(textfield),
        contents_changed_callback_(contents_changed_callback) {}

  FolderNameViewController(const FolderNameViewController&) = delete;
  FolderNameViewController& operator=(const FolderNameViewController&) = delete;

  ~FolderNameViewController() override = default;

  // SystemTextfieldController:
  void ContentsChanged(views::Textfield* sender,
                       const std::u16string& new_contents) override {
    contents_changed_callback_.Run(new_contents);
  }
  bool HandleKeyEvent(views::Textfield* sender,
                      const ui::KeyEvent& key_event) override {
    if (SystemTextfieldController::HandleKeyEvent(sender, key_event)) {
      // TODO(b/323054951): Clean this code once the SystemTextfield has
      // implemented clearing focus.
      const bool should_clear_focus =
          key_event.type() == ui::EventType::kKeyPressed &&
          (key_event.key_code() == ui::VKEY_RETURN ||
           key_event.key_code() == ui::VKEY_ESCAPE);

      if (should_clear_focus) {
        textfield_->GetFocusManager()->ClearFocus();
      }
      return true;
    }

    if (IsUnhandledLeftRightKeyEvent(key_event)) {
      return ProcessLeftRightKeyTraversalForTextfield(sender, key_event);
    }

    return false;
  }

 private:
  raw_ptr<SystemTextfield> textfield_ = nullptr;
  const ContentsChangedCallback contents_changed_callback_;
};

FolderHeaderView::FolderHeaderView(FolderHeaderViewDelegate* delegate,
                                   bool tablet_mode)
    : folder_item_(nullptr),
      folder_name_placeholder_text_(
          ui::ResourceBundle::GetSharedInstance().GetLocalizedString(
              IDS_APP_LIST_FOLDER_NAME_PLACEHOLDER)),
      delegate_(delegate),
      folder_name_visible_(true),
      is_tablet_mode_(tablet_mode) {
  SystemTextfield* typed_folder_name_view =
      AddChildView(std::make_unique<FolderNameJellyView>(tablet_mode));
  folder_name_view_ = typed_folder_name_view;
  folder_name_controller_ = std::make_unique<FolderNameViewController>(
      typed_folder_name_view,
      base::BindRepeating(&FolderHeaderView::UpdateFolderName,
                          base::Unretained(this)));
  folder_name_view_->SetPlaceholderText(folder_name_placeholder_text_);

  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
}

FolderHeaderView::~FolderHeaderView() {
  if (folder_item_) {
    folder_item_->RemoveObserver(this);
  }
}

void FolderHeaderView::SetFolderItem(AppListFolderItem* folder_item) {
  if (folder_item_) {
    folder_item_->RemoveObserver(this);
  }

  folder_item_ = folder_item;
  if (!folder_item_) {
    return;
  }
  folder_item_->AddObserver(this);

  folder_name_view_->SetEnabled(folder_item_->folder_type() !=
                                AppListFolderItem::FOLDER_TYPE_OEM);

  Update();
}

void FolderHeaderView::UpdateFolderNameVisibility(bool visible) {
  folder_name_visible_ = visible;
  Update();
  SchedulePaint();
}

void FolderHeaderView::SetTextFocus() {
  folder_name_view_->RequestFocus();
}

bool FolderHeaderView::HasTextFocus() const {
  return folder_name_view_->HasFocus();
}

void FolderHeaderView::Update() {
  if (!folder_item_) {
    return;
  }

  folder_name_view_->SetVisible(folder_name_visible_);
  if (folder_name_visible_) {
    std::u16string elided_folder_name = GetElidedFolderName();
    folder_name_view_->SetText(elided_folder_name);
    UpdateFolderNameAccessibleName();
  }

  DeprecatedLayoutImmediately();
}

void FolderHeaderView::UpdateFolderNameAccessibleName() {
  // Sets |folder_name_view_|'s accessible name to the placeholder text if
  // |folder_name_view_| is blank; otherwise, clear the accessible name, the
  // accessible state's value is set to be folder_name_view_->GetText() by
  // TextField.
  std::u16string accessible_name = folder_name_view_->GetText().empty()
                                       ? folder_name_placeholder_text_
                                       : std::u16string();
  folder_name_view_->GetViewAccessibility().SetName(accessible_name);
}

const std::u16string& FolderHeaderView::GetFolderNameForTest() {
  return folder_name_view_->GetText();
}

void FolderHeaderView::SetFolderNameForTest(const std::u16string& name) {
  folder_name_view_->SetText(name);
}

bool FolderHeaderView::IsFolderNameEnabledForTest() const {
  return folder_name_view_->GetEnabled();
}

gfx::Size FolderHeaderView::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  return gfx::Size(kMaxFolderHeaderWidth,
                   folder_name_view_->GetPreferredSize().height());
}

void FolderHeaderView::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  Update();
}

views::Textfield* FolderHeaderView::GetFolderNameViewForTest() const {
  return folder_name_view_;
}

bool FolderHeaderView::IsFolderNameViewActiveForTest() const {
  ash::SystemTextfield* const as_system_textfield =
      views::AsViewClass<ash::SystemTextfield>(folder_name_view_);
  if (as_system_textfield) {
    return as_system_textfield->IsActive();
  }
  return folder_name_view_->HasFocus();
}

int FolderHeaderView::GetMaxFolderNameCharLengthForTest() const {
  return kMaxFolderNameChars;
}

std::u16string FolderHeaderView::GetFolderName() const {
  if (!folder_item_) {
    return std::u16string();
  }

  return base::UTF8ToUTF16(folder_item_->name());
}

std::u16string FolderHeaderView::GetElidedFolderName() const {
  if (!folder_item_) {
    return std::u16string();
  }

  // Enforce the maximum folder name length.
  std::u16string folder_name = GetFolderName();
  std::u16string name = folder_name.substr(0, kMaxFolderNameChars);

  // Get maximum text width for fitting into |folder_name_view_|.
  int text_width = std::min(kMaxFolderHeaderWidth, width()) -
                   folder_name_view_->GetCaretBounds().width() -
                   folder_name_view_->GetInsets().width();
  std::u16string elided_name = gfx::ElideText(
      name, folder_name_view_->GetFontList(), text_width, gfx::ELIDE_TAIL);
  return elided_name;
}

void FolderHeaderView::Layout(PassKey) {
  gfx::Rect rect(GetContentsBounds());
  if (rect.IsEmpty()) {
    return;
  }

  gfx::Rect text_bounds(rect);

  std::u16string text = folder_name_view_->GetText().empty()
                            ? folder_name_placeholder_text_
                            : folder_name_view_->GetText();
  int text_width =
      gfx::Canvas::GetStringWidth(text, folder_name_view_->GetFontList()) +
      folder_name_view_->GetCaretBounds().width() +
      folder_name_view_->GetInsets().width();
  text_width = std::min(text_width, kMaxFolderHeaderWidth);
  text_width = std::max(text_width, kMinFolderHeaderWidth);
  text_bounds.set_x(std::max(0, rect.x() + (rect.width() - text_width) / 2));
  text_bounds.set_width(std::min(rect.width(), text_width));

  text_bounds.ClampToCenteredSize(gfx::Size(
      text_bounds.width(), folder_name_view_->GetPreferredSize().height()));
  folder_name_view_->SetBoundsRect(text_bounds);
}

void FolderHeaderView::ContentsChanged(views::Textfield* sender,
                                       const std::u16string& new_contents) {
  UpdateFolderName(new_contents);
}

void FolderHeaderView::UpdateFolderName(
    const std::u16string& textfield_contents) {
  // Temporarily remove from observer to ignore data change caused by us.
  if (!folder_item_) {
    return;
  }

  folder_item_->RemoveObserver(this);

  std::u16string trimmed_name =
      base::CollapseWhitespace(textfield_contents, false);
  // Enforce the maximum folder name length in UI by trimming `new_contents`
  // when it is longer than the max length.
  if (trimmed_name.length() > kMaxFolderNameChars) {
    trimmed_name.resize(kMaxFolderNameChars);
    folder_name_view_->SetText(trimmed_name);
  } else {
    delegate_->SetItemName(folder_item_, base::UTF16ToUTF8(trimmed_name));
  }

  folder_item_->AddObserver(this);

  UpdateFolderNameAccessibleName();

  DeprecatedLayoutImmediately();
}

bool FolderHeaderView::ShouldNameViewClearFocus(const ui::KeyEvent& key_event) {
  return key_event.type() == ui::EventType::kKeyPressed &&
         (key_event.key_code() == ui::VKEY_RETURN ||
          key_event.key_code() == ui::VKEY_ESCAPE);
}

bool FolderHeaderView::HandleKeyEvent(views::Textfield* sender,
                                      const ui::KeyEvent& key_event) {
  if (ShouldNameViewClearFocus(key_event)) {
    // If the user presses the escape key, we should revert the text in
    // `folder_name_view_`.
    if (key_event.key_code() == ui::VKEY_ESCAPE) {
      sender->SetText(previous_folder_name_);
    }

    folder_name_view_->GetFocusManager()->ClearFocus();
    return true;
  }
  if (!IsUnhandledLeftRightKeyEvent(key_event)) {
    return false;
  }
  return ProcessLeftRightKeyTraversalForTextfield(folder_name_view_, key_event);
}

void FolderHeaderView::ItemNameChanged() {
  Update();
}

BEGIN_METADATA(FolderHeaderView)
END_METADATA

}  // namespace ash

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/data_controls/desktop_data_controls_dialog.h"

#include "chrome/grit/generated_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/gfx/color_palette.h"
#include "ui/views/border.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout_view.h"

namespace data_controls {

namespace {

constexpr int kSpacingBetweenIconAndMessage = 8;
constexpr int kBusinessIconSize = 16;

DesktopDataControlsDialog::TestObserver* observer_for_testing_ = nullptr;

}  // namespace

DesktopDataControlsDialog::TestObserver::TestObserver() {
  DesktopDataControlsDialog::SetObserverForTesting(this);
}

DesktopDataControlsDialog::TestObserver::~TestObserver() {
  DesktopDataControlsDialog::SetObserverForTesting(nullptr);
}

// static
void DesktopDataControlsDialog::SetObserverForTesting(TestObserver* observer) {
  // These checks add safety that tests are only setting one observer at a time.
  if (observer_for_testing_) {
    DCHECK_EQ(observer, nullptr);
  } else {
    DCHECK_NE(observer, nullptr);
  }

  observer_for_testing_ = observer;
}

void DesktopDataControlsDialog::Show(base::OnceClosure on_destructed) {
  on_destructed_ = std::move(on_destructed);
  constrained_window::ShowWebModalDialogViews(this, web_contents());
}

DesktopDataControlsDialog::~DesktopDataControlsDialog() {
  if (on_destructed_) {
    std::move(on_destructed_).Run();
  }
  if (observer_for_testing_) {
    observer_for_testing_->OnDestructed(this);
  }
}

std::u16string DesktopDataControlsDialog::GetWindowTitle() const {
  // TODO(b/351342878): Move this title string selection logic to common code as
  // needed.
  int id;
  switch (type_) {
    case Type::kClipboardPasteBlock:
      id = IDS_DATA_CONTROLS_CLIPBOARD_PASTE_BLOCK_TITLE;
      break;

    case Type::kClipboardCopyBlock:
      id = IDS_DATA_CONTROLS_CLIPBOARD_COPY_BLOCK_TITLE;
      break;

    case Type::kClipboardPasteWarn:
      id = IDS_DATA_CONTROLS_CLIPBOARD_PASTE_WARN_TITLE;
      break;

    case Type::kClipboardCopyWarn:
      id = IDS_DATA_CONTROLS_CLIPBOARD_COPY_WARN_TITLE;
      break;
  }
  return l10n_util::GetStringUTF16(id);
}

views::View* DesktopDataControlsDialog::GetContentsView() {
  if (!contents_view_) {
    contents_view_ = new views::BoxLayoutView();  // Owned by caller

    contents_view_->SetOrientation(views::BoxLayout::Orientation::kHorizontal);
    contents_view_->SetMainAxisAlignment(
        views::BoxLayout::MainAxisAlignment::kStart);
    contents_view_->SetCrossAxisAlignment(
        views::BoxLayout::CrossAxisAlignment::kCenter);
    contents_view_->SetBorder(views::CreateEmptyBorder(
        views::LayoutProvider::Get()->GetInsetsMetric(views::INSETS_DIALOG)));
    contents_view_->SetBetweenChildSpacing(kSpacingBetweenIconAndMessage);

    contents_view_->AddChildView(CreateEnterpriseIcon());
    contents_view_->AddChildView(CreateMessage());
  }
  return contents_view_;
}

views::Widget* DesktopDataControlsDialog::GetWidget() {
  return contents_view_->GetWidget();
}

ui::mojom::ModalType DesktopDataControlsDialog::GetModalType() const {
  return ui::mojom::ModalType::kChild;
}

bool DesktopDataControlsDialog::ShouldShowCloseButton() const {
  return false;
}

void DesktopDataControlsDialog::OnWidgetInitialized() {
  if (observer_for_testing_) {
    observer_for_testing_->OnWidgetInitialized(this);
  }
}

void DesktopDataControlsDialog::WebContentsDestroyed() {
  // If the WebContents the dialog is showing on gets destroyed, then the dialog
  // was neither bypassed or accepted so it should close without calling
  // any callback.
  ClearCallbacks();
  AcceptDialog();
}

void DesktopDataControlsDialog::PrimaryPageChanged(content::Page& page) {
  // If the primary page is changed, the triggered Data Controls rules that lead
  // to this current dialog showing are not necessarily still applicable. Data
  // shouldn't be allowed through since there might be higher severity rules
  // that trigger on the new page, so callbacks must be cleared before closing
  // the dialog.
  ClearCallbacks();
  AcceptDialog();
}

DesktopDataControlsDialog::DesktopDataControlsDialog(
    Type type,
    content::WebContents* contents,
    base::OnceCallback<void(bool bypassed)> callback)
    : DataControlsDialog(type, std::move(callback)),
      content::WebContentsObserver(contents) {
  SetOwnedByWidget(true);
  set_fixed_width(views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH));

  // TODO(b/351342878): Move shared logic for dialog button styling to
  // `DataControlsDialog`.
  // For warning dialogs, "cancel" means "ignore the warning and bypass" and
  // "accept" means "accept the warning and stop copying/pasting".
  switch (type_) {
    case Type::kClipboardPasteBlock:
    case Type::kClipboardCopyBlock:
      SetButtons(static_cast<int>(ui::mojom::DialogButton::kOk));
      SetButtonLabel(ui::mojom::DialogButton::kOk,
                     l10n_util::GetStringUTF16(IDS_OK));
      break;

    case Type::kClipboardPasteWarn:
      SetButtons(static_cast<int>(ui::mojom::DialogButton::kCancel) |
                 static_cast<int>(ui::mojom::DialogButton::kOk));

      SetButtonLabel(ui::mojom::DialogButton::kOk,
                     l10n_util::GetStringUTF16(
                         IDS_DATA_CONTROLS_PASTE_WARN_CANCEL_BUTTON));

      SetButtonStyle(ui::mojom::DialogButton::kCancel, ui::ButtonStyle::kTonal);
      SetButtonLabel(ui::mojom::DialogButton::kCancel,
                     l10n_util::GetStringUTF16(
                         IDS_DATA_CONTROLS_PASTE_WARN_CONTINUE_BUTTON));
      break;

    case Type::kClipboardCopyWarn:
      SetButtons(static_cast<int>(ui::mojom::DialogButton::kCancel) |
                 static_cast<int>(ui::mojom::DialogButton::kOk));

      SetButtonLabel(
          ui::mojom::DialogButton::kOk,
          l10n_util::GetStringUTF16(IDS_DATA_CONTROLS_COPY_WARN_CANCEL_BUTTON));

      SetButtonStyle(ui::mojom::DialogButton::kCancel, ui::ButtonStyle::kTonal);
      SetButtonLabel(ui::mojom::DialogButton::kCancel,
                     l10n_util::GetStringUTF16(
                         IDS_DATA_CONTROLS_COPY_WARN_CONTINUE_BUTTON));
      break;
  }
  SetButtonStyle(ui::mojom::DialogButton::kOk, ui::ButtonStyle::kProminent);
  SetDefaultButton(static_cast<int>(ui::mojom::DialogButton::kOk));

  if (!callbacks_.empty()) {
    DCHECK(type_ == Type::kClipboardPasteWarn ||
           type_ == Type::kClipboardCopyWarn);
    SetAcceptCallback(
        base::BindOnce(&DesktopDataControlsDialog::OnDialogButtonClicked,
                       base::Unretained(this),
                       /*bypassed=*/false));
    SetCancelCallback(
        base::BindOnce(&DesktopDataControlsDialog::OnDialogButtonClicked,
                       base::Unretained(this),
                       /*bypassed=*/true));
  }

  if (observer_for_testing_) {
    observer_for_testing_->OnConstructed(this);
  }
}

std::unique_ptr<views::View> DesktopDataControlsDialog::CreateEnterpriseIcon()
    const {
  auto enterprise_icon = std::make_unique<views::ImageView>();
  enterprise_icon->SetImage(ui::ImageModel::FromVectorIcon(
      vector_icons::kBusinessIcon, ui::kColorSysOnSurfaceSubtle,
      kBusinessIconSize));
  return enterprise_icon;
}

std::unique_ptr<views::Label> DesktopDataControlsDialog::CreateMessage() const {
  int id;
  switch (type_) {
    case Type::kClipboardPasteBlock:
    case Type::kClipboardCopyBlock:
      id = IDS_DATA_CONTROLS_BLOCKED_LABEL;
      break;
    case Type::kClipboardPasteWarn:
    case Type::kClipboardCopyWarn:
      id = IDS_DATA_CONTROLS_WARNED_LABEL;
      break;
  }
  return std::make_unique<views::Label>(l10n_util::GetStringUTF16(id));
}

}  // namespace data_controls

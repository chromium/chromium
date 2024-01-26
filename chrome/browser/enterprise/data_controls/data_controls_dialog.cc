// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/data_controls/data_controls_dialog.h"

#include "chrome/grit/generated_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"
#include "ui/gfx/color_palette.h"
#include "ui/views/border.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout_view.h"

namespace data_controls {

namespace {

constexpr int kSpacingBetweenIconAndMessage = 16;
constexpr int kBusinessIconSize = 24;

DataControlsDialog::TestObserver* observer_for_testing_ = nullptr;

}  // namespace

DataControlsDialog::TestObserver::TestObserver() {
  DataControlsDialog::SetObserverForTesting(this);
}

DataControlsDialog::TestObserver::~TestObserver() {
  DataControlsDialog::SetObserverForTesting(nullptr);
}

// static
void DataControlsDialog::SetObserverForTesting(TestObserver* observer) {
  // These checks add safety that tests are only setting one observer at a time.
  if (observer_for_testing_) {
    DCHECK_EQ(observer, nullptr);
  } else {
    DCHECK_NE(observer, nullptr);
  }

  observer_for_testing_ = observer;
}

// static
void DataControlsDialog::Show(
    content::WebContents* web_contents,
    Type type,
    base::OnceCallback<void(bool bypassed)> callback) {
  DCHECK(web_contents);
  constrained_window::ShowWebModalDialogViews(
      new DataControlsDialog(type, std::move(callback)), web_contents);
}

DataControlsDialog::~DataControlsDialog() {
  if (observer_for_testing_) {
    observer_for_testing_->OnDestructed(this);
  }
}

std::u16string DataControlsDialog::GetWindowTitle() const {
  int id;
  switch (type_) {
    case Type::kClipboardPasteBlock:
      id = IDS_DATA_CONTROLS_CLIPBOARD_PASTE_BLOCK_TITLE;
      break;

    case Type::kClipboardCopyBlock:
      id = IDS_DATA_CONTROLS_CLIPBOARD_COPY_BLOCK_TITLE;
      break;
      // TODO(domfc): Add text for other values.
      // case Type::kClipboardPasteWarn:
      // case Type::kClipboardCopyWarn:
  }
  return l10n_util::GetStringUTF16(id);
}

views::View* DataControlsDialog::GetContentsView() {
  if (!contents_view_) {
    contents_view_ = new views::BoxLayoutView();  // Owned by caller

    contents_view_->SetOrientation(views::BoxLayout::Orientation::kHorizontal);
    contents_view_->SetMainAxisAlignment(
        views::BoxLayout::MainAxisAlignment::kStart);
    contents_view_->SetCrossAxisAlignment(
        views::BoxLayout::CrossAxisAlignment::kStart);
    contents_view_->SetBorder(views::CreateEmptyBorder(
        views::LayoutProvider::Get()->GetInsetsMetric(views::INSETS_DIALOG)));
    contents_view_->SetBetweenChildSpacing(kSpacingBetweenIconAndMessage);

    contents_view_->AddChildView(CreateEnterpriseIcon());
    contents_view_->AddChildView(CreateMessage());
  }
  return contents_view_;
}

views::Widget* DataControlsDialog::GetWidget() {
  return contents_view_->GetWidget();
}

ui::ModalType DataControlsDialog::GetModalType() const {
  return ui::MODAL_TYPE_CHILD;
}

bool DataControlsDialog::ShouldShowCloseButton() const {
  return false;
}

void DataControlsDialog::OnWidgetInitialized() {
  if (observer_for_testing_) {
    observer_for_testing_->OnWidgetInitialized(this);
  }
}

DataControlsDialog::DataControlsDialog(
    Type type,
    base::OnceCallback<void(bool bypassed)> callback)
    : type_(type), callback_(std::move(callback)) {
  SetOwnedByWidget(true);

  switch (type_) {
    case Type::kClipboardPasteBlock:
    case Type::kClipboardCopyBlock:
      DialogDelegate::SetButtons(ui::DIALOG_BUTTON_CANCEL);
      DialogDelegate::SetButtonLabel(ui::DIALOG_BUTTON_CANCEL,
                                     l10n_util::GetStringUTF16(IDS_OK));
      break;
      // TODO(domfc): Add text for other values.
      // case Type::kClipboardPasteWarn:
      // case Type::kClipboardCopyWarn:
  }

  if (observer_for_testing_) {
    observer_for_testing_->OnConstructed(this);
  }
}

std::unique_ptr<views::View> DataControlsDialog::CreateEnterpriseIcon() const {
  auto enterprise_icon = std::make_unique<views::ImageView>();
  enterprise_icon->SetImage(ui::ImageModel::FromVectorIcon(
      vector_icons::kBusinessIcon, ui::kColorIcon, kBusinessIconSize));
  return enterprise_icon;
}

std::unique_ptr<views::Label> DataControlsDialog::CreateMessage() const {
  int id;
  switch (type_) {
    case Type::kClipboardPasteBlock:
    case Type::kClipboardCopyBlock:
      id = IDS_DATA_CONTROLS_BLOCKED_LABEL;
      break;
      // TODO(domfc): Add text for other values.
      // case Type::kClipboardPasteWarn:
      // case Type::kClipboardCopyWarn:
  }
  return std::make_unique<views::Label>(l10n_util::GetStringUTF16(id));
}

}  // namespace data_controls

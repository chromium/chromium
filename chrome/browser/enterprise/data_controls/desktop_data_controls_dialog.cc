// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/data_controls/desktop_data_controls_dialog.h"

#include "chrome/browser/ui/tabs/public/tab_dialog_manager.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/grit/generated_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/guest_view/browser/guest_view_base.h"
#include "components/strings/grit/components_strings.h"
#include "components/tabs/public/tab_interface.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/gfx/color_palette.h"
#include "ui/views/border.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout_view.h"

#if BUILDFLAG(ENABLE_GLIC)
#include "chrome/browser/glic/host/guest_util.h"
#endif  // BUILDFLAG(ENABLE_GLIC)

namespace data_controls {

namespace {

constexpr int kSpacingBetweenIconAndMessage = 8;
constexpr int kBusinessIconSize = 16;

DesktopDataControlsDialog::TestObserver* observer_for_testing_ = nullptr;

std::unique_ptr<views::View> CreateEnterpriseIcon() {
  auto enterprise_icon = std::make_unique<views::ImageView>();
  enterprise_icon->SetImage(ui::ImageModel::FromVectorIcon(
      vector_icons::kBusinessIcon, ui::kColorSysOnSurfaceSubtle,
      kBusinessIconSize));
  return enterprise_icon;
}

gfx::Rect GetDialogBounds(content::WebContents* contents,
                          const gfx::Rect& current_widget_bounds) {
  gfx::Rect rect = contents->GetContainerBounds();


  // This will show the dialog right above the top of the contents.
  rect.set_y(rect.y() - 40);
#if BUILDFLAG(ENABLE_GLIC)
  if (glic::IsGlicWebUI(contents)) {
    // This will show the dialog right below the "header" part of Glic.
    rect.set_y(rect.y() + 80);
  }
#endif  // BUILDFLAG(ENABLE_GLIC)

  rect.set_x(rect.x() + (rect.width() / 2) -
             (current_widget_bounds.width() / 2));

  return rect;
}

class DataControlsDialogDelegate : public views::DialogDelegate {
 public:
  explicit DataControlsDialogDelegate(DataControlsDialog::Type type,
                                      DesktopDataControlsDialog* dialog)
      : type_(type), dialog_(dialog) {
    set_fixed_width(views::LayoutProvider::Get()->GetDistanceMetric(
        views::DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH));
    // TODO(crbug.com/351342878): Move shared logic for dialog button styling to
    // `DataControlsDialog`.
    // For warning dialogs, "cancel" means "ignore the warning and bypass" and
    // "accept" means "accept the warning and stop copying/pasting".
    switch (type_) {
      case DataControlsDialog::Type::kClipboardPasteBlock:
      case DataControlsDialog::Type::kClipboardCopyBlock:
        SetButtons(static_cast<int>(ui::mojom::DialogButton::kOk));
        SetButtonLabel(ui::mojom::DialogButton::kOk,
                       l10n_util::GetStringUTF16(IDS_OK));
        break;

      case DataControlsDialog::Type::kClipboardPasteWarn:
        SetButtons(static_cast<int>(ui::mojom::DialogButton::kCancel) |
                   static_cast<int>(ui::mojom::DialogButton::kOk));

        SetButtonLabel(ui::mojom::DialogButton::kOk,
                       l10n_util::GetStringUTF16(
                           IDS_DATA_CONTROLS_PASTE_WARN_CANCEL_BUTTON));

        SetButtonStyle(ui::mojom::DialogButton::kCancel,
                       ui::ButtonStyle::kTonal);
        SetButtonLabel(ui::mojom::DialogButton::kCancel,
                       l10n_util::GetStringUTF16(
                           IDS_DATA_CONTROLS_PASTE_WARN_CONTINUE_BUTTON));
        break;

      case DataControlsDialog::Type::kClipboardCopyWarn:
        SetButtons(static_cast<int>(ui::mojom::DialogButton::kCancel) |
                   static_cast<int>(ui::mojom::DialogButton::kOk));

        SetButtonLabel(ui::mojom::DialogButton::kOk,
                       l10n_util::GetStringUTF16(
                           IDS_DATA_CONTROLS_COPY_WARN_CANCEL_BUTTON));

        SetButtonStyle(ui::mojom::DialogButton::kCancel,
                       ui::ButtonStyle::kTonal);
        SetButtonLabel(ui::mojom::DialogButton::kCancel,
                       l10n_util::GetStringUTF16(
                           IDS_DATA_CONTROLS_COPY_WARN_CONTINUE_BUTTON));
        break;
    }
    SetButtonStyle(ui::mojom::DialogButton::kOk, ui::ButtonStyle::kProminent);
    SetDefaultButton(static_cast<int>(ui::mojom::DialogButton::kOk));

    if (observer_for_testing_) {
      observer_for_testing_->OnConstructed(dialog_, this);
    }
  }

  ~DataControlsDialogDelegate() override {}

  std::u16string GetWindowTitle() const override {
    // TODO(crbug.com/351342878): Move this title string selection logic to
    // common code as needed.
    int id;
    switch (type_) {
      case DataControlsDialog::Type::kClipboardPasteBlock:
        id = IDS_DATA_CONTROLS_CLIPBOARD_PASTE_BLOCK_TITLE;
        break;

      case DataControlsDialog::Type::kClipboardCopyBlock:
        id = IDS_DATA_CONTROLS_CLIPBOARD_COPY_BLOCK_TITLE;
        break;

      case DataControlsDialog::Type::kClipboardPasteWarn:
        id = IDS_DATA_CONTROLS_CLIPBOARD_PASTE_WARN_TITLE;
        break;

      case DataControlsDialog::Type::kClipboardCopyWarn:
        id = IDS_DATA_CONTROLS_CLIPBOARD_COPY_WARN_TITLE;
        break;
    }
    return l10n_util::GetStringUTF16(id);
  }

  std::unique_ptr<views::Label> CreateMessage() const {
    int id;
    switch (type_) {
      case DataControlsDialog::Type::kClipboardPasteBlock:
      case DataControlsDialog::Type::kClipboardCopyBlock:
        id = IDS_DATA_CONTROLS_BLOCKED_LABEL;
        break;
      case DataControlsDialog::Type::kClipboardPasteWarn:
      case DataControlsDialog::Type::kClipboardCopyWarn:
        id = IDS_DATA_CONTROLS_WARNED_LABEL;
        break;
    }
    return std::make_unique<views::Label>(l10n_util::GetStringUTF16(id));
  }

  views::View* GetContentsView() override {
    if (!contents_view_) {
      contents_view_ = new views::BoxLayoutView();  // Owned by caller

      contents_view_->SetOrientation(
          views::BoxLayout::Orientation::kHorizontal);
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

  ui::mojom::ModalType GetModalType() const override {
    return ui::mojom::ModalType::kChild;
  }

  bool ShouldShowCloseButton() const override { return false; }

  void OnWidgetInitialized() override {
    if (observer_for_testing_) {
      observer_for_testing_->OnWidgetInitialized(dialog_, this);
    }
  }

  // Resets internal members to avoid dangling pointers. Only call this when the
  // owning widget is about to be destroyed.
  void Shutdown() {
    contents_view_ = nullptr;
    dialog_ = nullptr;
  }

 private:
  DataControlsDialog::Type type_;
  raw_ptr<views::BoxLayoutView> contents_view_ = nullptr;
  raw_ptr<DesktopDataControlsDialog> dialog_ = nullptr;
};

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

  content::WebContents* top_web_contents =
      guest_view::GuestViewBase::GetTopLevelWebContents(web_contents());

  dialog_delegate_ = std::make_unique<DataControlsDialogDelegate>(type_, this);
  dialog_delegate_->SetOwnershipOfNewWidget(
      views::Widget::InitParams::CLIENT_OWNS_WIDGET);

  widget_ = base::WrapUnique(views::DialogDelegate::CreateDialogWidget(
      dialog_delegate_.get(), gfx::NativeWindow(),
#if BUILDFLAG(IS_MAC)
      top_web_contents->GetNativeView()));
#else
      top_web_contents->GetTopLevelNativeWindow()));
#endif

  widget_->MakeCloseSynchronous(base::BindOnce(
      &DesktopDataControlsDialog::CloseDialog, base::Unretained(this)));
  widget_->SetBounds(
      GetDialogBounds(top_web_contents, widget_->GetWindowBoundsInScreen()));
  scoped_ignore_input_events_ =
      top_web_contents->IgnoreInputEvents(std::nullopt);

  constrained_window::ShowModalDialog(widget_->GetNativeWindow(),
                                      top_web_contents);
}

void DesktopDataControlsDialog::CloseDialog(
    views::Widget::ClosedReason reason) {
  if (reason == views::Widget::ClosedReason::kAcceptButtonClicked) {
    OnDialogButtonClicked(/*bypassed=*/false);
  }
  if (reason == views::Widget::ClosedReason::kCancelButtonClicked) {
    OnDialogButtonClicked(/*bypassed=*/true);
  }

  static_cast<DataControlsDialogDelegate*>(dialog_delegate_.get())->Shutdown();

  // The existing pattern is self-owned via
  // SetOwnedByWidget(OwnedByWidgetPassKey());, since the previous code was a
  // DialogDelegate owned by the widget.
  // In the new pattern, deleting this implicitly deletes all the scopers,
  // including the widget and the WebContents::ScopedIgnoreInputEvents.
  delete this;
}

DesktopDataControlsDialog::~DesktopDataControlsDialog() {
  if (on_destructed_) {
    std::move(on_destructed_).Run();
  }
  if (observer_for_testing_) {
    observer_for_testing_->OnDestructed(this);
  }
}

void DesktopDataControlsDialog::WebContentsDestroyed() {
  // If the WebContents the dialog is showing on gets destroyed, then the dialog
  // was neither bypassed or accepted so it should close without calling
  // any callback.
  ClearCallbacks();
  CloseDialog(views::Widget::ClosedReason::kAcceptButtonClicked);
}

void DesktopDataControlsDialog::PrimaryPageChanged(content::Page& page) {
  // If the primary page is changed, the triggered Data Controls rules that lead
  // to this current dialog showing are not necessarily still applicable. Data
  // shouldn't be allowed through since there might be higher severity rules
  // that trigger on the new page, so callbacks must be cleared before closing
  // the dialog.
  ClearCallbacks();
  CloseDialog(views::Widget::ClosedReason::kAcceptButtonClicked);
}

DesktopDataControlsDialog::DesktopDataControlsDialog(
    Type type,
    content::WebContents* contents,
    base::OnceCallback<void(bool bypassed)> callback)
    : DataControlsDialog(type, std::move(callback)),
      content::WebContentsObserver(contents) {
}

}  // namespace data_controls

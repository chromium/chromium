// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/devtools_policy_dialog.h"

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/notreached.h"
#include "build/build_config.h"
#include "chrome/browser/ui/dialogs/browser_dialogs.h"
#include "chrome/browser/ui/tab_modal_confirm_dialog.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/tab_modal_confirm_dialog_views.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "extensions/common/constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/views/bubble/bubble_dialog_model_host.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_delegate.h"

namespace {

#if BUILDFLAG(IS_LINUX)
constexpr int kIconPadding = 4;
#else
constexpr int kIconPadding = 2;
#endif
}

DevToolsPolicyDialog::DevToolsPolicyDialog(content::WebContents* web_contents) {
}

static DevToolsPolicyDialog::TestObserver* g_test_observer = nullptr;

DevToolsPolicyDialog::~DevToolsPolicyDialog() {
  if (g_test_observer) {
    g_test_observer->OnDialogDestroyed(this);
  }
}

// static
void DevToolsPolicyDialog::SetTestObserver(TestObserver* observer) {
  g_test_observer = observer;
}

// static
std::map<content::WebContents*, std::unique_ptr<DevToolsPolicyDialog>>&
DevToolsPolicyDialog::GetCurrentDialogs() {
  static base::NoDestructor<
      std::map<content::WebContents*, std::unique_ptr<DevToolsPolicyDialog>>>
      dialogs;
  return *dialogs;
}

// static
void DevToolsPolicyDialog::Show(content::WebContents* web_contents) {
  DCHECK(web_contents);
  if (GetCurrentDialogs().count(web_contents)) {
    return;
  }

  auto dialog_manager = base::WrapUnique<DevToolsPolicyDialog>(
      new DevToolsPolicyDialog(web_contents));
  GetCurrentDialogs()[web_contents] = std::move(dialog_manager);

  auto dialog_model =
      ui::DialogModel::Builder(std::make_unique<ui::DialogModelDelegate>())
          .SetTitle(l10n_util::GetStringUTF16(IDS_DEVTOOLS_NOT_ALLOWED))
          .AddCustomField(
              std::make_unique<views::BubbleDialogModelHost::CustomView>(
                  []() {
                    auto view = std::make_unique<views::View>();
                    auto* layout = view->SetLayoutManager(
                        std::make_unique<views::BoxLayout>());
                    layout->set_cross_axis_alignment(
                        views::BoxLayout::CrossAxisAlignment::kStart);

                    auto icon = std::make_unique<views::ImageView>();
                    icon->SetBorder(views::CreateEmptyBorder(
                        gfx::Insets::TLBR(kIconPadding, 0, 0, 0)));
                    icon->SetImage(ui::ImageModel::FromVectorIcon(
                        vector_icons::kBusinessIcon, ui::kColorIcon,
                        extension_misc::EXTENSION_ICON_BITTY));

                    auto label = std::make_unique<views::Label>(
                        l10n_util::GetStringUTF16(
                            IDS_DEVTOOLS_BLOCKED_BY_POLICY));
                    label->SetMultiLine(true);
                    label->SetHorizontalAlignment(gfx::ALIGN_LEFT);

                    int text_padding =
                        ChromeLayoutProvider::Get()->GetDistanceMetric(
                            views::DISTANCE_TEXTFIELD_HORIZONTAL_TEXT_PADDING);
                    layout->set_between_child_spacing(text_padding);

                    view->AddChildView(std::move(icon));
                    view->AddChildView(std::move(label));
                    return view;
                  }(),
                  views::BubbleDialogModelHost::FieldType::kControl))

          .AddOkButton(base::DoNothing(),
                       ui::DialogModel::Button::Params().SetLabel(
                           l10n_util::GetStringUTF16(IDS_OK)))
          .SetDialogDestroyingCallback(base::BindOnce(
              [](uintptr_t web_contents) {
                auto* key =
                    reinterpret_cast<content::WebContents*>(web_contents);

                DevToolsPolicyDialog::GetCurrentDialogs().erase(key);
              },
              reinterpret_cast<uintptr_t>(web_contents)))
          .Build();
  chrome::ShowTabModal(std::move(dialog_model), web_contents);

  if (g_test_observer) {
    g_test_observer->OnDialogShown(GetCurrentDialogs()[web_contents].get());
  }
}

// static
void DevToolsPolicyDialog::TestOnlyCloseDialog(
    content::WebContents* web_contents) {
#if !BUILDFLAG(IS_MAC)
  DCHECK(web_contents);

  if (!GetCurrentDialogs().count(web_contents)) {
    return;  // Dialog not recorded as open
  }

  gfx::NativeView top_level_view = web_contents->GetTopLevelNativeWindow();
  if (!top_level_view) {
    return;
  }

  views::Widget::Widgets child_widgets =
      views::Widget::GetAllChildWidgets(top_level_view);

  for (views::Widget* widget : child_widgets) {
    if (!widget || !widget->IsVisible() || !widget->widget_delegate()) {
      continue;
    }

    if (widget->widget_delegate()->GetWindowTitle() ==
        l10n_util::GetStringUTF16(IDS_DEVTOOLS_NOT_ALLOWED)) {
      views::DialogDelegate* dialog_delegate =
          widget->widget_delegate()->AsDialogDelegate();
      if (dialog_delegate) {
        dialog_delegate->GetWidget()->CloseNow();
        return;
      }
    }
  }
#endif
}

// static
size_t DevToolsPolicyDialog::GetCurrentDialogsSizeForTesting() {
  return GetCurrentDialogs().size();
}

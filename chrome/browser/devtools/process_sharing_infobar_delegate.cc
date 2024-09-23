// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/process_sharing_infobar_delegate.h"

#include "chrome/browser/about_flags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/grit/generated_resources.h"
#include "components/flags_ui/pref_service_flags_storage.h"
#include "components/infobars/core/infobar.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/window/dialog_delegate.h"

namespace {
std::unique_ptr<views::View> MakeRestartView() {
  auto view = std::make_unique<views::View>();

  views::LayoutProvider* provider = views::LayoutProvider::Get();
  view->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical,
      provider->GetInsetsMetric(views::InsetsMetric::INSETS_DIALOG),
      provider->GetDistanceMetric(views::DISTANCE_RELATED_CONTROL_VERTICAL)));

  const std::u16string message = l10n_util::GetStringUTF16(
      IDS_DEV_TOOLS_SHARED_PROCESS_INFOBAR_RESTART_NEEDED);
  views::Label* message_label = new views::Label(message);
  message_label->SetMultiLine(true);
  message_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  view->AddChildView(message_label);

  return view;
}

}  // namespace

ProcessSharingInfobarDelegate::ProcessSharingInfobarDelegate(
    content::WebContents* web_contents)
    : inspected_web_contents_(web_contents->GetWeakPtr()) {}

ProcessSharingInfobarDelegate::~ProcessSharingInfobarDelegate() = default;

std::u16string ProcessSharingInfobarDelegate::GetMessageText() const {
  return l10n_util::GetStringUTF16(IDS_DEV_TOOLS_SHARED_PROCESS_INFOBAR);
}

int ProcessSharingInfobarDelegate::GetButtons() const {
  return BUTTON_OK;
}

std::u16string ProcessSharingInfobarDelegate::GetButtonLabel(
    InfoBarButton button) const {
  DCHECK_EQ(BUTTON_OK, button);
  return l10n_util::GetStringUTF16(
      IDS_DEV_TOOLS_SHARED_PROCESS_INFOBAR_OPT_OUT);
}

std::u16string ProcessSharingInfobarDelegate::GetLinkText() const {
  return l10n_util::GetStringUTF16(
      IDS_DEV_TOOLS_SHARED_PROCESS_INFOBAR_LEARN_MORE);
}

GURL ProcessSharingInfobarDelegate::GetLinkURL() const {
  return GURL("https://developer.chrome.com/blog/process-sharing-experiment");
}

infobars::InfoBarDelegate::InfoBarIdentifier
ProcessSharingInfobarDelegate::GetIdentifier() const {
  return DEV_TOOLS_SHARED_PROCESS_DELEGATE;
}

bool ProcessSharingInfobarDelegate::Accept() {
  if (!inspected_web_contents_) {
    return true;
  }
  auto delegate = std::make_unique<views::DialogDelegate>();
  delegate->set_internal_name("ProcessSharingAppRestart");
  delegate->SetButtons(static_cast<int>(ui::mojom::DialogButton::kOk) |
                       static_cast<int>(ui::mojom::DialogButton::kCancel));
  delegate->SetContentsView(MakeRestartView());
  delegate->SetModalType(ui::mojom::ModalType::kSystem);
  delegate->SetOwnedByWidget(true);
  delegate->SetShowCloseButton(false);
  delegate->set_fixed_width(ChromeLayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH));
  delegate->SetAcceptCallback(base::BindOnce(
      [](base::WeakPtr<content::WebContents> inspected_web_contents) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
        PrefService* prefs = Profile::FromBrowserContext(
                                 inspected_web_contents->GetBrowserContext())
                                 ->GetPrefs();
#else
        PrefService* prefs = g_browser_process->local_state();
#endif
        //  Note: Both ChromeOS owner and non-owner use PrefServiceFlagsStorage
        //  under the hood. OwnersFlagsStorage has additional functionalities
        //  for setting flags but since we are just reading the storage assume
        //  non-owner case and bypass asynchronous owner check.
        auto flags_storage =
            std::make_unique<flags_ui::PrefServiceFlagsStorage>(prefs);

        about_flags::SetFeatureEntryEnabled(
            flags_storage.get(),
            "enable-process-per-site-up-to-main-frame-threshold@2", true);
        flags_storage->CommitPendingWrites();

        base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE, base::BindOnce(&chrome::AttemptRestart));
      },
      inspected_web_contents_));

  views::DialogDelegate::CreateDialogWidget(
      std::move(delegate), inspected_web_contents_->GetTopLevelNativeWindow(),
      nullptr)
      ->Show();

  return ConfirmInfoBarDelegate::Accept();
}

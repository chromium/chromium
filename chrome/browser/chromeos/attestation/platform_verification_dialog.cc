// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/attestation/platform_verification_dialog.h"

#include <stddef.h>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/singleton_tabs.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "ui/aura/window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/page_transition_types.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_delegate.h"

namespace {

std::unique_ptr<views::ImageButton> CreateLearnMoreButton(
    views::ButtonListener* listener) {
  auto learn_more_button = views::CreateVectorImageButton(listener);
  views::SetImageFromVectorIcon(learn_more_button.get(),
                                vector_icons::kHelpOutlineIcon);
  learn_more_button->SetAccessibleName(
      l10n_util::GetStringUTF16(IDS_CHROMEOS_ACC_LEARN_MORE));
  learn_more_button->SetFocusForPlatform();
  return learn_more_button;
}

}  // namespace

namespace chromeos {
namespace attestation {

// static
views::Widget* PlatformVerificationDialog::ShowDialog(
    content::WebContents* web_contents,
    const GURL& requesting_origin,
    ConsentCallback callback) {
  // This could happen when the permission is requested from an extension. See
  // http://crbug.com/728534
  // TODO(wittman): Remove this check after ShowWebModalDialogViews() API is
  // improved/fixed. See http://crbug.com/733355
  if (!web_modal::WebContentsModalDialogManager::FromWebContents(
          web_contents)) {
    DVLOG(1) << "WebContentsModalDialogManager not registered for WebContents.";
    return nullptr;
  }

  // In the case of an extension or hosted app, the origin of the request is
  // best described by the extension / app name.
  const extensions::Extension* extension =
      extensions::ExtensionRegistry::Get(web_contents->GetBrowserContext())
          ->enabled_extensions()
          .GetExtensionOrAppByURL(web_contents->GetLastCommittedURL());

  // TODO(xhwang): We should only show the name if the request is from the
  // extension's true frame. See http://crbug.com/455821
  std::string origin = extension ? extension->name() : requesting_origin.spec();

  PlatformVerificationDialog* dialog = new PlatformVerificationDialog(
      web_contents, base::UTF8ToUTF16(origin), std::move(callback));

  return constrained_window::ShowWebModalDialogViews(dialog, web_contents);
}

PlatformVerificationDialog::~PlatformVerificationDialog() {
}

PlatformVerificationDialog::PlatformVerificationDialog(
    content::WebContents* web_contents,
    const base::string16& domain,
    ConsentCallback callback)
    : content::WebContentsObserver(web_contents),
      domain_(domain),
      callback_(std::move(callback)),
      learn_more_button_(nullptr) {
  DialogDelegate::set_button_label(
      ui::DIALOG_BUTTON_OK, l10n_util::GetStringUTF16(IDS_PERMISSION_ALLOW));
  DialogDelegate::set_button_label(
      ui::DIALOG_BUTTON_CANCEL, l10n_util::GetStringUTF16(IDS_PERMISSION_DENY));
  learn_more_button_ =
      DialogDelegate::SetExtraView(CreateLearnMoreButton(this));
  SetLayoutManager(std::make_unique<views::FillLayout>());
  SetBorder(views::CreateEmptyBorder(
      views::LayoutProvider::Get()->GetDialogInsetsForContentType(
          views::TEXT, views::TEXT)));

  // Explanation string.
  views::Label* label = new views::Label(l10n_util::GetStringFUTF16(
      IDS_PLATFORM_VERIFICATION_DIALOG_HEADLINE, domain_));
  label->SetMultiLine(true);
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  AddChildView(label);
  chrome::RecordDialogCreation(chrome::DialogIdentifier::PLATFORM_VERIFICATION);
}

bool PlatformVerificationDialog::Cancel() {
  // This method is called when user clicked "Block" button.
  std::move(callback_).Run(CONSENT_RESPONSE_DENY);
  return true;
}

bool PlatformVerificationDialog::Accept() {
  // This method is called when user clicked "Allow" button.
  std::move(callback_).Run(CONSENT_RESPONSE_ALLOW);
  return true;
}

bool PlatformVerificationDialog::Close() {
  // This method is called when user clicked "x" or pressed "Esc" to dismiss the
  // dialog, the permission request is canceled, or when the tab containing this
  // dialog is closed.
  std::move(callback_).Run(CONSENT_RESPONSE_NONE);
  return true;
}

ui::ModalType PlatformVerificationDialog::GetModalType() const {
  return ui::MODAL_TYPE_CHILD;
}

gfx::Size PlatformVerificationDialog::CalculatePreferredSize() const {
  const int default_width = views::LayoutProvider::Get()->GetDistanceMetric(
      DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH);
  return gfx::Size(
      default_width,
      GetLayoutManager()->GetPreferredHeightForWidth(this, default_width));
}

void PlatformVerificationDialog::ButtonPressed(views::Button* sender,
                                               const ui::Event& event) {
  if (sender != learn_more_button_)
    return;

  Browser* browser = chrome::FindBrowserWithWebContents(web_contents());
  const GURL learn_more_url(chrome::kEnhancedPlaybackNotificationLearnMoreURL);

  // |web_contents()| might not be in a browser in case of v2 apps. In that
  // case, open a new tab in the usual way.
  if (!browser) {
    Profile* profile =
        Profile::FromBrowserContext(web_contents()->GetBrowserContext());
    NavigateParams params(profile, learn_more_url, ui::PAGE_TRANSITION_LINK);
    params.disposition = WindowOpenDisposition::SINGLETON_TAB;
    Navigate(&params);
  } else {
    ShowSingletonTab(browser, learn_more_url);
  }
}

void PlatformVerificationDialog::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInMainFrame() ||
      navigation_handle->IsSameDocument())
    return;

  views::Widget* widget = GetWidget();
  if (widget)
    widget->Close();
}

}  // namespace attestation
}  // namespace chromeos

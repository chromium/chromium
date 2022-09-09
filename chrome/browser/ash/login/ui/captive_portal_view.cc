// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/ui/captive_portal_view.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ash/login/ui/captive_portal_window_proxy.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_state.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "components/captive_portal/core/captive_portal_detector.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/widget/widget_delegate.h"
#include "url/gurl.h"

namespace ash {
namespace {

const char* CaptivePortalStartURL() {
  return captive_portal::CaptivePortalDetector::kDefaultURL;
}

std::u16string WindowTitleForNetwork(const NetworkState* network) {
  if (network && !network->name().empty()) {
    return l10n_util::GetStringFUTF16(IDS_LOGIN_CAPTIVE_PORTAL_WINDOW_TITLE,
                                      base::ASCIIToUTF16(network->name()));
  } else {
    NOTREACHED() << "Captive portal with no active network?";
    return l10n_util::GetStringFUTF16(IDS_LOGIN_CAPTIVE_PORTAL_WINDOW_TITLE,
                                      {});
  }
}

}  // namespace

CaptivePortalView::CaptivePortalView(Profile* profile,
                                     CaptivePortalWindowProxy* proxy)
    : SimpleWebViewDialog(profile), proxy_(proxy), redirected_(false) {}

CaptivePortalView::~CaptivePortalView() {}

void CaptivePortalView::StartLoad() {
  SimpleWebViewDialog::StartLoad(GURL(CaptivePortalStartURL()));
}

void CaptivePortalView::NavigationStateChanged(
    content::WebContents* source,
    content::InvalidateTypes changed_flags) {
  SimpleWebViewDialog::NavigationStateChanged(source, changed_flags);

  // Naive way to determine the redirection. This won't be needed after portal
  // detection will be done on the Chrome side.
  GURL url = source->GetLastCommittedURL();
  // Note, `url` will be empty for "client3.google.com/generate_204" page.
  if (!redirected_ && url != GURL::EmptyGURL() &&
      url != GURL(CaptivePortalStartURL())) {
    redirected_ = true;
    proxy_->OnRedirected();
  }
}

void CaptivePortalView::LoadingStateChanged(content::WebContents* source,
                                            bool to_different_document) {
  SimpleWebViewDialog::LoadingStateChanged(source, to_different_document);
  // TODO(nkostylev): Fix case of no connectivity, check HTTP code returned.
  // Disable this heuristic as it has false positives.
  // Relying on just shill portal check to close dialog is fine.
  // if (!is_loading && !redirected_)
  //   proxy_->OnOriginalURLLoaded();
}

std::unique_ptr<views::WidgetDelegate> CaptivePortalView::MakeWidgetDelegate() {
  auto delegate = SimpleWebViewDialog::MakeWidgetDelegate();
  delegate->SetCanResize(false);
  delegate->SetModalType(ui::MODAL_TYPE_SYSTEM);
  delegate->SetShowTitle(true);
  delegate->SetTitle(WindowTitleForNetwork(
      NetworkHandler::Get()->network_state_handler()->DefaultNetwork()));
  return delegate;
}

}  // namespace ash

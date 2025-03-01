// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/glic_fre_dialog_view.h"

#include <memory>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/view_class_properties.h"

namespace glic {

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(GlicFreDialogView,
                                      kWebViewElementIdForTesting);

GlicFreDialogView::GlicFreDialogView(Profile* profile,
                                     const gfx::Size& initial_size) {
  SetShowCloseButton(false);
  SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));
  SetModalType(ui::mojom::ModalType::kChild);
  SetOwnedByWidget(false);
  SetOwnershipOfNewWidget(
      views::Widget::InitParams::Ownership::CLIENT_OWNS_WIDGET);
  // TODO(cuianthony): Share this constant in GlicWindowController to use with
  // both the FRE dialog and the main glic window.
  set_corner_radius(12);

  SetUseDefaultFillLayout(true);

  auto web_view = std::make_unique<views::WebView>(profile);
  web_view->SetProperty(views::kElementIdentifierKey,
                        kWebViewElementIdForTesting);
  web_view->SetSize(initial_size);
  web_view->SetPreferredSize(initial_size);

  web_contents_ =
      content::WebContents::Create(content::WebContents::CreateParams(profile));
  DCHECK(web_contents_);
  web_contents_->SetPageBaseBackgroundColor(SK_ColorTRANSPARENT);
  web_contents_->GetController().LoadURLWithParams(
      content::NavigationController::LoadURLParams(
          GURL{chrome::kChromeUIGlicFreURL}));
  web_view->SetWebContents(web_contents_.get());

  AddChildView(std::move(web_view));
}

GlicFreDialogView::~GlicFreDialogView() = default;

}  // namespace glic

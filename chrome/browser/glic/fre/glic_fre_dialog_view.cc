// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/fre/glic_fre_dialog_view.h"

#include <memory>

#include "chrome/browser/glic/fre/glic_fre_controller.h"
#include "chrome/browser/glic/widget/glic_view.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/view_class_properties.h"

namespace glic {

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(GlicFreDialogView,
                                      kWebViewElementIdForTesting);

GlicFreDialogView::GlicFreDialogView(Profile* profile,
                                     GlicFreController* fre_controller) {
  SetShowCloseButton(false);
  SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));
  SetModalType(ui::mojom::ModalType::kChild);
  SetOwnershipOfNewWidget(
      views::Widget::InitParams::Ownership::CLIENT_OWNS_WIDGET);
  // TODO(cuianthony): Share this constant in GlicWindowController to use with
  // both the FRE dialog and the main glic window.
  set_corner_radius(12);
  SetAccessibleTitle(l10n_util::GetStringUTF16(IDS_GLIC_WINDOW_TITLE));

  SetLayoutManager(std::make_unique<views::FillLayout>());

  auto web_view = std::make_unique<views::WebView>(profile);
  web_view->SetProperty(views::kElementIdentifierKey,
                        kWebViewElementIdForTesting);
  web_view_ = web_view.get();
  web_view->SetPreferredSize(fre_controller->GetFreInitialSize());

  contents_ = std::make_unique<FreWebUIContentsContainer>(profile, web_view_,
                                                          fre_controller);
  web_view->SetWebContents(contents_->web_contents());

  AddChildView(std::move(web_view));
}

GlicFreDialogView::~GlicFreDialogView() = default;

}  // namespace glic

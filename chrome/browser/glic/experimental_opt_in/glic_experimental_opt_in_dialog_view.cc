// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/experimental_opt_in/glic_experimental_opt_in_dialog_view.h"

#include <memory>

#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/mojom/ui_base_types.mojom.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/view_class_properties.h"
#include "url/gurl.h"

namespace glic {

namespace {

constexpr int kDefaultWidth = 800;
constexpr int kDefaultHeight = 600;

class GlicWebView : public views::WebView {
 public:
  explicit GlicWebView(Profile* profile) : views::WebView(profile) {}
  ~GlicWebView() override = default;

  gfx::Size GetMinimumSize() const override {
    return gfx::Size(kDefaultWidth, kDefaultHeight);
  }
};

}  // namespace

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(GlicExperimentalOptInDialogView,
                                      kDialogElementId);

GlicExperimentalOptInDialogView::GlicExperimentalOptInDialogView(
    Profile* profile) {
  SetShowCloseButton(false);
  SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));
  SetModalType(ui::mojom::ModalType::kChild);
  set_esc_should_cancel_dialog_override(true);

  auto web_view = std::make_unique<GlicWebView>(profile);
  web_view->SetProperty(views::kElementIdentifierKey, kDialogElementId);

  gfx::Size initial_size(kDefaultWidth, kDefaultHeight);
  web_view->SetPreferredSize(initial_size);

  // Create WebContents for the webview.
  auto web_contents =
      content::WebContents::Create(content::WebContents::CreateParams(profile));

  // Load about:blank for this initial minimal CL.
  web_contents->GetController().LoadURLWithParams(
      content::NavigationController::LoadURLParams(GURL("about:blank")));

  web_view->SetOwnedWebContents(std::move(web_contents));

  SetContentsView(std::move(web_view));
}

GlicExperimentalOptInDialogView::~GlicExperimentalOptInDialogView() = default;

}  // namespace glic

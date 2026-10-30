// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/selection/explain_suggestion_fre.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/prefs/pref_service.h"
#include "components/tabs/public/tab_interface.h"
#include "ui/base/base_window.h"
#include "ui/base/models/dialog_model.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/views/bubble/bubble_dialog_model_host.h"

namespace glic {

namespace {

// TODO(b:576746917): Move the FRE strings to the .grd once UX signs them off.
constexpr char16_t kFreTitle[] =
    u"Let Chrome use page content to explain highlighted text?";
constexpr char16_t kFreBody[] =
    u"The content and URL of the page you\u2019re on, and the highlighted "
    u"text, will be sent to Google to generate an explanation";
constexpr char16_t kFreAccept[] = u"Allow";
constexpr char16_t kFreDecline[] = u"Cancel";

}  // namespace

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(ExplainSuggestionFre,
                                      kAllowButtonElementId);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(ExplainSuggestionFre,
                                      kCancelButtonElementId);

ExplainSuggestionFre::ExplainSuggestionFre(tabs::TabInterface& tab)
    : tab_(tab) {}

ExplainSuggestionFre::~ExplainSuggestionFre() = default;

// static
bool ExplainSuggestionFre::IsAccepted(Profile* profile) {
  return profile &&
         profile->GetPrefs()->GetBoolean(prefs::kGlicQuickAnswersFreAccepted);
}

void ExplainSuggestionFre::Show(ResultCallback callback) {
  // Only null for the mock tab in unit tests.
  BrowserWindowInterface* browser = tab_->GetBrowserWindowInterface();
  if (IsShowing() || !browser) {
    std::move(callback).Run(false);
    return;
  }
  callback_ = std::move(callback);

  auto model = ui::DialogModel::Builder()
                   .SetInternalName(kQuickAnswersFreDialogName)
                   .SetTitle(kFreTitle)
                   .AddParagraph(ui::DialogModelLabel(kFreBody))
                   .AddOkButton(base::DoNothing(),
                                ui::DialogModel::Button::Params()
                                    .SetLabel(kFreAccept)
                                    .SetId(kAllowButtonElementId)
                                    .SetStyle(ui::ButtonStyle::kProminent))
                   .AddCancelButton(base::DoNothing(),
                                    ui::DialogModel::Button::Params()
                                        .SetLabel(kFreDecline)
                                        .SetId(kCancelButtonElementId)
                                        .SetStyle(ui::ButtonStyle::kTonal))
                   .Build();
  std::unique_ptr<views::BubbleDialogModelHost> host =
      views::BubbleDialogModelHost::CreateModal(std::move(model),
                                                ui::mojom::ModalType::kWindow);
  host->SetOwnershipOfNewWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  // Window-modal: the user can't use the browser window, overlay included,
  // until they answer.
  widget_ = base::WrapUnique(constrained_window::CreateBrowserModalDialogViews(
      std::move(host), browser->GetWindow()->GetNativeWindow()));
  widget_->MakeCloseSynchronous(base::BindOnce(&ExplainSuggestionFre::OnClosed,
                                               weak_ptr_factory_.GetWeakPtr()));
  widget_->Show();
}

bool ExplainSuggestionFre::IsShowing() const {
  return !!widget_;
}

void ExplainSuggestionFre::Close() {
  weak_ptr_factory_.InvalidateWeakPtrs();
  widget_.reset();
  callback_.Reset();
}

void ExplainSuggestionFre::OnClosed(views::Widget::ClosedReason reason) {
  widget_.reset();
  const bool accepted =
      reason == views::Widget::ClosedReason::kAcceptButtonClicked;
  if (accepted) {
    tab_->GetProfile()->GetPrefs()->SetBoolean(
        prefs::kGlicQuickAnswersFreAccepted, true);
  }
  // May destroy `this`.
  std::move(callback_).Run(accepted);
}

}  // namespace glic

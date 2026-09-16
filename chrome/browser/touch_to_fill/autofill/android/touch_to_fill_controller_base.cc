// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/touch_to_fill/autofill/android/touch_to_fill_controller_base.h"

#include <optional>
#include <utility>

#include "base/functional/bind.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#include "chrome/browser/ui/autofill/autofill_popup_hide_helper.h"
#include "chrome/browser/ui/autofill/autofill_suggestion_controller_utils.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/core/browser/foundations/browser_autofill_manager.h"
#include "components/autofill/core/browser/integrators/touch_to_fill/touch_to_fill_payment_method_delegate.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"

namespace autofill {

namespace {

bool IsAncestorOf(content::RenderFrameHost* ancestor,
                  content::RenderFrameHost* descendant) {
  for (auto* rfh = descendant; rfh; rfh = rfh->GetParent()) {
    if (rfh == ancestor) {
      return true;
    }
  }
  return false;
}

}  // namespace

TouchToFillControllerBase::TouchToFillControllerBase() = default;
TouchToFillControllerBase::~TouchToFillControllerBase() = default;

bool TouchToFillControllerBase::InitHideHelper(
    TouchToFillPaymentMethodDelegate& delegate) {
  content::WebContents* wc = GetWebContents();
  if (!wc) {
    return false;
  }
  content::RenderFrameHost* rfh = wc->GetFocusedFrame();
  content::RenderFrameHost* delegate_rfh =
      static_cast<ContentAutofillDriver&>(
          delegate.GetAutofillManager().driver())
          .render_frame_host();

  if (!rfh || !IsAncestorOf(delegate_rfh, rfh)) {
    return false;
  }

  // The WebContents may have lost the focus because, for example, a new tab has
  // just been opened.
  // Beware that this check only works as intended before TTF is shown because
  // the bottom sheet steals the focus from the RenderWidgetHostView.
  if (auto* rwhv = wc->GetRenderWidgetHostView(); !rwhv || !rwhv->HasFocus()) {
    return false;
  }

  if (IsPointerLocked(wc)) {
    return false;
  }

  // The bottom sheet steals the focus from the WebContents, so we cannot rely
  // on AutofillPopupHideHelper's focus handling. Instead, we check if
  // `IsActiveWebContents()` in the event handlers below.
  AutofillPopupHideHelper::HidingParams params = {
      .hide_on_web_contents_lost_focus = false};

  AutofillPopupHideHelper::HidingCallback hide_callback =
      base::IgnoreArgs<SuggestionHidingReason>(base::BindRepeating(
          &TouchToFillControllerBase::Hide, base::Unretained(this)));

  // TODO(crbug.com/521318493): Should we hide TTF in the face of a PiP?
  AutofillPopupHideHelper::PictureInPictureDetectionCallback
      pip_detection_callback = base::BindRepeating([]() { return false; });

  hide_helper_ = std::make_unique<AutofillPopupHideHelper>(
      wc, rfh->GetGlobalId(), std::move(params), std::move(hide_callback),
      std::move(pip_detection_callback));
  return true;
}

bool TouchToFillControllerBase::IsActiveWebContents() {
  content::WebContents* wc = GetWebContents();
  if (!wc) {
    return false;
  }
  TabModel* tab_model = TabModelList::GetTabModelForWebContents(wc);
  return tab_model && tab_model->GetActiveWebContents() == wc;
}

}  // namespace autofill

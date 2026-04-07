// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_INDIGO_ONBOARDING_INDIGO_ONBOARDING_DIALOG_H_
#define CHROME_BROWSER_INDIGO_ONBOARDING_INDIGO_ONBOARDING_DIALOG_H_

#include <memory>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/common/indigo/indigo.mojom.h"
#include "content/public/browser/global_routing_id.h"
#include "mojo/public/cpp/bindings/associated_receiver_set.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/views/view_observer.h"
#include "ui/views/widget/widget.h"

class GURL;

namespace content {
class RenderFrameHost;
class WebContents;
}  // namespace content

namespace tabs {
class TabInterface;
}

namespace views {
class DialogDelegate;
class View;
}  // namespace views

namespace indigo {

struct OnboardingResult {
  bool acknowledge_chrome_disclaimer = false;
};

// Owns and manages an onboarding dialog which is mainly powered by a WebView.
class IndigoOnboardingDialog
    : public views::ViewObserver,
      public chrome::mojom::IndigoOnboardingDialogHost {
 public:
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kWebViewId);

  // Creates and shows a tab-modal dialog embedding a WebView pointing to an
  // external onboarding URL. Returns a new `IndigoOnboardingDialog`, or nullptr
  // if the tab cannot show modal UI (e.g., another modal UI is already shown).
  // `close_callback` is run when the dialog is closed.
  static std::unique_ptr<IndigoOnboardingDialog> Show(
      tabs::TabInterface& tab,
      const GURL& onboarding_url,
      base::OnceCallback<void(const OnboardingResult&)> close_callback);

  static void BindOnboardingDialogHost(
      mojo::PendingAssociatedReceiver<chrome::mojom::IndigoOnboardingDialogHost>
          receiver,
      content::RenderFrameHost* render_frame_host);

  static bool IsOnboardingWebContents(content::WebContents* web_contents);

  // chrome::mojom::OnboardingDialogHost:
  void AcknowledgeChromeDisclaimer() override;

  IndigoOnboardingDialog(const IndigoOnboardingDialog&) = delete;
  IndigoOnboardingDialog& operator=(const IndigoOnboardingDialog&) = delete;

  ~IndigoOnboardingDialog() override;

  // Closes the dialog immediately. This will call the `close_callback` passed
  // to `Show`, likely resulting in the destruction of this object.
  void Close();

 private:
  explicit IndigoOnboardingDialog(
      tabs::TabInterface& tab,
      const GURL& onboarding_url,
      base::OnceCallback<void(const OnboardingResult&)> close_callback);

  void OnWidgetClosed(views::Widget::ClosedReason reason);

  // views::ViewObserver:
  void OnViewPreferredSizeChanged(views::View* observed_view) override;

  // It is safe to hold a raw pointer to `tab_` because the dialog is tab-modal
  // and will be closed (and this object destroyed by its owner) if the tab is
  // destroyed.
  const raw_ptr<tabs::TabInterface> tab_;

  base::OnceCallback<void(const OnboardingResult&)> close_callback_;

  std::unique_ptr<views::DialogDelegate> delegate_;
  base::ScopedObservation<views::View, views::ViewObserver> view_observation_{
      this};

  // `widget_` must be destroyed early, and especially before `delegate_`,
  // because the widget holds a raw pointer to the delegate. It should also
  // be destroyed before `view_observation_`, since it is accessed (to reset)
  // in `OnWidgetClosed`, which can happen during widget destruction.
  std::unique_ptr<views::Widget> widget_;

  mojo::AssociatedReceiverSet<chrome::mojom::IndigoOnboardingDialogHost,
                              content::GlobalRenderFrameHostId>
      receiver_set_;
  OnboardingResult onboarding_result_;

  base::WeakPtrFactory<IndigoOnboardingDialog> weak_ptr_factory_{this};
};

}  // namespace indigo

#endif  // CHROME_BROWSER_INDIGO_ONBOARDING_INDIGO_ONBOARDING_DIALOG_H_

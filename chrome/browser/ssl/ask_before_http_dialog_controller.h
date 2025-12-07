// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SSL_ASK_BEFORE_HTTP_DIALOG_CONTROLLER_H_
#define CHROME_BROWSER_SSL_ASK_BEFORE_HTTP_DIALOG_CONTROLLER_H_

#include <memory>

#include "base/callback_list.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/security_interstitials/core/metrics_helper.h"
#include "components/tabs/public/tab_interface.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/views/widget/widget.h"

class GURL;

namespace content {
class WebContents;
}  // namespace content

namespace ui {
class DialogModel;
class Event;
}  // namespace ui

inline constexpr char kAskBeforeHttpDialogName[] = "AskBeforeHttpDialog";

// Manages the Ask-before-HTTP dialog instance for the assocated tab.
class AskBeforeHttpDialogController {
 public:
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kGoBackButtonId);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kContinueButtonId);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kDescriptionTextId);

  explicit AskBeforeHttpDialogController(tabs::TabInterface* tab_interface);
  AskBeforeHttpDialogController(const AskBeforeHttpDialogController&) = delete;
  AskBeforeHttpDialogController& operator=(
      const AskBeforeHttpDialogController&) = delete;
  ~AskBeforeHttpDialogController();

  // Show the Ask-before-HTTP dialog. The user can choose to proceed through the
  // warning, or go back to the previous page. If called while a dialog is
  // already showing, this will create a new dialog and replace the old one.
  void ShowDialog(content::WebContents* web_contents,
                  const GURL& request_url,
                  ukm::SourceId navigation_source_id);

  // Returns whether there is an associated open dialog widget.
  bool HasOpenDialogWidget() const;

  // Closes the dialog widget, if one is open.
  void CloseDialogWidget(views::Widget::ClosedReason reason);

 private:
  std::unique_ptr<ui::DialogModel> CreateDialogModel(const GURL& request_url);
  void OnHelpCenterLinkClicked(const ui::Event& event);
  void OnGoBackButtonClicked();
  void OnContinueButtonClicked(const GURL& request_url, const ui::Event& event);
  void TabWillDetach(tabs::TabInterface* tab,
                     tabs::TabInterface::DetachReason reason);

  ukm::SourceId navigation_source_id_ = ukm::kInvalidSourceId;
  std::unique_ptr<security_interstitials::MetricsHelper> metrics_helper_;

  // The associated tab. Guaranteed to remain valid for the lifetime of this
  // class. This can be used to dynamically access relevant window state.
  const raw_ptr<tabs::TabInterface> tab_interface_ = nullptr;

  // Pointer to the widget that contains the current open dialog, if any.
  std::unique_ptr<views::Widget> dialog_widget_;

  base::CallbackListSubscription tab_will_detach_subscription_;

  base::WeakPtrFactory<AskBeforeHttpDialogController> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_SSL_ASK_BEFORE_HTTP_DIALOG_CONTROLLER_H_

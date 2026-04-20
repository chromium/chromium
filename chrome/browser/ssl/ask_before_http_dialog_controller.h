// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SSL_ASK_BEFORE_HTTP_DIALOG_CONTROLLER_H_
#define CHROME_BROWSER_SSL_ASK_BEFORE_HTTP_DIALOG_CONTROLLER_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "components/security_interstitials/core/metrics_helper.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents_observer.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"
#include "url/gurl.h"

#if !BUILDFLAG(IS_ANDROID)
#include "ui/views/widget/widget.h"
#endif

namespace content {
class WebContents;
}  // namespace content

namespace ui {
class DialogModel;
class Event;
}  // namespace ui

inline constexpr char kAskBeforeHttpDialogName[] = "AskBeforeHttpDialog";

// Manages the Ask-before-HTTP dialog instance for the associated WebContents.
class AskBeforeHttpDialogController : public content::WebContentsObserver {
 public:
  DECLARE_USER_DATA(AskBeforeHttpDialogController);
  static AskBeforeHttpDialogController* From(tabs::TabInterface* tab);

  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kGoBackButtonId);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kContinueButtonId);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kDescriptionTextId);

  explicit AskBeforeHttpDialogController(tabs::TabInterface* tab);

  AskBeforeHttpDialogController(const AskBeforeHttpDialogController&) = delete;
  AskBeforeHttpDialogController& operator=(
      const AskBeforeHttpDialogController&) = delete;
  ~AskBeforeHttpDialogController() override;

  // Show the Ask-before-HTTP dialog. The user can choose to proceed through the
  // warning, or go back to the previous page. If called while a dialog is
  // already showing, this will create a new dialog and replace the old one.
  void ShowDialog(content::WebContents* web_contents,
                  const GURL& request_url,
                  ukm::SourceId navigation_source_id);

  // Returns whether there is an associated open dialog.
  bool HasOpenDialog() const;

  // Closes the dialog programmatically when not triggered by user interaction.
  void CloseDialog();

#if !BUILDFLAG(IS_ANDROID)
  // Closes the dialog widget, if one is open.
  void CloseDialogWidget(views::Widget::ClosedReason reason);
#endif

  // content::WebContentsObserver:
  void OnVisibilityChanged(content::Visibility visibility) override;

 private:
  std::unique_ptr<ui::DialogModel> CreateDialogModel(const GURL& request_url);
  void OnHelpCenterLinkClicked(const ui::Event& event);
  void OnGoBackButtonClicked();
  void OnContinueButtonClicked(const GURL& request_url);
  void OnDialogDestroying();
  void OnUserDismissed();

  ukm::SourceId navigation_source_id_ = ukm::kInvalidSourceId;
  GURL request_url_;
  bool is_suspended_ = false;
  std::unique_ptr<security_interstitials::MetricsHelper> metrics_helper_;

#if !BUILDFLAG(IS_ANDROID)
  // Pointer to the widget that contains the current open dialog, if any.
  std::unique_ptr<views::Widget> dialog_widget_;
#else
  // Pointer to the dialog model, if any.
  raw_ptr<ui::DialogModel> current_dialog_model_ = nullptr;
#endif

  ui::ScopedUnownedUserData<AskBeforeHttpDialogController>
      scoped_unowned_user_data_;

  base::WeakPtrFactory<AskBeforeHttpDialogController> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_SSL_ASK_BEFORE_HTTP_DIALOG_CONTROLLER_H_

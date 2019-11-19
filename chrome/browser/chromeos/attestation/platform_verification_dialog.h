// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ATTESTATION_PLATFORM_VERIFICATION_DIALOG_H_
#define CHROME_BROWSER_CHROMEOS_ATTESTATION_PLATFORM_VERIFICATION_DIALOG_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "content/public/browser/reload_type.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/window/dialog_delegate.h"

namespace content {
class WebContents;
}

namespace chromeos {
namespace attestation {

// A tab-modal dialog UI to ask the user for PlatformVerificationFlow.
class PlatformVerificationDialog : public views::DialogDelegateView,
                                   public views::ButtonListener,
                                   public content::WebContentsObserver {
 public:
  enum ConsentResponse {
    CONSENT_RESPONSE_NONE,
    CONSENT_RESPONSE_ALLOW,
    CONSENT_RESPONSE_DENY
  };

  using ConsentCallback = base::OnceCallback<void(ConsentResponse response)>;

  // Initializes a tab-modal dialog for |web_contents| and |requesting_origin|
  // and shows it. Returns a non-owning pointer to the widget so that caller can
  // close the dialog and cancel the request. The returned widget is only
  // guaranteed to be valid before |callback| is called. The |callback| will
  // never be run when null is returned.
  static views::Widget* ShowDialog(content::WebContents* web_contents,
                                   const GURL& requesting_origin,
                                   ConsentCallback callback);

 protected:
  ~PlatformVerificationDialog() override;

 private:
  PlatformVerificationDialog(content::WebContents* web_contents,
                             const base::string16& domain,
                             ConsentCallback callback);

  // views::DialogDelegate:
  bool Cancel() override;
  bool Accept() override;
  bool Close() override;

  // views::WidgetDelegate:
  ui::ModalType GetModalType() const override;

  // views::View:
  gfx::Size CalculatePreferredSize() const override;

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // content::WebContentsObserver:
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;

  base::string16 domain_;
  ConsentCallback callback_;
  views::ImageButton* learn_more_button_;

  DISALLOW_COPY_AND_ASSIGN(PlatformVerificationDialog);
};

}  // namespace attestation
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_ATTESTATION_PLATFORM_VERIFICATION_DIALOG_H_

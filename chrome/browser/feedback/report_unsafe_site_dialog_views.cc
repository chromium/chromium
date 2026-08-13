// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/feedback/report_unsafe_site_dialog_views.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/feedback/report_unsafe_site/screenshot_taker.h"
#include "chrome/browser/feedback/report_unsafe_site_dialog.h"
#include "chrome/browser/feedback/show_feedback_page.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/views/web_dialogs/chrome_webui_dialog.h"
#include "chrome/browser/ui/webui/feedback/feedback_ui.h"
#include "chrome/browser/ui/webui/top_chrome/untrusted_top_chrome_web_ui_controller.h"
#include "chrome/browser/ui/webui/top_chrome/webui_contents_wrapper.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/branded_strings.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/base/window_open_disposition.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

namespace feedback {
DEFINE_ELEMENT_IDENTIFIER_VALUE(kReportUnsafeSiteWebviewElementId);

namespace {

void OnWidgetClose(std::unique_ptr<views::Widget> widget,
                   views::Widget::ClosedReason closed_reason) {
  base::UmaHistogramEnumeration(
      "SafeBrowsing.ReportUnsafeSite.DialogClosedReason", closed_reason);
  widget.reset();
}

// Serves the Safe Browsing policy links, which branded builds open in a tab.
content::WebContents* OpenLinkInBrowser(
    base::WeakPtr<tabs::TabInterface> tab_interface,
    content::WebContents* source,
    std::unique_ptr<content::WebContents> new_contents,
    const GURL& target_url,
    WindowOpenDisposition disposition,
    const blink::mojom::WindowFeatures& window_features,
    bool user_gesture) {
  if (!tab_interface || !user_gesture) {
    return nullptr;
  }
  if (disposition != WindowOpenDisposition::NEW_FOREGROUND_TAB &&
      disposition != WindowOpenDisposition::NEW_POPUP) {
    return nullptr;
  }
  return chrome::AddWebContents(
      tab_interface->GetBrowserWindowInterface(), source,
      std::move(new_contents), target_url, disposition, window_features,
      NavigateParams::WindowAction::kShowWindow, user_gesture);
}

}  // anonymous namespace

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(ReportUnsafeSiteDialogViews,
                                      kReportUnsafeSiteDialogId);

// static
bool ReportUnsafeSiteDialog::IsEnabled(const Profile& profile) {
  const PrefService* prefs = profile.GetPrefs();
  return base::FeatureList::IsEnabled(features::kReportUnsafeSite) &&
         !profile.IsOffTheRecord() && chrome::CanShowFeedback(&profile) &&
         safe_browsing::IsSafeBrowsingEnabled(*prefs);
}

// static
void ReportUnsafeSiteDialog::Show(Browser* browser) {
  Profile* profile = browser->GetProfile();
  if (!ReportUnsafeSiteDialog::IsEnabled(*profile)) {
    return;
  }

  content::WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  if (!web_contents) {
    return;
  }

  tabs::TabInterface* tab_interface =
      tabs::TabInterface::GetFromContents(web_contents);
  if (!tab_interface) {
    return;
  }

  if (!tab_interface->CanShowModalUI()) {
    return;
  }

  // The dialog might be shown for a different tab than the user expected when
  // the tab is split.
  base::UmaHistogramBoolean("SafeBrowsing.ReportUnsafeSiteDialog.IsTabSplit",
                            tab_interface->IsSplit());

  auto contents_wrapper = std::make_unique<WebUIContentsWrapperT<FeedbackUI>>(
      GURL(chrome::kChromeUIFeedbackReportUnsafeSiteURL), profile,
      IDS_REPORT_UNSAFE_SITE_DIALOG_TITLE);
  FeedbackUI* feedback_ui = contents_wrapper->GetWebUIController();
  feedback_ui->set_triggering_web_contents(web_contents);
  feedback_ui->set_screenshot_taker(
      ScreenshotTaker::Start(web_contents->GetPrimaryMainFrame()->GetView()));

  webui_dialog::WebDialogSpec spec;
  spec.modal_type = ui::mojom::ModalType::kChild;
  spec.parent_tab = tab_interface->GetWeakPtr();
  // Sizing is left unconstrained; this dialog has always been content-sized.
  //
  // The page draws its own buttons, so ESC must not report a cancel.
  spec.esc_should_cancel_dialog_override = false;
  spec.dialog_element_identifier =
      ReportUnsafeSiteDialogViews::kReportUnsafeSiteDialogId;
  spec.element_identifier = kReportUnsafeSiteWebviewElementId;
  spec.add_new_contents_callback =
      base::BindRepeating(&OpenLinkInBrowser, tab_interface->GetWeakPtr());
  std::unique_ptr<views::Widget> widget = webui_dialog::ChromeWebUIDialog::Show(
      tab_interface->GetBrowserWindowInterface()
          ->GetWindow()
          ->GetNativeWindow(),
      std::move(contents_wrapper), spec);
  feedback_ui->set_dialog_widget(widget.get());

  widget->MakeCloseSynchronous(
      base::BindOnce(&OnWidgetClose, std::move(widget)));
}

}  // namespace feedback

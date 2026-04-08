// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/feedback/report_unsafe_site_dialog_views.h"

#include "base/metrics/histogram_functions.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/feedback/report_unsafe_site/screenshot_taker.h"
#include "chrome/browser/feedback/report_unsafe_site_dialog.h"
#include "chrome/browser/feedback/show_feedback_page.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/public/tab_dialog_manager.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/views/bubble/webui_bubble_dialog_view.h"
#include "chrome/browser/ui/webui/feedback/feedback_ui.h"
#include "chrome/browser/ui/webui/top_chrome/untrusted_top_chrome_web_ui_controller.h"
#include "chrome/browser/ui/webui/top_chrome/webui_contents_wrapper.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/branded_strings.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"

namespace feedback {
DEFINE_ELEMENT_IDENTIFIER_VALUE(kReportUnsafeSiteWebviewElementId);

namespace {

void OnWidgetClose(std::unique_ptr<views::Widget> widget,
                   views::Widget::ClosedReason closed_reason) {
  base::UmaHistogramEnumeration(
      "SafeBrowsing.ReportUnsafeSite.DialogClosedReason", closed_reason);
  widget.reset();
}

// Report-unsafe-site WebUIBubbleDialogView.
class ReportUnsafeSiteDialogView : public WebUIBubbleDialogView {
  METADATA_HEADER(ReportUnsafeSiteDialogView, WebUIBubbleDialogView)

 public:
  ReportUnsafeSiteDialogView(
      std::unique_ptr<WebUIContentsWrapper> contents_wrapper,
      tabs::TabInterface* tab_interface)
      : WebUIBubbleDialogView(/*anchor_view=*/nullptr,
                              contents_wrapper->GetWeakPtr()),
        contents_wrapper_(std::move(contents_wrapper)),
        tab_interface_(tab_interface->GetWeakPtr()) {
    set_parent_window(platform_util::GetViewForWindow(
        tab_interface->GetBrowserWindowInterface()
            ->GetWindow()
            ->GetNativeWindow()));
    set_close_on_deactivate(false);
    set_esc_should_cancel_dialog_override(false);
    SetShowCloseButton(false);
    SetProperty(views::kElementIdentifierKey,
                ReportUnsafeSiteDialogViews::kReportUnsafeSiteDialogId);
    web_view()->SetProperty(views::kElementIdentifierKey,
                            feedback::kReportUnsafeSiteWebviewElementId);
  }
  ~ReportUnsafeSiteDialogView() override = default;

  // WebUIContentsWrapper::Host:
  void ShowUI() override {
    if (!tab_interface_ || !GetWidget()) {
      return;
    }
    tab_interface_->GetTabFeatures()->tab_dialog_manager()->ShowDialog(
        GetWidget(), std::make_unique<tabs::TabDialogManager::Params>());
  }

  content::WebContents* AddNewContents(
      content::WebContents* source,
      std::unique_ptr<content::WebContents> new_contents,
      const GURL& target_url,
      WindowOpenDisposition disposition,
      const blink::mojom::WindowFeatures& window_features,
      bool user_gesture,
      bool* was_blocked) override {
    if (!tab_interface_ || !user_gesture) {
      return nullptr;
    }
    if (disposition != WindowOpenDisposition::NEW_FOREGROUND_TAB &&
        disposition != WindowOpenDisposition::NEW_POPUP) {
      return nullptr;
    }
    return chrome::AddWebContents(
        tab_interface_->GetBrowserWindowInterface(), source,
        std::move(new_contents), target_url, disposition, window_features,
        NavigateParams::WindowAction::kShowWindow, user_gesture);
  }

 private:
  std::unique_ptr<WebUIContentsWrapper> contents_wrapper_;
  const base::WeakPtr<tabs::TabInterface> tab_interface_;
};

BEGIN_METADATA(ReportUnsafeSiteDialogView)
END_METADATA

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
  Profile* profile = browser->profile();
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
  auto bubble_dialog = std::make_unique<ReportUnsafeSiteDialogView>(
      std::move(contents_wrapper), tab_interface);
  std::unique_ptr<views::Widget> widget =
      base::WrapUnique(views::BubbleDialogDelegateView::CreateBubble(
          std::move(bubble_dialog),
          views::Widget::InitParams::CLIENT_OWNS_WIDGET));
  feedback_ui->set_dialog_widget(widget.get());

  widget->MakeCloseSynchronous(
      base::BindOnce(&OnWidgetClose, std::move(widget)));
}

}  // namespace feedback

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/feedback/report_unsafe_site_dialog_views.h"

#include "chrome/browser/feedback/report_unsafe_site_dialog.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/public/tab_dialog_manager.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/views/bubble/webui_bubble_dialog_view.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/webui/feedback/feedback_ui.h"
#include "chrome/browser/ui/webui/top_chrome/untrusted_top_chrome_web_ui_controller.h"
#include "chrome/browser/ui/webui/top_chrome/webui_contents_wrapper.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/branded_strings.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"

namespace feedback {
namespace {

// Report-unsafe-site WebUIBubbleDialogView.
class ReportUnsafeSiteDialogView : public WebUIBubbleDialogView {
  METADATA_HEADER(ReportUnsafeSiteDialogView, WebUIBubbleDialogView)

 public:
  ReportUnsafeSiteDialogView(
      std::unique_ptr<WebUIContentsWrapper> contents_wrapper,
      gfx::NativeWindow parent_window)
      : WebUIBubbleDialogView(/*anchor_view=*/nullptr,
                              contents_wrapper->GetWeakPtr()),
        contents_wrapper_(std::move(contents_wrapper)) {
    set_parent_window(platform_util::GetViewForWindow(parent_window));
    set_close_on_deactivate(false);
    SetTitle(l10n_util::GetStringUTF16(IDS_REPORT_UNSAFE_SITE_DIALOG_TITLE));
    SetShowCloseButton(true);
    set_fixed_width(ChromeLayoutProvider::Get()->GetDistanceMetric(
        views::DISTANCE_BUBBLE_PREFERRED_WIDTH));
    SetProperty(views::kElementIdentifierKey,
                ReportUnsafeSiteDialogViews::kReportUnsafeSiteDialogId);
  }
  ~ReportUnsafeSiteDialogView() override = default;

 private:
  std::unique_ptr<WebUIContentsWrapper> contents_wrapper_;
};

BEGIN_METADATA(ReportUnsafeSiteDialogView)
END_METADATA

}  // anonymous namespace

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(ReportUnsafeSiteDialogViews,
                                      kReportUnsafeSiteDialogId);

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

  auto contents_wrapper = std::make_unique<WebUIContentsWrapperT<FeedbackUI>>(
      GURL(chrome::kChromeUIFeedbackReportUnsafeSiteURL), profile,
      IDS_REPORT_UNSAFE_SITE_DIALOG_TITLE);
  auto bubble_dialog = std::make_unique<ReportUnsafeSiteDialogView>(
      std::move(contents_wrapper), browser->window()->GetNativeWindow());
  views::Widget* widget =
      views::BubbleDialogDelegateView::CreateBubble(std::move(bubble_dialog));

  tab_interface->GetTabFeatures()->tab_dialog_manager()->ShowDialog(
      widget, std::make_unique<tabs::TabDialogManager::Params>());
}

}  // namespace feedback

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/ui/simple_web_view_dialog.h"

#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/chromeos/login/helper.h"
#include "chrome/browser/chromeos/login/ui/captive_portal_window_proxy.h"
#include "chrome/browser/command_updater_impl.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ssl/security_state_tab_helper.h"
#include "chrome/browser/ui/autofill/chrome_autofill_client.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/content_settings/content_setting_bubble_model_delegate.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/toolbar/reload_button.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "components/omnibox/browser/toolbar_model_impl.h"
#include "components/password_manager/core/browser/password_manager.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_constants.h"
#include "ipc/ipc_message.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/theme_provider.h"
#include "ui/views/background.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

using content::WebContents;
using views::GridLayout;

namespace {

const int kLocationBarHeight = 35;

// Margin between screen edge and SimpleWebViewDialog border.
const int kExternalMargin = 60;

// Margin between WebView and SimpleWebViewDialog border.
const int kInnerMargin = 2;

const SkColor kDialogColor = SK_ColorWHITE;

class ToolbarRowView : public views::View {
 public:
  ToolbarRowView() {
    SetBackground(views::CreateSolidBackground(kDialogColor));
  }

  ~ToolbarRowView() override {}

  void Init(views::View* back,
            views::View* forward,
            views::View* reload,
            views::View* location_bar) {
    GridLayout* layout = SetLayoutManager(std::make_unique<GridLayout>(this));

    const int related_horizontal_spacing =
        ChromeLayoutProvider::Get()->GetDistanceMetric(
            views::DISTANCE_RELATED_CONTROL_HORIZONTAL);
    // Back button.
    views::ColumnSet* column_set = layout->AddColumnSet(0);
    column_set->AddColumn(GridLayout::CENTER, GridLayout::CENTER, 0,
                          GridLayout::USE_PREF, 0, 0);
    column_set->AddPaddingColumn(0, related_horizontal_spacing);
    // Forward button.
    column_set->AddColumn(GridLayout::CENTER, GridLayout::CENTER, 0,
                          GridLayout::USE_PREF, 0, 0);
    column_set->AddPaddingColumn(0, related_horizontal_spacing);
    // Reload button.
    column_set->AddColumn(GridLayout::CENTER, GridLayout::CENTER, 0,
                          GridLayout::USE_PREF, 0, 0);
    column_set->AddPaddingColumn(0, related_horizontal_spacing);
    // Location bar.
    column_set->AddColumn(GridLayout::FILL, GridLayout::CENTER, 1,
                          GridLayout::FIXED, kLocationBarHeight, 0);
    column_set->AddPaddingColumn(0, related_horizontal_spacing);

    layout->StartRow(0, 0);
    layout->AddView(back);
    layout->AddView(forward);
    layout->AddView(reload);
    layout->AddView(location_bar);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ToolbarRowView);
};

}  // namespace

namespace chromeos {

// Stub implementation of ContentSettingBubbleModelDelegate.
class StubBubbleModelDelegate : public ContentSettingBubbleModelDelegate {
 public:
  StubBubbleModelDelegate() {}
  ~StubBubbleModelDelegate() override {}

 private:
  // ContentSettingBubbleModelDelegate implementation:
  void ShowCollectedCookiesDialog(content::WebContents* web_contents) override {
  }
  void ShowContentSettingsPage(ContentSettingsType type) override {}
  void ShowMediaSettingsPage() override {}
  void ShowLearnMorePage(ContentSettingsType type) override {}

  DISALLOW_COPY_AND_ASSIGN(StubBubbleModelDelegate);
};

// SimpleWebViewDialog class ---------------------------------------------------

SimpleWebViewDialog::SimpleWebViewDialog(Profile* profile)
    : profile_(profile),
      bubble_model_delegate_(new StubBubbleModelDelegate) {
  command_updater_.reset(new CommandUpdaterImpl(this));
  command_updater_->UpdateCommandEnabled(IDC_BACK, true);
  command_updater_->UpdateCommandEnabled(IDC_FORWARD, true);
  command_updater_->UpdateCommandEnabled(IDC_STOP, true);
  command_updater_->UpdateCommandEnabled(IDC_RELOAD, true);
  command_updater_->UpdateCommandEnabled(IDC_RELOAD_BYPASSING_CACHE, true);
  command_updater_->UpdateCommandEnabled(IDC_RELOAD_CLEARING_CACHE, true);
}

SimpleWebViewDialog::~SimpleWebViewDialog() {
  if (web_view_ && web_view_->web_contents())
    web_view_->web_contents()->SetDelegate(nullptr);
}

void SimpleWebViewDialog::StartLoad(const GURL& url) {
  if (!web_view_container_.get())
    web_view_container_.reset(new views::WebView(profile_));
  web_view_ = web_view_container_.get();
  web_view_->set_owned_by_client();
  web_view_->GetWebContents()->SetDelegate(this);
  web_view_->LoadInitialURL(url);

  WebContents* web_contents = web_view_->GetWebContents();
  DCHECK(web_contents);

  // Create the password manager that is needed for the proxy.
  ChromePasswordManagerClient::CreateForWebContentsWithAutofillClient(
      web_contents,
      autofill::ChromeAutofillClient::FromWebContents(web_contents));
}

void SimpleWebViewDialog::Init() {
  // Create the security state model that the toolbar model needs.
  if (web_view_->GetWebContents())
    SecurityStateTabHelper::CreateForWebContents(web_view_->GetWebContents());
  toolbar_model_.reset(
      new ToolbarModelImpl(this, content::kMaxURLDisplayChars));

  SetBackground(views::CreateSolidBackground(kDialogColor));

  // Back/Forward buttons.
  back_ = new views::ImageButton(this);
  back_->set_triggerable_event_flags(ui::EF_LEFT_MOUSE_BUTTON |
                                     ui::EF_MIDDLE_MOUSE_BUTTON);
  back_->set_tag(IDC_BACK);
  back_->SetImageAlignment(views::ImageButton::ALIGN_RIGHT,
                           views::ImageButton::ALIGN_TOP);
  back_->SetTooltipText(l10n_util::GetStringUTF16(IDS_TOOLTIP_BACK));
  back_->SetAccessibleName(l10n_util::GetStringUTF16(IDS_ACCNAME_BACK));
  back_->set_id(VIEW_ID_BACK_BUTTON);

  forward_ = new views::ImageButton(this);
  forward_->set_triggerable_event_flags(ui::EF_LEFT_MOUSE_BUTTON |
                                        ui::EF_MIDDLE_MOUSE_BUTTON);
  forward_->set_tag(IDC_FORWARD);
  forward_->SetTooltipText(l10n_util::GetStringUTF16(IDS_TOOLTIP_FORWARD));
  forward_->SetAccessibleName(l10n_util::GetStringUTF16(IDS_ACCNAME_FORWARD));
  forward_->set_id(VIEW_ID_FORWARD_BUTTON);

  // Location bar.
  location_bar_ = new LocationBarView(nullptr, profile_, command_updater_.get(),
                                      this, true);

  // Reload button.
  reload_ = new ReloadButton(command_updater_.get());
  reload_->set_triggerable_event_flags(ui::EF_LEFT_MOUSE_BUTTON |
                                       ui::EF_MIDDLE_MOUSE_BUTTON);
  reload_->set_tag(IDC_RELOAD);
  reload_->SetTooltipText(l10n_util::GetStringUTF16(IDS_TOOLTIP_RELOAD));
  reload_->SetAccessibleName(l10n_util::GetStringUTF16(IDS_ACCNAME_RELOAD));
  reload_->set_id(VIEW_ID_RELOAD_BUTTON);

  // Use separate view to setup custom background.
  ToolbarRowView* toolbar_row = new ToolbarRowView;
  toolbar_row->Init(back_, forward_, reload_, location_bar_);

  // Layout.
  GridLayout* layout = SetLayoutManager(std::make_unique<GridLayout>(this));

  views::ColumnSet* column_set = layout->AddColumnSet(0);
  column_set->AddColumn(GridLayout::FILL, GridLayout::FILL, 1,
                        GridLayout::FIXED, 0, 0);

  column_set = layout->AddColumnSet(1);
  column_set->AddPaddingColumn(0, kInnerMargin);
  column_set->AddColumn(GridLayout::FILL, GridLayout::FILL, 1,
                        GridLayout::FIXED, 0, 0);
  column_set->AddPaddingColumn(0, kInnerMargin);

  // Setup layout rows.
  layout->StartRow(0, 0);
  layout->AddView(toolbar_row);

  layout->AddPaddingRow(0, kInnerMargin);

  layout->StartRow(1, 1);
  layout->AddView(web_view_container_.get());
  layout->AddPaddingRow(0, kInnerMargin);

  LoadImages();

  location_bar_->Init();
  UpdateReload(web_view_->web_contents()->IsLoading(), true);

  gfx::Rect bounds(CalculateScreenBounds(gfx::Size()));
  bounds.Inset(kExternalMargin, kExternalMargin);
  layout->set_minimum_size(bounds.size());

  Layout();
}

void SimpleWebViewDialog::Layout() {
  views::WidgetDelegateView::Layout();
}

views::View* SimpleWebViewDialog::GetInitiallyFocusedView() {
  return web_view_;
}

void SimpleWebViewDialog::ButtonPressed(views::Button* sender,
                                        const ui::Event& event) {
  command_updater_->ExecuteCommand(sender->tag());
}

content::WebContents* SimpleWebViewDialog::OpenURL(
    const content::OpenURLParams& params) {
  // As there are no Browsers right now, this could not actually ever work.
  NOTIMPLEMENTED();
  return nullptr;
}

void SimpleWebViewDialog::NavigationStateChanged(
    WebContents* source,
    content::InvalidateTypes changed_flags) {
  if (location_bar_) {
    location_bar_->Update(nullptr);
    UpdateButtons();
  }
}

void SimpleWebViewDialog::LoadingStateChanged(WebContents* source,
                                              bool to_different_document) {
  bool is_loading = source->IsLoading();
  UpdateReload(is_loading && to_different_document, false);
  command_updater_->UpdateCommandEnabled(IDC_STOP, is_loading);
}

WebContents* SimpleWebViewDialog::GetWebContents() {
  return nullptr;
}

ToolbarModel* SimpleWebViewDialog::GetToolbarModel() {
  return toolbar_model_.get();
}

const ToolbarModel* SimpleWebViewDialog::GetToolbarModel() const {
  return toolbar_model_.get();
}

ContentSettingBubbleModelDelegate*
SimpleWebViewDialog::GetContentSettingBubbleModelDelegate() {
  return bubble_model_delegate_.get();
}

content::WebContents* SimpleWebViewDialog::GetActiveWebContents() const {
  return web_view_->web_contents();
}

void SimpleWebViewDialog::ExecuteCommandWithDisposition(int id,
                                                        WindowOpenDisposition) {
  WebContents* web_contents = web_view_->web_contents();
  switch (id) {
    case IDC_BACK:
      if (web_contents->GetController().CanGoBack()) {
        location_bar_->Revert();
        web_contents->GetController().GoBack();
      }
      break;
    case IDC_FORWARD:
      if (web_contents->GetController().CanGoForward()) {
        location_bar_->Revert();
        web_contents->GetController().GoForward();
      }
      break;
    case IDC_STOP:
      web_contents->Stop();
      break;
    case IDC_RELOAD:
    // Always reload bypassing cache.
    case IDC_RELOAD_BYPASSING_CACHE:
    case IDC_RELOAD_CLEARING_CACHE:
      location_bar_->Revert();
      web_contents->GetController().Reload(content::ReloadType::BYPASSING_CACHE,
                                           true);
      break;
    default:
      NOTREACHED();
  }
}

void SimpleWebViewDialog::LoadImages() {
  const ui::ThemeProvider* tp = GetThemeProvider();

  back_->SetImage(views::Button::STATE_NORMAL, tp->GetImageSkiaNamed(IDR_BACK));
  back_->SetImage(views::Button::STATE_HOVERED,
                  tp->GetImageSkiaNamed(IDR_BACK_H));
  back_->SetImage(views::Button::STATE_PRESSED,
                  tp->GetImageSkiaNamed(IDR_BACK_P));
  back_->SetImage(views::Button::STATE_DISABLED,
                  tp->GetImageSkiaNamed(IDR_BACK_D));

  forward_->SetImage(views::Button::STATE_NORMAL,
                     tp->GetImageSkiaNamed(IDR_FORWARD));
  forward_->SetImage(views::Button::STATE_HOVERED,
                     tp->GetImageSkiaNamed(IDR_FORWARD_H));
  forward_->SetImage(views::Button::STATE_PRESSED,
                     tp->GetImageSkiaNamed(IDR_FORWARD_P));
  forward_->SetImage(views::Button::STATE_DISABLED,
                     tp->GetImageSkiaNamed(IDR_FORWARD_D));

  reload_->LoadImages();
}

void SimpleWebViewDialog::UpdateButtons() {
  const content::NavigationController& navigation_controller =
      web_view_->web_contents()->GetController();
  back_->SetEnabled(navigation_controller.CanGoBack());
  forward_->SetEnabled(navigation_controller.CanGoForward());
}

void SimpleWebViewDialog::UpdateReload(bool is_loading, bool force) {
  if (reload_) {
    reload_->ChangeMode(
        is_loading ? ReloadButton::Mode::kStop : ReloadButton::Mode::kReload,
        force);
  }
}

}  // namespace chromeos

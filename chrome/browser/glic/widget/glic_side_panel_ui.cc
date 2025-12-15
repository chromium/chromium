// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/widget/glic_side_panel_ui.h"

#include "base/notimplemented.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/glic/public/glic_instance.h"
#include "chrome/browser/glic/service/glic_ui_embedder.h"
#include "chrome/browser/glic/service/metrics/glic_instance_metrics.h"
#include "chrome/browser/glic/widget/application_hotkey_delegate.h"
#include "chrome/browser/glic/widget/glic_inactive_side_panel_ui.h"
#include "chrome/browser/glic/widget/glic_panel_hotkey_delegate.h"
#include "chrome/browser/glic/widget/glic_view.h"
#include "chrome/browser/glic/widget/glic_widget.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/side_panel/side_panel.h"
#include "chrome/browser/ui/views/side_panel/side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry.h"
#include "chrome/browser/ui/views/tabs/tab.h"
#include "components/tabs/public/tab_interface.h"
#include "components/viz/common/frame_sinks/copy_output_result.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "third_party/skia/include/core/SkRegion.h"
#include "ui/views/view.h"

namespace glic {

GlicSidePanelUi::GlicSidePanelUi(Profile* profile,
                                 base::WeakPtr<tabs::TabInterface> tab,
                                 GlicUiEmbedder::Delegate& delegate,
                                 GlicInstanceMetrics& instance_metrics)
    : profile_(profile),
      tab_(tab),
      delegate_(delegate),
      instance_metrics_(instance_metrics) {
  auto* glic_side_panel_coordinator = GetGlicSidePanelCoordinator();
  if (!glic_side_panel_coordinator) {
    return;
  }

  application_hotkey_manager_ =
      MakeApplicationHotkeyManager(weak_ptr_factory_.GetWeakPtr());
  glic_panel_hotkey_manager_ =
      MakeGlicWindowHotkeyManager(weak_ptr_factory_.GetWeakPtr());

  panel_visibility_subscription_ =
      glic_side_panel_coordinator->AddStateCallback(
          base::BindRepeating(&GlicSidePanelUi::SidePanelStateChanged,
                              weak_ptr_factory_.GetWeakPtr()));

  // If the tab gets moved to a different browser, then this object will be
  // destroyed and a new one will be created, so this subscription will be
  // on the correct window for the lifetime of this object.
  if (auto* browser_window = tab_->GetBrowserWindowInterface()) {
    activation_subscription_ = browser_window->RegisterDidBecomeActive(
        base::BindRepeating(&GlicSidePanelUi::OnBrowserWindowActivated,
                            weak_ptr_factory_.GetWeakPtr()));
    deactivation_subscription_ = browser_window->RegisterDidBecomeInactive(
        base::BindRepeating(&GlicSidePanelUi::OnBrowserWindowDeactivated,
                            weak_ptr_factory_.GetWeakPtr()));
    delegate_->OnEmbedderWindowActivationChanged(browser_window->IsActive());
  }

  glic_side_panel_coordinator->SetContentsView(CreateView(profile_));

  // Add capability to show web modal dialogs (e.g. Data Controls Dialogs for
  // enterprise users) via constrained_window APIs.
  web_modal::WebContentsModalDialogManager::CreateForWebContents(
      delegate_->host().webui_contents());
  web_modal::WebContentsModalDialogManager::FromWebContents(
      delegate_->host().webui_contents())
      ->SetDelegate(this);

  panel_state_.kind = mojom::PanelStateKind::kAttached;
}

std::unique_ptr<views::View> GlicSidePanelUi::CreateView(Profile* profile) {
  auto glic_view =
      std::make_unique<GlicView>(profile, GlicWidget::GetInitialSize(),
                                 glic_panel_hotkey_manager_->GetWeakPtr());
  glic_view->SetWebContents(delegate_->host().webui_contents());
  glic_view->UpdateBackgroundColor();
  glic_view_ = glic_view->GetWeakPtr();
  return glic_view;
}

GlicSidePanelUi::~GlicSidePanelUi() {
  if (glic_view_) {
    glic_view_->SetWebContents(nullptr);
  }
  auto* webui_contents = delegate_->host().webui_contents();
  if (!webui_contents) {
    return;
  }
  auto* dialog_manager =
      web_modal::WebContentsModalDialogManager::FromWebContents(webui_contents);
  if (dialog_manager) {
    dialog_manager->SetDelegate(nullptr);
  }
}

void GlicSidePanelUi::OnClientReady() {
  instance_metrics_->OnClientReady(
      GlicInstanceMetrics::EmbedderType::kSidePanel);
}

Host::EmbedderDelegate* GlicSidePanelUi::GetHostEmbedderDelegate() {
  return this;
}

mojom::PanelState GlicSidePanelUi::GetPanelState() const {
  return panel_state_;
}

gfx::Size GlicSidePanelUi::GetPanelSize() {
  if (!glic_view_) {
    return {};
  }
  return glic_view_->size();
}

void GlicSidePanelUi::Resize(const gfx::Size& size,
                             base::TimeDelta duration,
                             base::OnceClosure callback) {
  NOTIMPLEMENTED();
  std::move(callback).Run();
}

void GlicSidePanelUi::SetDraggableAreas(
    const std::vector<gfx::Rect>& draggable_areas) {
  NOTIMPLEMENTED();
}

void GlicSidePanelUi::SetDraggableRegion(const SkRegion& draggable_region) {
  NOTIMPLEMENTED();
}

void GlicSidePanelUi::EnableDragResize(bool enabled) {
  NOTIMPLEMENTED();
}

void GlicSidePanelUi::Attach() {
  // The Side Panel Ui is already attached, do nothing.
}

void GlicSidePanelUi::Detach() {
  if (!tab_) {
    return;
  }
  // NOTE: `this` will be destroyed after this call.
  delegate_->Detach(*tab_);
}

void GlicSidePanelUi::SetMinimumWidgetSize(const gfx::Size& size) {
  NOTIMPLEMENTED();
}

bool GlicSidePanelUi::IsShowing() const {
  auto* glic_side_panel_coordinator = GetGlicSidePanelCoordinator();
  if (!glic_side_panel_coordinator) {
    return false;
  }
  return glic_side_panel_coordinator->IsShowing();
}

void GlicSidePanelUi::Focus() {
  // When daisy chaining focus request is lost when opening a new tab.
  // Wait a little for things to settle.
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&GlicSidePanelUi::SetFocusDelayed,
                     weak_ptr_factory_.GetWeakPtr()),
      base::Milliseconds(50));
}

void GlicSidePanelUi::SetFocusDelayed() {
  if (auto* web_contents = delegate_->host().webui_contents()) {
    web_contents->Focus();
  }
}

void GlicSidePanelUi::SidePanelStateChanged(
    GlicSidePanelCoordinator::State state) {
  // Showing only happens through glic entrypoint, hiding can also be triggered
  // by side panel coordinator when replacing glic with another entry.
  if (state != GlicSidePanelCoordinator::State::kShown && tab_) {
    instance_metrics_->OnSidePanelClosed(tab_.get());
    panel_state_.kind = mojom::PanelStateKind::kHidden;
    delegate_->NotifyPanelStateChanged();
    // NOTE: `this` will be destroyed after this call.
    delegate_->WillCloseFor(tab_.get());
  }
}

void GlicSidePanelUi::SwitchConversation(
    glic::mojom::ConversationInfoPtr info,
    mojom::WebClientHandler::SwitchConversationCallback callback) {
  // NOTE: `this` may be destroyed after this call.
  delegate_->SwitchConversation(ShowOptions::ForSidePanel(*tab_),
                                std::move(info), std::move(callback));
}

void GlicSidePanelUi::CaptureScreenshot(
    glic::mojom::WebClientHandler::CaptureScreenshotCallback callback) {
  if (!tab_) {
    std::move(callback).Run(nullptr);
  }
  if (!screenshot_capturer_) {
    screenshot_capturer_ = std::make_unique<GlicScreenshotCapturer>();
  }
  auto* browser_window = tab_->GetBrowserWindowInterface();
  CHECK(browser_window);
  screenshot_capturer_->CaptureScreenshot(
      browser_window->GetWindow()->GetNativeWindow(), std::move(callback));
}

void GlicSidePanelUi::Show(const ShowOptions& options) {
  instance_metrics_->OnShowInSidePanel(tab_.get());
  auto* glic_side_panel_coordinator = GetGlicSidePanelCoordinator();
  if (!glic_side_panel_coordinator) {
    return;
  }
  panel_state_.kind = mojom::PanelStateKind::kAttached;
  delegate_->NotifyPanelStateChanged();
  delegate_->host().FloatingPanelCanAttachChanged(false);
  application_hotkey_manager_->InitializeAccelerators();
  glic_panel_hotkey_manager_->InitializeAccelerators();

  bool suppress_animations = false;
  if (const auto* side_panel_options =
          std::get_if<SidePanelShowOptions>(&options.embedder_options)) {
    suppress_animations = side_panel_options->suppress_opening_animation;
  }
  glic_side_panel_coordinator->Show(suppress_animations);
}

void GlicSidePanelUi::Close() {
  if (screenshot_capturer_) {
    screenshot_capturer_->CloseScreenPicker();
  }
  auto* glic_side_panel_coordinator = GetGlicSidePanelCoordinator();
  if (!glic_side_panel_coordinator || !IsShowing()) {
    return;
  }
  // NOTE: `this` will be destroyed after this call.
  glic_side_panel_coordinator->Close();
}

void GlicSidePanelUi::ClosePanel() {
  Close();
}

std::unique_ptr<GlicUiEmbedder> GlicSidePanelUi::CreateInactiveEmbedder()
    const {
  return GlicInactiveSidePanelUi::CreateForVisibleTab(tab_, delegate_.get());
}

void GlicSidePanelUi::FocusIfOpen() {
  if (IsShowing()) {
    Focus();
  }
}

bool GlicSidePanelUi::HasFocus() {
  if (!glic_view_) {
    return false;
  }
  return glic_view_->HasFocus();
}

bool GlicSidePanelUi::ActivateBrowser() {
  if (!tab_) {
    return false;
  }
  tab_->GetContents()->Focus();
  return true;
}

void GlicSidePanelUi::ShowTitleBarContextMenuAt(gfx::Point event_loc) {
  // This is floaty-specific. It doesn't make sense in side panel.
}

base::WeakPtr<views::View> GlicSidePanelUi::GetView() {
  return glic_view_;
}

// web_modal::WebContentsModalDialogManagerDelegate
web_modal::WebContentsModalDialogHost*
GlicSidePanelUi::GetWebContentsModalDialogHost(
    content::WebContents* web_contents) {
  return tab_->GetBrowserWindowInterface()
      ->GetWebContentsModalDialogHostForWindow();
}

void GlicSidePanelUi::OnBrowserWindowActivated(BrowserWindowInterface* bwi) {
  delegate_->OnEmbedderWindowActivationChanged(true);
}

void GlicSidePanelUi::OnBrowserWindowDeactivated(BrowserWindowInterface* bwi) {
  delegate_->OnEmbedderWindowActivationChanged(false);
}

GlicSidePanelCoordinator* GlicSidePanelUi::GetGlicSidePanelCoordinator() const {
  if (!tab_ || !tab_->GetTabFeatures()) {
    return nullptr;
  }
  return tab_->GetTabFeatures()->glic_side_panel_coordinator();
}

std::string GlicSidePanelUi::DescribeForTesting() {
  return base::StrCat({"SidePanelUi for tab ",
                       base::NumberToString(tab_->GetHandle().raw_value())});
}

}  // namespace glic

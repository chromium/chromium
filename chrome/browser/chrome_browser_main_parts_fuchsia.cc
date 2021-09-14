// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_browser_main_parts_fuchsia.h"

#include <fuchsia/ui/app/cpp/fidl.h>
#include <lib/sys/cpp/component_context.h>
#include <lib/ui/scenic/cpp/commands.h>
#include <lib/ui/scenic/cpp/resources.h>
#include <lib/ui/scenic/cpp/session.h>

#include "base/bind.h"
#include "base/check.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/fuchsia/process_context.h"
#include "base/fuchsia/scoped_service_binding.h"
#include "base/notreached.h"
#include "ui/platform_window/fuchsia/initialize_presenter_api_view.h"

namespace {

fuchsia::ui::views::ViewRef CloneViewRef(
    const fuchsia::ui::views::ViewRef& view_ref) {
  fuchsia::ui::views::ViewRef dup;
  zx_status_t status =
      view_ref.reference.duplicate(ZX_RIGHT_SAME_RIGHTS, &dup.reference);
  ZX_CHECK(status == ZX_OK, status) << "zx_object_duplicate";
  return dup;
}

struct SubViewData {
  scenic::ViewHolder view_holder;
  fuchsia::ui::views::ViewRef view_ref;
};

// ViewProvider implementation that provides a single view and exposes all
// requested view from PlatformOzoneScenic inside it.
class ViewProviderScenic : public fuchsia::ui::app::ViewProvider {
 public:
  ViewProviderScenic()
      : binding_(base::ComponentContextForProcess()->outgoing().get(), this),
        scenic_(base::ComponentContextForProcess()
                    ->svc()
                    ->Connect<fuchsia::ui::scenic::Scenic>()),
        scenic_session_(scenic_.get(), focuser_.NewRequest()) {
    scenic_.set_error_handler([](zx_status_t status) {
      ZX_LOG(ERROR, status) << " Scenic lost.";
      // Terminate here so that e.g. a Scenic crash results in the browser
      // immediately terminating, without generating a cascading crash report.
      base::Process::TerminateCurrentProcessImmediately(1);
    });
    scenic_session_.set_event_handler(
        fit::bind_member(this, &ViewProviderScenic::OnScenicEvents));
  }
  ~ViewProviderScenic() override = default;

  // fuchsia::ui::app::ViewProvider overrides.
  void CreateView(
      zx::eventpair token,
      fidl::InterfaceRequest<fuchsia::sys::ServiceProvider> incoming_services,
      fidl::InterfaceHandle<fuchsia::sys::ServiceProvider> outgoing_services)
      override {
    CreateViewWithViewRef(std::move(token), {}, {});
  }
  void CreateViewWithViewRef(zx::eventpair token,
                             fuchsia::ui::views::ViewRefControl control_ref,
                             fuchsia::ui::views::ViewRef view_ref) override {
    if (view_) {
      LOG(WARNING) << "Unexpected spurious call to |CreateViewWithViewRef|. "
                      "Deleting previously created view.";
      subviews_.clear();
      is_node_attached_ = false;
      view_has_focus_ = false;
      node_.reset();
      view_.reset();
      view_properties_ = absl::nullopt;
    }

    view_ = std::make_unique<scenic::View>(
        &scenic_session_, fuchsia::ui::views::ViewToken({std::move(token)}),
        std::move(control_ref), std::move(view_ref), "root-view");
    node_ = std::make_unique<scenic::EntityNode>(&scenic_session_);
    for (auto& subview : subviews_) {
      node_->AddChild(subview.view_holder);
    }
    Present();
  }

  void PresentView(fuchsia::ui::views::ViewHolderToken view_holder_token,
                   fuchsia::ui::views::ViewRef view_ref) {
    SubViewData subview = {
        .view_holder = scenic::ViewHolder(&scenic_session_,
                                          std::move(view_holder_token).value,
                                          "subview-holder"),
        .view_ref = std::move(view_ref)};
    if (view_) {
      if (view_properties_) {
        subview.view_holder.SetViewProperties(*view_properties_);
      }
      node_->AddChild(subview.view_holder);
      Present();
    }
    subviews_.push_back(std::move(subview));
  }

 private:
  void OnScenicEvents(std::vector<fuchsia::ui::scenic::Event> events) {
    for (const auto& event : events) {
      if (event.is_gfx() && event.gfx().is_view_properties_changed()) {
        if (event.gfx().view_properties_changed().view_id != view_->id()) {
          LOG(WARNING) << "Received event for unknown view.";
          return;
        }
        UpdateViewProperties(event.gfx().view_properties_changed().properties);
      } else if (event.is_input() && event.input().is_focus()) {
        view_has_focus_ = event.input().focus().focused;
        FocusView();
      }
    }
  }

  void UpdateViewProperties(
      const fuchsia::ui::gfx::ViewProperties& view_properties) {
    const float width =
        view_properties.bounding_box.max.x - view_properties.bounding_box.min.x;
    const float height =
        view_properties.bounding_box.max.y - view_properties.bounding_box.min.y;
    if (width == 0 || height == 0) {
      if (is_node_attached_) {
        node_->Detach();
        is_node_attached_ = false;
      }
    } else {
      if (!is_node_attached_) {
        view_->AddChild(*node_);
        is_node_attached_ = true;
      }
    }

    view_properties_ = view_properties;
    for (auto& subview : subviews_) {
      subview.view_holder.SetViewProperties(*view_properties_);
    }
    Present();
  }

  void FocusView(int tries = 2) {
    if (tries == 0) {
      LOG(ERROR) << "Unable to pass focus to chrome window.";
      return;
    }
    if (!subviews_.empty() && view_has_focus_) {
      focuser_->RequestFocus({CloneViewRef(subviews_.front().view_ref)},
                             [this, tries](auto result) {
                               if (result.is_err()) {
                                 FocusView(tries - 1);
                               }
                             });
    }
  }

  void Present() {
    scenic_session_.Present(
        zx_clock_get_monotonic(),
        [this](fuchsia::images::PresentationInfo info) { FocusView(); });
  }

  const base::ScopedServiceBinding<fuchsia::ui::app::ViewProvider> binding_;

  fuchsia::ui::scenic::ScenicPtr scenic_;
  fuchsia::ui::views::FocuserPtr focuser_;
  scenic::Session scenic_session_;

  // The view created by this ViewProvider. The view is created lazily when a
  // request is received.
  std::unique_ptr<scenic::View> view_;

  // Entity node for the |view_|.
  std::unique_ptr<scenic::EntityNode> node_;

  // True if the root EntityNode has been added to the View.
  bool is_node_attached_ = false;

  // True is the root view has focus.
  bool view_has_focus_ = false;

  // The holders for all the views that are presented.
  std::vector<SubViewData> subviews_;

  // The properties of the top level view. They are forwarded to the embedded
  // views.
  absl::optional<fuchsia::ui::gfx::ViewProperties> view_properties_;
};

}  // namespace

ChromeBrowserMainPartsFuchsia::ChromeBrowserMainPartsFuchsia(
    const content::MainFunctionParams& parameters,
    StartupData* startup_data)
    : ChromeBrowserMainParts(parameters, startup_data) {}

void ChromeBrowserMainPartsFuchsia::ShowMissingLocaleMessageBox() {
  // Locale data should be bundled for all possible platform locales,
  // so crash here to make missing-locale states more visible.
  CHECK(false);
}

int ChromeBrowserMainPartsFuchsia::PreMainMessageLoopRun() {
  // Register the ViewPresenter API.
  ui::fuchsia::SetScenicViewPresenter(
      base::BindRepeating(&ViewProviderScenic::PresentView,
                          std::make_unique<ViewProviderScenic>()));

  zx_status_t status =
      base::ComponentContextForProcess()->outgoing()->ServeFromStartupInfo();
  ZX_CHECK(status == ZX_OK, status);

  return ChromeBrowserMainParts::PreMainMessageLoopRun();
}

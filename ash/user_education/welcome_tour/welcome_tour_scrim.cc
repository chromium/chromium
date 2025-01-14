// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/user_education/welcome_tour/welcome_tour_scrim.h"

#include <array>
#include <vector>

#include "ash/display/window_tree_host_manager.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/style/ash_color_provider_source.h"
#include "ash/user_education/user_education_help_bubble_controller.h"
#include "ash/user_education/user_education_types.h"
#include "base/callback_list.h"
#include "base/check.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/core/SkRRect.h"
#include "third_party/skia/include/core/SkRect.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/color/color_provider.h"
#include "ui/color/color_provider_source_observer.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_delegate.h"
#include "ui/compositor/layer_owner.h"
#include "ui/compositor/paint_recorder.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/gfx/geometry/rrect_f.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace ash {
namespace {

// Used to ensure existence of only a single `WelcomeTourScrim` at a time.
WelcomeTourScrim* g_instance = nullptr;

// Appearance.
constexpr float kBlurSigma = 3.f;
constexpr float kClipCornerRadius = 24.f;

// Helpers ---------------------------------------------------------------------

gfx::RectF TranslateRectFromScreen(const aura::Window* root_window,
                                   const gfx::Rect& rect_in_screen) {
  gfx::RectF rect_in_root_window(rect_in_screen);
  ::wm::TranslateRectFromScreen(root_window, &rect_in_root_window);
  return rect_in_root_window;
}

std::vector<gfx::RectF> GetHelpBubbleAnchorBoundsInRootWindow(
    const aura::Window* root_window) {
  std::vector<gfx::RectF> bounds_in_root_window;
  if (auto* ctrl = UserEducationHelpBubbleController::Get()) {
    for (const auto& [_, metadata] : ctrl->help_bubble_metadata_by_key()) {
      if (metadata.anchor_root_window == root_window) {
        bounds_in_root_window.emplace_back(TranslateRectFromScreen(
            metadata.anchor_root_window, metadata.anchor_bounds_in_screen));
      }
    }
  }
  return bounds_in_root_window;
}

// MaskLayerOwner --------------------------------------------------------------

// The class which owns the mask layer for a `WelcomeTour::Scrim`. The mask
// layer is responsible for clipping the scrim around help bubble anchor views
// so that they are emphasized by the scrim and not obstructed by it.
class MaskLayerOwner : public ui::LayerOwner, public ui::LayerDelegate {
 public:
  explicit MaskLayerOwner(const aura::Window* root_window)
      : ui::LayerOwner(std::make_unique<ui::Layer>(ui::LAYER_TEXTURED)),
        root_window_(root_window) {
    Init();
  }

  MaskLayerOwner(const MaskLayerOwner&) = delete;
  MaskLayerOwner& operator=(const MaskLayerOwner&) = delete;
  ~MaskLayerOwner() override = default;

 private:
  // ui::LayerDelegate:
  void OnDeviceScaleFactorChanged(float old_device_scale_factory,
                                  float new_device_scale_factor) override {
    Invalidate();
  }

  void OnPaintLayer(const ui::PaintContext& context) override {
    // In the absence of help bubble anchor views, the scrim should be fully
    // visible. As such, the mask layer for the scrim should be fully opaque.
    gfx::SizeF size(layer()->size());
    SkPath path(SkPath::Rect(gfx::RectFToSkRect(gfx::RectF(size)),
                             SkPathDirection::kCW));

    // Clip the otherwise fully opaque mask layer around help bubble anchor
    // views so that they are emphasized by the scrim and not obstructed by it.
    for (const auto& bounds :
         GetHelpBubbleAnchorBoundsInRootWindow(root_window_)) {
      path.addRRect(
          SkRRect(gfx::RRectF(bounds, gfx::RoundedCornersF(kClipCornerRadius))),
          SkPathDirection::kCCW);
    }

    // Configure `canvas`.
    ui::PaintRecorder recorder(context, gfx::ToFlooredSize(size));
    gfx::Canvas* const canvas = recorder.canvas();

    // Configure `flags`.
    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    flags.setColor(SK_ColorBLACK);
    flags.setStyle(cc::PaintFlags::kFill_Style);

    // Draw `path`.
    canvas->DrawPath(path, flags);
  }

  // Invoked once to initialize `this`.
  void Init() {
    // Configure static mask layer properties.
    layer()->SetFillsBoundsOpaquely(false);
    layer()->SetName(WelcomeTourScrim::kMaskLayerName);
    layer()->set_delegate(this);

    // Invalidate the mask layer in response to help bubble related events in
    // order to clip the scrim around help bubble anchor views so that they are
    // emphasized by the scrim and not obstructed by it.
    if (auto* controller = UserEducationHelpBubbleController::Get()) {
      base::RepeatingCallback invalidate = base::BindRepeating(
          &MaskLayerOwner::Invalidate, base::Unretained(this));
      help_bubble_event_subscriptions_[0] =
          controller->AddHelpBubbleAnchorBoundsChangedCallback(invalidate);
      help_bubble_event_subscriptions_[1] =
          controller->AddHelpBubbleClosedCallback(invalidate);
      help_bubble_event_subscriptions_[2] =
          controller->AddHelpBubbleShownCallback(invalidate);
    }
  }

  // Invoked as needed to schedule repaint of the mask layer.
  void Invalidate() { layer()->SchedulePaint(gfx::Rect(layer()->size())); }

  // Pointer to the root window associated with `this` mask.
  const raw_ptr<const aura::Window> root_window_;

  // Subscriptions for help bubble related events which are held for the life of
  // `this` object in order to clip the scrim around help bubble anchor views so
  // that they are emphasized by the scrim and not obstructed by it.
  std::array<base::CallbackListSubscription, 3>
      help_bubble_event_subscriptions_;
};

}  // namespace

// WelcomeTourScrim::Scrim -----------------------------------------------------

// The class which applies a scrim to the help bubble container for a single
// root window while in existence. On destruction, the scrim for the associated
// root window is automatically removed.
class WelcomeTourScrim::Scrim : public aura::WindowObserver,
                                public ui::ColorProviderSourceObserver {
 public:
  explicit Scrim(aura::Window* root_window)
      : root_window_(root_window),
        layer_owner_(std::make_unique<ui::Layer>(ui::LAYER_SOLID_COLOR)),
        mask_layer_owner_(root_window) {
    Init();
  }

  Scrim(const Scrim&) = delete;
  Scrim& operator=(const Scrim&) = delete;
  ~Scrim() override = default;

 private:
  // aura::WindowObserver:
  void OnWindowBoundsChanged(aura::Window* window,
                             const gfx::Rect& old_bounds,
                             const gfx::Rect& new_bounds,
                             ui::PropertyChangeReason reason) override {
    UpdateBounds();
  }

  void OnWindowDestroying(aura::Window* window) override {
    window_observation_.Reset();
  }

  // ui::ColorProviderSourceObserver:
  void OnColorProviderChanged() override { UpdateColor(); }

  // Returns the help bubble container associated with `this` scrim.
  aura::Window* GetHelpBubbleContainer() {
    return root_window_->GetChildById(kShellWindowId_HelpBubbleContainer);
  }

  // Returns the root window controller associated with `this` scrim.
  RootWindowController* GetRootWindowController() {
    return RootWindowController::ForWindow(root_window_);
  }

  // Invoked once to initialize `this` scrim.
  void Init() {
    // Configure static scrim layer properties.
    layer_owner_.layer()->SetFillsBoundsOpaquely(false);
    layer_owner_.layer()->SetMaskLayer(mask_layer_owner_.layer());
    layer_owner_.layer()->SetName(WelcomeTourScrim::kLayerName);

    // Configure dynamic scrim layer properties.
    UpdateBlur();
    UpdateBounds();
    UpdateColor();

    // Add the scrim layer to the bottom of the `help_bubble_container`.
    aura::Window* const help_bubble_container = GetHelpBubbleContainer();
    help_bubble_container->layer()->Add(layer_owner_.layer());
    help_bubble_container->layer()->StackAtBottom(layer_owner_.layer());

    // Update blur in response to help bubble related events in order to apply
    // blur behind the scrim layer if and only if help bubbles are present.
    if (auto* controller = UserEducationHelpBubbleController::Get()) {
      base::RepeatingCallback update_blur = base::BindRepeating(
          &WelcomeTourScrim::Scrim::UpdateBlur, base::Unretained(this));
      help_bubble_event_subscriptions_[0] =
          controller->AddHelpBubbleClosedCallback(update_blur);
      help_bubble_event_subscriptions_[1] =
          controller->AddHelpBubbleShownCallback(update_blur);
    }

    // Observe the `help_bubble_container` and associated color provider source
    // so that dynamic scrim layer properties can be updated appropriately.
    window_observation_.Observe(help_bubble_container);
    Observe(GetRootWindowController()->color_provider_source());
  }

  // Invoked to update blur applied behind the scrim layer. Note that blur is
  // applied if and only if help bubbles are present.
  void UpdateBlur() {
    if (auto* controller = UserEducationHelpBubbleController::Get()) {
      layer_owner_.layer()->SetBackgroundBlur(
          controller->help_bubble_metadata_by_key().empty() ? 0.f : kBlurSigma);
    }
  }

  // Invoked to update bounds of the scrim and mask layers. Note that scrim and
  // mask layer bounds must remain in sync.
  void UpdateBounds() {
    const gfx::Rect bounds(GetHelpBubbleContainer()->bounds().size());
    layer_owner_.layer()->SetBounds(bounds);
    mask_layer_owner_.layer()->SetBounds(bounds);
  }

  // Invoked to update color of the scrim layer.
  void UpdateColor() {
    layer_owner_.layer()->SetColor(GetRootWindowController()
                                       ->color_provider_source()
                                       ->GetColorProvider()
                                       ->GetColor(cros_tokens::kCrosSysScrim));
  }

  // Pointer to the root window associated with `this` scrim.
  const raw_ptr<aura::Window> root_window_;

  // Owner for the scrim layer applied to the associated help bubble container.
  ui::LayerOwner layer_owner_;

  // Owner for the mask layer which is applied to the scrim layer to clip the
  // scrim around help bubble anchor views so that they are emphasized by the
  // scrim and not obstructed by it.
  MaskLayerOwner mask_layer_owner_;

  // Subscriptions for help bubble related events which are held for the life of
  // `this` object in order to apply blur behind the scrim layer if and only if
  // help bubbles are present.
  std::array<base::CallbackListSubscription, 2>
      help_bubble_event_subscriptions_;

  // Used to observe the associated help bubble container in order to keep the
  // bounds of the scrim layer in sync.
  base::ScopedObservation<aura::Window, aura::WindowObserver>
      window_observation_{this};
};

// WelcomeTourScrim ------------------------------------------------------------

WelcomeTourScrim::WelcomeTourScrim() {
  CHECK_EQ(g_instance, nullptr);
  g_instance = this;

  // Cache `shell` and associated window tree host manager.
  auto* shell = Shell::Get();
  CHECK(shell);
  auto* window_tree_host_mgr = shell->window_tree_host_manager();
  CHECK(window_tree_host_mgr);

  // Create a scrim for every root window.
  for (aura::Window* root_window : window_tree_host_mgr->GetAllRootWindows()) {
    Init(root_window);
  }

  // Observe `shell` so that scrims can be dynamically created/destroyed when
  // root windows are added/removed.
  shell_observation_.Observe(shell);
}

WelcomeTourScrim::~WelcomeTourScrim() {
  CHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

void WelcomeTourScrim::OnRootWindowAdded(aura::Window* root_window) {
  Init(root_window);
}

void WelcomeTourScrim::OnRootWindowWillShutdown(aura::Window* root_window) {
  Reset(root_window);
}

void WelcomeTourScrim::OnShellDestroying() {
  auto* window_tree_host_mgr = Shell::Get()->window_tree_host_manager();
  CHECK(window_tree_host_mgr);

  // Destroy scrims for every root window.
  for (aura::Window* root_window : window_tree_host_mgr->GetAllRootWindows()) {
    Reset(root_window);
  }

  shell_observation_.Reset();
}

void WelcomeTourScrim::Init(aura::Window* root_window) {
  auto [it, inserted] = scrims_by_root_window_.emplace(
      root_window, std::make_unique<Scrim>(root_window));
  CHECK(inserted);
}

void WelcomeTourScrim::Reset(aura::Window* root_window) {
  scrims_by_root_window_.erase(root_window);
}

}  // namespace ash

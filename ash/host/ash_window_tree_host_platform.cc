// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/host/ash_window_tree_host_platform.h"

#include <utility>

#include "ash/host/root_window_transformer.h"
#include "ash/host/transformer_helper.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/window_factory.h"
#include "ash/ws/window_service_owner.h"
#include "base/feature_list.h"
#include "base/trace_event/trace_event.h"
#include "services/ws/host_event_queue.h"
#include "services/ws/public/cpp/input_devices/input_device_controller_client.h"
#include "services/ws/public/mojom/window_manager.mojom.h"
#include "services/ws/window_service.h"
#include "ui/aura/mus/input_method_mus.h"
#include "ui/aura/null_window_targeter.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host_platform.h"
#include "ui/base/ui_base_features.h"
#include "ui/events/event_sink.h"
#include "ui/events/null_event_targeter.h"
#include "ui/events/ozone/chromeos/cursor_controller.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/transform.h"
#include "ui/platform_window/mojo/ime_type_converters.h"
#include "ui/platform_window/platform_ime_controller.h"
#include "ui/platform_window/platform_window.h"
#include "ui/platform_window/platform_window_init_properties.h"
#include "ui/platform_window/text_input_state.h"

namespace ash {
namespace {

// String passed to Compositor to identify who is submitting compositor frames.
// Used by telemetry.
const char* kTraceEnvironmentName = "ash";

}  // namespace

AshWindowTreeHostPlatform::AshWindowTreeHostPlatform(
    ui::PlatformWindowInitProperties properties)
    : aura::WindowTreeHostPlatform(
          std::move(properties),
          window_factory::NewWindow(),
          ::features::IsUsingWindowService() ? kTraceEnvironmentName : nullptr),
      transformer_helper_(this) {
  CommonInit();
}

AshWindowTreeHostPlatform::AshWindowTreeHostPlatform()
    : aura::WindowTreeHostPlatform(window_factory::NewWindow()),
      transformer_helper_(this) {
  CreateCompositor(
      viz::FrameSinkId(),
      /* force_software_compositor */ false,
      /* external_begin_frames_enabled */ false,
      /* are_events_in_pixels */ true,
      ::features::IsUsingWindowService() ? kTraceEnvironmentName : nullptr);
  CommonInit();
}

AshWindowTreeHostPlatform::~AshWindowTreeHostPlatform() = default;

void AshWindowTreeHostPlatform::ConfineCursorToRootWindow() {
  if (!allow_confine_cursor())
    return;

  gfx::Rect confined_bounds(GetBoundsInPixels().size());
  confined_bounds.Inset(transformer_helper_.GetHostInsets());
  last_cursor_confine_bounds_in_pixels_ = confined_bounds;
  platform_window()->ConfineCursorToBounds(confined_bounds);
}

void AshWindowTreeHostPlatform::ConfineCursorToBoundsInRoot(
    const gfx::Rect& bounds_in_root) {
  if (!allow_confine_cursor())
    return;

  gfx::RectF bounds_f(bounds_in_root);
  GetRootTransform().TransformRect(&bounds_f);
  last_cursor_confine_bounds_in_pixels_ = gfx::ToEnclosingRect(bounds_f);
  platform_window()->ConfineCursorToBounds(
      last_cursor_confine_bounds_in_pixels_);
}

gfx::Rect AshWindowTreeHostPlatform::GetLastCursorConfineBoundsInPixels()
    const {
  return last_cursor_confine_bounds_in_pixels_;
}

void AshWindowTreeHostPlatform::SetCursorConfig(
    const display::Display& display,
    display::Display::Rotation rotation) {
  // Scale all motion on High-DPI displays.
  float scale = display.device_scale_factor();

  if (!display.IsInternal())
    scale *= 1.2;

  ui::CursorController::GetInstance()->SetCursorConfigForWindow(
      GetAcceleratedWidget(), rotation, scale);
}

void AshWindowTreeHostPlatform::ClearCursorConfig() {
  ui::CursorController::GetInstance()->ClearCursorConfigForWindow(
      GetAcceleratedWidget());
}

void AshWindowTreeHostPlatform::UpdateTextInputState(
    ui::mojom::TextInputStatePtr state) {
  SetTextInputState(std::move(state));
}

void AshWindowTreeHostPlatform::UpdateImeVisibility(
    bool visible,
    ui::mojom::TextInputStatePtr state) {
  SetImeVisibility(visible, std::move(state));
}

void AshWindowTreeHostPlatform::SetRootWindowTransformer(
    std::unique_ptr<RootWindowTransformer> transformer) {
  transformer_helper_.SetRootWindowTransformer(std::move(transformer));
  ConfineCursorToRootWindow();
}

gfx::Insets AshWindowTreeHostPlatform::GetHostInsets() const {
  return transformer_helper_.GetHostInsets();
}

aura::WindowTreeHost* AshWindowTreeHostPlatform::AsWindowTreeHost() {
  return this;
}

void AshWindowTreeHostPlatform::PrepareForShutdown() {
  // Block the root window from dispatching events because it is weird for a
  // ScreenPositionClient not to be attached to the root window and for
  // ui::EventHandlers to be unable to convert the event's location to screen
  // coordinates.
  window()->SetEventTargeter(std::make_unique<aura::NullWindowTargeter>());

  // Do anything platform specific necessary before shutdown (eg. stop
  // listening for configuration XEvents).
  platform_window()->PrepareForShutdown();
}

void AshWindowTreeHostPlatform::SetRootTransform(
    const gfx::Transform& transform) {
  transformer_helper_.SetTransform(transform);
}

gfx::Transform AshWindowTreeHostPlatform::GetRootTransform() const {
  return transformer_helper_.GetTransform();
}

gfx::Transform AshWindowTreeHostPlatform::GetInverseRootTransform() const {
  return transformer_helper_.GetInverseTransform();
}

gfx::Rect AshWindowTreeHostPlatform::GetTransformedRootWindowBoundsInPixels(
    const gfx::Size& host_size_in_pixels) const {
  return transformer_helper_.GetTransformedWindowBounds(host_size_in_pixels);
}

void AshWindowTreeHostPlatform::OnCursorVisibilityChangedNative(bool show) {
  SetTapToClickPaused(!show);
}

void AshWindowTreeHostPlatform::SetBoundsInPixels(
    const gfx::Rect& bounds,
    const viz::LocalSurfaceId& local_surface_id,
    base::TimeTicks allocation_time) {
  WindowTreeHostPlatform::SetBoundsInPixels(bounds, local_surface_id,
                                            allocation_time);
  ConfineCursorToRootWindow();
}

void AshWindowTreeHostPlatform::DispatchEvent(ui::Event* event) {
  host_event_queue_->DispatchOrQueueEvent(event);
}

void AshWindowTreeHostPlatform::CommonInit() {
  transformer_helper_.Init();

  host_event_queue_ = Shell::Get()
                          ->window_service_owner()
                          ->window_service()
                          ->RegisterHostEventDispatcher(this, this);

  if (!base::FeatureList::IsEnabled(features::kMash))
    return;

  input_method_ = std::make_unique<aura::InputMethodMus>(this, this);
  input_method_->Init(Shell::Get()->connector());
  SetSharedInputMethod(input_method_.get());
}

void AshWindowTreeHostPlatform::DispatchEventFromQueue(ui::Event* event) {
  TRACE_EVENT0("input", "AshWindowTreeHostPlatform::DispatchEvent");
  if (event->IsLocatedEvent())
    TranslateLocatedEvent(static_cast<ui::LocatedEvent*>(event));
  SendEventToSink(event);
}

void AshWindowTreeHostPlatform::SetTapToClickPaused(bool state) {
  ws::InputDeviceControllerClient* input_device_controller_client =
      Shell::Get()->shell_delegate()->GetInputDeviceControllerClient();
  if (!input_device_controller_client)
    return;  // Happens in tests.

  // Temporarily pause tap-to-click when the cursor is hidden.
  input_device_controller_client->SetTapToClickPaused(state);
}

bool AshWindowTreeHostPlatform::ShouldSendKeyEventToIme() {
  // Don't send key events to IME if they are going to go to a remote client.
  // Remote clients handle forwarding to IME (as necessary).
  aura::Window* target = window()->targeter()->FindTargetForKeyEvent(window());
  return !target || !ws::WindowService::HasRemoteClient(target);
}

void AshWindowTreeHostPlatform::SetTextInputState(
    ui::mojom::TextInputStatePtr state) {
  ui::PlatformImeController* ime =
      platform_window()->GetPlatformImeController();
  if (ime)
    ime->UpdateTextInputState(state.To<ui::TextInputState>());
}

void AshWindowTreeHostPlatform::SetImeVisibility(
    bool visible,
    ui::mojom::TextInputStatePtr state) {
  if (!state.is_null())
    SetTextInputState(std::move(state));

  ui::PlatformImeController* ime =
      platform_window()->GetPlatformImeController();
  if (ime)
    ime->SetImeVisibility(visible);
}

}  // namespace ash

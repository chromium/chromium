// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr/browser_renderer_factory.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/android/vr/cardboard_input_delegate.h"
#include "chrome/browser/android/vr/gvr_input_delegate.h"
#include "chrome/browser/android/vr/gvr_scheduler_delegate.h"
#include "chrome/browser/android/vr/ui_factory.h"
#include "chrome/browser/android/vr/vr_gl_thread.h"
#include "chrome/browser/vr/browser_renderer.h"
#include "chrome/browser/vr/sounds_manager_audio_delegate.h"
#include "chrome/browser/vr/ui_interface.h"

namespace {
// Number of frames to use for sliding averages for pose timings,
// as used for estimating prediction times.
constexpr unsigned kSlidingAverageSize = 5;
}  // namespace

namespace vr {

BrowserRendererFactory::Params::Params(
    gvr::GvrApi* gvr_api,
    const UiInitialState& ui_initial_state,
    bool reprojected_rendering,
    bool cardboard_gamepad,
    base::WaitableEvent* gl_surface_created_event,
    base::OnceCallback<gfx::AcceleratedWidget()> surface_callback)
    : gvr_api(gvr_api),
      ui_initial_state(ui_initial_state),
      reprojected_rendering(reprojected_rendering),
      cardboard_gamepad(cardboard_gamepad),
      gl_surface_created_event(gl_surface_created_event),
      surface_callback(std::move(surface_callback)) {}

BrowserRendererFactory::Params::~Params() = default;

std::unique_ptr<BrowserRenderer> BrowserRendererFactory::Create(
    VrGLThread* vr_gl_thread,
    UiFactory* ui_factory,
    std::unique_ptr<Params> params) {
  DCHECK(params);
  params->ui_initial_state.gvr_input_support = !params->cardboard_gamepad;

  auto ui = ui_factory->Create(vr_gl_thread, params->ui_initial_state);
  std::unique_ptr<InputDelegate> input_delegate;
  if (params->cardboard_gamepad) {
    input_delegate = std::make_unique<CardboardInputDelegate>(params->gvr_api);
  } else {
    input_delegate = std::make_unique<GvrInputDelegate>(params->gvr_api);
  }
  auto graphics_delegate = std::make_unique<GvrGraphicsDelegate>(
      vr_gl_thread,
      base::BindOnce(&UiInterface::OnGlInitialized, base::Unretained(ui.get())),
      params->gvr_api, params->reprojected_rendering, kSlidingAverageSize);
  auto scheduler_delegate = std::make_unique<GvrSchedulerDelegate>(
      vr_gl_thread, ui->GetSchedulerUiPtr(), params->gvr_api,
      graphics_delegate.get(), params->cardboard_gamepad, kSlidingAverageSize);
  graphics_delegate->set_webxr_presentation_state(scheduler_delegate->webxr());
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&GvrGraphicsDelegate::Init,
                     graphics_delegate->GetWeakPtr(),
                     base::Unretained(params->gl_surface_created_event),
                     std::move(params->surface_callback)));
  auto browser_renderer = std::make_unique<BrowserRenderer>(
      std::move(ui), std::move(scheduler_delegate),
      std::move(graphics_delegate), std::move(input_delegate), vr_gl_thread,
      kSlidingAverageSize);
  return browser_renderer;
}

}  // namespace vr

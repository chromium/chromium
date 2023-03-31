// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ROUNDED_DISPLAY_ROUNDED_DISPLAY_HOST_H_
#define ASH_ROUNDED_DISPLAY_ROUNDED_DISPLAY_HOST_H_

#include <memory>
#include <vector>

#include "ash/frame_sink/frame_sink_host.h"
#include "ash/rounded_display/rounded_display_gutter.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"

namespace viz {
class CompositorFrame;
}  // namespace viz

namespace ash {
class RoundedDisplayFrameFactory;

// Renders the rounded-display mask textures by creating independent
// compositor frames and submitting them to the display compositor.
class ASH_EXPORT RoundedDisplayHost : public ash::FrameSinkHost {
 public:
  using GetGuttersCallback =
      base::RepeatingCallback<void(std::vector<RoundedDisplayGutter*>&)>;

  // The callback is used to get RoundedDisplayGutters that are used to paint
  // mask textures for compositor frames.
  explicit RoundedDisplayHost(GetGuttersCallback callback);

  RoundedDisplayHost(const RoundedDisplayHost&) = delete;
  RoundedDisplayHost& operator=(const RoundedDisplayHost&) = delete;

  ~RoundedDisplayHost() override;

 protected:
  // ash::FrameSinkHost:
  std::unique_ptr<viz::CompositorFrame> CreateCompositorFrame(
      const viz::BeginFrameAck& begin_frame_ack,
      UiResourceManager& resource_manager,
      bool auto_update,
      const gfx::Size& last_submitted_frame_size,
      float last_submitted_frame_dsf) override;

 private:
  // This callback is used to get RoundedDisplayGutters that know how to
  // paint mask textures for rounded corners of a display.
  GetGuttersCallback get_resource_generator_callback_;

  // RoundedDisplayHost delegates the work to create and configure compositor
  // frames to the RoundedDisplayFrameFactory.
  std::unique_ptr<RoundedDisplayFrameFactory> frame_factory_;

  base::WeakPtrFactory<RoundedDisplayHost> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_ROUNDED_DISPLAY_ROUNDED_DISPLAY_HOST_H_

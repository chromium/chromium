// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_SERVICE_GLIC_OFFSCREEN_RENDERING_CONTROLLER_H_
#define CHROME_BROWSER_GLIC_SERVICE_GLIC_OFFSCREEN_RENDERING_CONTROLLER_H_

#include "base/memory/raw_ref.h"

namespace content {
class WebContents;
}  // namespace content

namespace glic {

// Attaches the Glic web client WebContents to the Android offscreen rendering
// manager while the panel has no active embedder and either a hidden invoke is
// initializing the client or an actor task is actuating. This keeps the client
// running while it is not shown onscreen. Does nothing on other platforms.
class GlicOffscreenRenderingController {
 public:
  class Delegate {
   public:
    virtual ~Delegate() = default;

    // Returns true if the Glic panel is attached to an active embedder.
    virtual bool HasActiveEmbedder() const = 0;

    // Returns true while an actor task is actuating.
    virtual bool IsActuating() const = 0;

    // Wakes the host if it is hibernated.
    virtual void EnsureHostAwake() = 0;

    // Returns the WebContents to render offscreen, or null if there is none.
    virtual content::WebContents* GetOffscreenRenderingContents() const = 0;
  };

  explicit GlicOffscreenRenderingController(Delegate& delegate);
  GlicOffscreenRenderingController(const GlicOffscreenRenderingController&) =
      delete;
  GlicOffscreenRenderingController& operator=(
      const GlicOffscreenRenderingController&) = delete;
  ~GlicOffscreenRenderingController();

  bool is_active() const { return is_active_; }

  // Marks whether a hidden invoke is initializing the client.
  void SetHiddenInitializing(bool initializing);

  // Starts or stops offscreen rendering to match the current state.
  void UpdateState();

  // Stops offscreen rendering if it is active.
  void Stop();

 private:
  const raw_ref<Delegate> delegate_;
  bool is_hidden_initializing_ = false;
  bool is_active_ = false;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_SERVICE_GLIC_OFFSCREEN_RENDERING_CONTROLLER_H_

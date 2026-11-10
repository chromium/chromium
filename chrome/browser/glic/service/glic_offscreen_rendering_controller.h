// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_SERVICE_GLIC_OFFSCREEN_RENDERING_CONTROLLER_H_
#define CHROME_BROWSER_GLIC_SERVICE_GLIC_OFFSCREEN_RENDERING_CONTROLLER_H_

#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"

namespace content {
class WebContents;
}  // namespace content

namespace glic {

// Attaches the Glic web client WebContents to the Android offscreen rendering
// manager while the panel has no active embedder and either a hidden invoke is
// initializing the client or an actor task is actuating. This keeps the client
// running while it is not shown onscreen. Remembers the attached WebContents
// so that offscreen rendering moves to new contents when the host recreates
// them (e.g. on reload). Does nothing on other platforms.
class GlicOffscreenRenderingController {
 public:
  class Delegate {
   public:
    virtual ~Delegate() = default;

    // Returns true if the Glic panel is attached to an active embedder.
    virtual bool HasActiveEmbedder() const = 0;

    // Returns true while an actor task is actuating.
    virtual bool IsActuating() const = 0;

    // Returns true while an experimental triggering session is actively running
    // on this instance.
    virtual bool HasActiveExperimentalTriggeringSession() const = 0;

    // Returns the WebContents to render offscreen, or null if there is none.
    virtual content::WebContents* GetOffscreenRenderingContents() const = 0;
  };

  explicit GlicOffscreenRenderingController(Delegate& delegate);
  GlicOffscreenRenderingController(const GlicOffscreenRenderingController&) =
      delete;
  GlicOffscreenRenderingController& operator=(
      const GlicOffscreenRenderingController&) = delete;
  ~GlicOffscreenRenderingController();

  bool is_active() const { return !!attached_contents_; }

  // Marks whether a hidden invoke is initializing the client.
  void SetHiddenInitializing(bool initializing);

  // Starts or stops offscreen rendering to match the current state. Also
  // moves offscreen rendering to the delegate's current contents if they
  // changed, so call this when the host creates new contents.
  void UpdateState();

  // Stops offscreen rendering on the attached WebContents, if any.
  void Stop();

 private:
  const raw_ref<Delegate> delegate_;
  bool is_hidden_initializing_ = false;
  // The WebContents attached to the offscreen rendering manager. The host's
  // contents manager detaches it before destroying it, which also invalidates
  // this pointer.
  base::WeakPtr<content::WebContents> attached_contents_;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_SERVICE_GLIC_OFFSCREEN_RENDERING_CONTROLLER_H_

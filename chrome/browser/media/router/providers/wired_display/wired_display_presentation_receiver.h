// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_WIRED_DISPLAY_WIRED_DISPLAY_PRESENTATION_RECEIVER_H_
#define CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_WIRED_DISPLAY_WIRED_DISPLAY_PRESENTATION_RECEIVER_H_

#include <string>

#include "base/functional/callback.h"

class GURL;

namespace media_router {

// An interface for receivers used by WiredDisplayMediaRouteProvider for
// launching presentations on wired displays.
class WiredDisplayPresentationReceiver {
 public:
  WiredDisplayPresentationReceiver() = default;

  WiredDisplayPresentationReceiver(const WiredDisplayPresentationReceiver&) =
      delete;
  WiredDisplayPresentationReceiver& operator=(
      const WiredDisplayPresentationReceiver&) = delete;

  virtual ~WiredDisplayPresentationReceiver() = default;

  // Starts a presentation with the given ID and URL.
  virtual void Start(const std::string& presentation_id,
                     const GURL& start_url) = 0;

  // Terminates the presentation.
  virtual void Terminate() = 0;

  // Exits fullscreen and shows the receiver in windowed mode.
  virtual void ExitFullscreen() = 0;
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_WIRED_DISPLAY_WIRED_DISPLAY_PRESENTATION_RECEIVER_H_

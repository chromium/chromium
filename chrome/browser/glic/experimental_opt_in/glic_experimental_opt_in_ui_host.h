// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_EXPERIMENTAL_OPT_IN_GLIC_EXPERIMENTAL_OPT_IN_UI_HOST_H_
#define CHROME_BROWSER_GLIC_EXPERIMENTAL_OPT_IN_GLIC_EXPERIMENTAL_OPT_IN_UI_HOST_H_

#include <memory>

#include "build/build_config.h"

class GURL;
class Profile;

namespace base {
class TickClock;
}

namespace content {
class WebContents;
}

#if !BUILDFLAG(IS_ANDROID)
namespace views {
class Widget;
}
#endif

namespace glic {

#if !BUILDFLAG(IS_ANDROID)
class GlicExperimentalOptInDialogView;
#endif

// Host interface for displaying the Glic experimental opt-in UI across
// platforms (Views dialog vs Android container).
class GlicExperimentalOptInUIHost {
 public:
  class Delegate {
   public:
    virtual ~Delegate() = default;
    virtual void OnUIClosed(bool accepted) = 0;
  };

  // Factory method to create a platform-specific UI host implementation.
  static std::unique_ptr<GlicExperimentalOptInUIHost> Create(
      Profile* profile,
      Delegate* delegate);

  virtual ~GlicExperimentalOptInUIHost() = default;

  // Shows the opt-in UI over the given `web_contents`.
  virtual void Show(content::WebContents* web_contents) = 0;

  // Closes the opt-in UI.
  virtual void Close(bool accepted) = 0;

  // Opens a link in a new tab associated with the host's window/context.
  virtual void OpenLinkInNewTab(const GURL& url) {}

  // Gets, or creates, a web contents suitable for showing the experimental
  // opt-in dialog over. A web contents is considered "suitable" if it's
  // at the target URL.
  virtual content::WebContents* GetOrCreateSuitableWebContents() = 0;

  virtual void SetTickClockForTesting(const base::TickClock* tick_clock) {}

#if !BUILDFLAG(IS_ANDROID)
  // Views-desktop specific testing accessors.
  virtual views::Widget* GetWidgetForTesting() = 0;
  virtual GlicExperimentalOptInDialogView* GetDialogViewForTesting() = 0;
#endif
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_EXPERIMENTAL_OPT_IN_GLIC_EXPERIMENTAL_OPT_IN_UI_HOST_H_

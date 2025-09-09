// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_PUBLIC_GLIC_INSTANCE_COMPONENTS_H_
#define CHROME_BROWSER_GLIC_PUBLIC_GLIC_INSTANCE_COMPONENTS_H_

#include <memory>

#include "chrome/browser/glic/glic_zero_state_suggestions_manager.h"
#include "chrome/browser/glic/public/context/glic_sharing_manager.h"

class Profile;

namespace glic {

class GlicMetrics;
class GlicWindowController;
class GlicSharingManagerImpl;
class GlicScreenshotCapturer;
class Host;

class GlicInstanceComponents {
 public:
  explicit GlicInstanceComponents(
      Profile* profile,
      GlicWindowController& window_controller,
      Host& host,
      GlicMetrics& metrics,
      contextual_cueing::ContextualCueingService* contextual_cueing_service);
  GlicInstanceComponents(const GlicInstanceComponents&) = delete;
  GlicInstanceComponents& operator=(const GlicInstanceComponents&) = delete;
  ~GlicInstanceComponents();

  GlicSharingManager& sharing_manager();
  GlicZeroStateSuggestionsManager& zero_state_suggestions_manager() {
    return *zero_state_suggestions_manager_;
  }
  GlicScreenshotCapturer& screenshot_capturer() {
    return *screenshot_capturer_;
  }

 private:
  std::unique_ptr<GlicSharingManagerImpl> sharing_manager_;
  std::unique_ptr<GlicZeroStateSuggestionsManager>
      zero_state_suggestions_manager_;
  std::unique_ptr<GlicScreenshotCapturer> screenshot_capturer_;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_PUBLIC_GLIC_INSTANCE_COMPONENTS_H_

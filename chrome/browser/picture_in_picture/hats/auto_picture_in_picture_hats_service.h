// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PICTURE_IN_PICTURE_HATS_AUTO_PICTURE_IN_PICTURE_HATS_SERVICE_H_
#define CHROME_BROWSER_PICTURE_IN_PICTURE_HATS_AUTO_PICTURE_IN_PICTURE_HATS_SERVICE_H_

#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/picture_in_picture/auto_pip_setting_helper.h"
#include "components/keyed_service/core/keyed_service.h"
#include "media/base/picture_in_picture_events_info.h"
#include "url/gurl.h"

namespace base {
class TickClock;
}  // namespace base

class Profile;

// Service responsible for managing Happiness Tracking Surveys (HaTS) for
// Automatic Picture-in-Picture.
class AutoPictureInPictureHatsService : public KeyedService {
 public:
  // Represents an active Auto Picture-in-Picture window context. A context is
  // considered "active" from the moment an Auto Picture-in-Picture window is
  // successfully opened via `AutoPictureInPictureWindowOpened()` until it is
  // closed via `AutoPictureInPictureWindowClosed()`.
  //
  // This struct stores the metadata required to decide which HaTS survey to
  // trigger (if any) and to calculate the Product-Specific Data (PSD) sent with
  // the survey.
  struct ActiveWindowContext {
    ActiveWindowContext(media::PictureInPictureEventsInfo::AutoPipReason reason,
                        const GURL& origin,
                        base::TimeTicks start_time);

    media::PictureInPictureEventsInfo::AutoPipReason reason;
    GURL origin;
    std::optional<AutoPipSettingHelper::PromptResult> prompt_result;
    base::TimeTicks start_time;
  };

  explicit AutoPictureInPictureHatsService(Profile* profile);
  AutoPictureInPictureHatsService(const AutoPictureInPictureHatsService&) =
      delete;
  AutoPictureInPictureHatsService& operator=(
      const AutoPictureInPictureHatsService&) = delete;
  ~AutoPictureInPictureHatsService() override;

  // Notifies the service that an Auto Picture-in-Picture window has been
  // opened. This allows the service to begin tracking session duration and
  // metadata for a potential HaTS survey.
  void AutoPictureInPictureWindowOpened(
      media::PictureInPictureEventsInfo::AutoPipReason reason,
      const GURL& origin);

  // Sets the result of the permission prompt.
  void SetPromptResult(AutoPipSettingHelper::PromptResult result);

  // Notifies the service that the Auto Picture-in-Picture window has been
  // closed. This allows the service to determine if a HaTS survey should be
  // shown based on the window session.
  void AutoPictureInPictureWindowClosed();

  const std::optional<ActiveWindowContext>&
  get_active_window_context_for_testing() const {
    return active_window_context_;
  }

  void set_clock_for_testing(const base::TickClock* clock) { clock_ = clock; }

 private:
  raw_ptr<Profile> profile_;

  raw_ptr<const base::TickClock> clock_;

  // State for the current active Auto Picture-in-Picture window, if any.
  std::optional<ActiveWindowContext> active_window_context_;
};

#endif  // CHROME_BROWSER_PICTURE_IN_PICTURE_HATS_AUTO_PICTURE_IN_PICTURE_HATS_SERVICE_H_

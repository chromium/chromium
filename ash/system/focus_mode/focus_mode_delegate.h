// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_FOCUS_MODE_FOCUS_MODE_DELEGATE_H_
#define ASH_SYSTEM_FOCUS_MODE_FOCUS_MODE_DELEGATE_H_

#include <memory>

#include "ash/ash_export.h"
#include "components/account_id/account_id.h"

namespace ash {

namespace youtube_music {
class YouTubeMusicClient;
}  // namespace youtube_music

// Interface for focus mode delegate.
// TODO(yongshun): Move this and the YouTube Music client interface to
// ash/public/cpp if they are going to be implemented in chrome.
class ASH_EXPORT FocusModeDelegate {
 public:
  virtual ~FocusModeDelegate() = default;

  // Virtual function that is implemented in chrome to create the client.
  virtual std::unique_ptr<youtube_music::YouTubeMusicClient>
  CreateYouTubeMusicClient(const AccountId& account_id,
                           const std::string& device_id) = 0;

  // Returns the application locale from the browser process.
  virtual const std::string& GetLocale() = 0;

  // True if the active user is considered a minor (e.g. under the age of 18)
  // using the manta service account capability as the signal.
  virtual bool IsMinorUser() = 0;
};

}  // namespace ash

#endif  // ASH_SYSTEM_FOCUS_MODE_FOCUS_MODE_DELEGATE_H_

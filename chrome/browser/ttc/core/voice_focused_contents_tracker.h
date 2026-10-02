// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TTC_CORE_VOICE_FOCUSED_CONTENTS_TRACKER_H_
#define CHROME_BROWSER_TTC_CORE_VOICE_FOCUSED_CONTENTS_TRACKER_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"

class Profile;

namespace content {
class WebContents;
}  // namespace content

namespace ttc {

// Tracks the active tab's contents across `profile`'s browser windows, and
// notifies observers whenever it changes.
//
// The tracker binds to `profile`'s currently active browser window (a TabModel
// on Android):
//  - Switching tabs within the bound window changes the active tab.
//  - Activating a different browser window of `profile` binds to that window
//    and tracks its active tab.
//  - Closing all browser windows of `profile` clears the active tab.
//
// Desktop and Android implementations live in
// VoiceFocusedContentsTrackerDesktop and VoiceFocusedContentsTrackerAndroid
// respectively; use Create() to get the one for the current platform.
class VoiceFocusedContentsTracker {
 public:
  class Observer : public base::CheckedObserver {
   public:
    // Invoked asynchronously after construction with the initial active tab's
    // contents, and whenever the tracked active tab subsequently changes.
    // `web_contents` is the active tab's contents, or null if there is no
    // active tab (e.g. all browser windows of the profile closed).
    virtual void OnVoiceFocusedContentsChanged(
        content::WebContents* web_contents) {}
  };

  // Returns the implementation for the current platform. `profile` must
  // outlive the returned object. Defined by each platform's implementation.
  static std::unique_ptr<VoiceFocusedContentsTracker> Create(Profile& profile);

  VoiceFocusedContentsTracker(const VoiceFocusedContentsTracker&) = delete;
  VoiceFocusedContentsTracker& operator=(const VoiceFocusedContentsTracker&) =
      delete;
  virtual ~VoiceFocusedContentsTracker();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Returns the contents of the active tab across the profile's browser
  // windows, or null if there is no active tab.
  virtual content::WebContents* GetActiveWebContents() const = 0;

 protected:
  VoiceFocusedContentsTracker();

  // Notifies observers that the result of GetActiveWebContents() has changed.
  void NotifyActiveTabChanged();

 private:
  base::ObserverList<Observer> observers_;

  base::WeakPtrFactory<VoiceFocusedContentsTracker> weak_ptr_factory_{this};
};

}  // namespace ttc

#endif  // CHROME_BROWSER_TTC_CORE_VOICE_FOCUSED_CONTENTS_TRACKER_H_

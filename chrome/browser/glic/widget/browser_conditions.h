// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_WIDGET_BROWSER_CONDITIONS_H_
#define CHROME_BROWSER_GLIC_WIDGET_BROWSER_CONDITIONS_H_

#include <memory>

class Browser;
class Profile;

namespace glic {

// Returns the browser the glic window would attach to, if the user pressed
// the attach button on the window. Returns null if attachment is not possible.
Browser* FindBrowserForAttachment(Profile* profile);

// Returns whether `browser` can be used for attaching the glic panel for the
// given profile.
bool IsBrowserGlicAttachable(Profile* profile, Browser* browser);

// Returns whether 'browser' is in the foreground. This is based on active
// state and on windows includes a occlusion check.
bool IsBrowserInForeground(Browser* browser);

// Observes changes to what value FindBrowserForAttachment() would return.
class BrowserAttachObserver {
 public:
  // Informs the observer that the browser for attachment has changed. Null if
  // no browser is available for attachment.
  virtual void BrowserForAttachmentChanged(Browser* browser) {}
  // Informs the observer that the value of `CanAttachToBrowser()` has changed.
  virtual void CanAttachToBrowserChanged(bool can_attach) {}
};

class BrowserAttachObservation {
 protected:
  BrowserAttachObservation() = default;

 public:
  BrowserAttachObservation(const BrowserAttachObservation&) = delete;
  BrowserAttachObservation& operator=(const BrowserAttachObservation&) = delete;
  virtual ~BrowserAttachObservation() = default;

  // Whether a browser is available for attachment right now.
  virtual bool CanAttachToBrowser() const = 0;
};

// Observes BrowserAttachObserver events until the returned observation is
// destroyed.
std::unique_ptr<BrowserAttachObservation> ObserveBrowserForAttachment(
    Profile* profile,
    BrowserAttachObserver* observer);

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_WIDGET_BROWSER_CONDITIONS_H_

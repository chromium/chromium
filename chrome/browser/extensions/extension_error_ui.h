// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_ERROR_UI_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_ERROR_UI_H_

namespace content {
class BrowserContext;
}

namespace extensions {

class ExtensionSet;

// This class encapsulates the UI we want to show users when certain events
// occur related to installed extensions.
class ExtensionErrorUI {
 public:
  class Delegate {
   public:
    // Get the BrowserContext associated with this UI.
    virtual content::BrowserContext* GetContext() = 0;

    // Get the set of blocklisted extensions to warn the user about.
    virtual const ExtensionSet& GetBlocklistedExtensions() = 0;

    // Handle the user clicking to get more details on the extension alert.
    virtual void OnAlertDetails() = 0;

    // Handle the user clicking "accept" on the extension alert.
    virtual void OnAlertAccept() = 0;

    // Handle the alert closing.
    virtual void OnAlertClosed() = 0;
  };

  // Shows the installation error in a bubble view. Should return true if a
  // bubble is shown, false if one could not be shown.
  virtual bool ShowErrorInBubbleView() = 0;

  // Shows the extension page. Called as a result of the user clicking more
  // info and should be only called from the context of a callback
  // (BubbleViewDidClose or BubbleViewAccept/CancelButtonPressed).
  // It should use the same browser as where the bubble was shown.
  virtual void ShowExtensions() = 0;

  // Closes the error UI. This will end up calling BubbleViewDidClose, possibly
  // synchronously.
  virtual void Close() = 0;

  virtual ~ExtensionErrorUI() {}
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_ERROR_UI_H_

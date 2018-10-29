// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_ACTION_ICON_FACTORY_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_ACTION_ICON_FACTORY_H_

#include <memory>

#include "base/macros.h"
#include "base/scoped_observer.h"
#include "extensions/browser/extension_icon_image.h"

class ExtensionAction;
class Profile;

namespace extensions {
class Extension;
}

// Used to get an icon to be used in the UI for an extension action.
// If the extension action icon is the default icon defined in the extension's
// manifest, it is loaded using extensions::IconImage. This icon can be loaded
// asynchronously. The factory observes underlying IconImage and notifies its
// own observer when the icon image changes.
class ExtensionActionIconFactory : public extensions::IconImage::Observer {
 public:
  class Observer {
   public:
    virtual ~Observer() {}
    // Called when the underlying icon image changes.
    virtual void OnIconUpdated() = 0;
  };

  // Observer should outlive this.
  ExtensionActionIconFactory(Profile* profile,
                             const extensions::Extension* extension,
                             ExtensionAction* action,
                             Observer* observer);
  ~ExtensionActionIconFactory() override;

  // Controls whether invisible icons will be returned by GetIcon().
  static void SetAllowInvisibleIconsForTest(bool value);

  // extensions::IconImage override.
  void OnExtensionIconImageChanged(extensions::IconImage* image) override;
  void OnExtensionIconImageDestroyed(extensions::IconImage* image) override;

  // Gets the extension action icon for the tab.
  // If there is an icon set using |SetIcon|, that icon is returned.
  // Else, if there is a default icon set for the extension action, the icon is
  // created using IconImage. Observer is triggered wheniever the icon gets
  // updated.
  // Else, the extension's placeholder icon is returned.
  // In all cases, action's attention and animation icon transformations are
  // applied on the icon.
  gfx::Image GetIcon(int tab_id);

 private:
  const ExtensionAction* action_;
  Observer* observer_;
  const bool should_check_icons_;
  gfx::Image cached_default_icon_image_;

  ScopedObserver<extensions::IconImage, extensions::IconImage::Observer>
      icon_image_observer_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionActionIconFactory);
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_ACTION_ICON_FACTORY_H_

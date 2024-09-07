// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_DESKTOP_ANDROID_DESKTOP_ANDROID_EXTENSION_WEB_CONTENTS_OBSERVER_H_
#define CHROME_BROWSER_EXTENSIONS_DESKTOP_ANDROID_DESKTOP_ANDROID_EXTENSION_WEB_CONTENTS_OBSERVER_H_

#include "content/public/browser/web_contents_user_data.h"
#include "extensions/browser/extension_web_contents_observer.h"

namespace extensions {

////////////////////////////////////////////////////////////////////////////////
// S  T  O  P
// ALL THIS CODE WILL BE DELETED.
// THINK TWICE (OR THRICE) BEFORE ADDING MORE.
//
// The details:
// This is part of an experimental desktop-android build and allows us to
// bootstrap the extension system by incorporating a lightweight extensions
// runtime into the chrome binary. This allows us to do things like load
// extensions in tests and exercise code in these builds without needing to have
// the entirety of the //chrome/browser/extensions system either compiled and
// implemented (which is a massive undertaking) or gracefully if-def'd out
// (which is a massive amount of technical debt).
// This approach, by comparison, allows us to have a minimal interface in the
// chrome browser that mostly relies on the top-level //extensions layer, along
// with small bits of the //chrome code that compile cleanly on the
// experimental desktop-android build.
//
// This entire class should go away. Instead of adding new functionality here,
// it should be added in a location that can be shared across desktop-android
// and other desktop builds. In practice, this means:
// * Pulling the code up to //extensions. If it can be cleanly segmented from
//   the //chrome layer, this is preferable. It gets cleanly included across
//   all builds, encourages proper separation of concerns, and reduces the
//   interdependency between features.
// * Including the functionality in the desktop-android build. This can be done
//   for //chrome sources that do not have any dependencies on areas that
//   cannot be included in desktop-android (such as dependencies on `Browser`
//   or native UI code).
//
// TODO(https://crbug.com/356905053): Delete this class once desktop-android
// properly leverages the extension system.
////////////////////////////////////////////////////////////////////////////////
class DesktopAndroidExtensionWebContentsObserver
    : public ExtensionWebContentsObserver,
      public content::WebContentsUserData<
          DesktopAndroidExtensionWebContentsObserver> {
 public:
  DesktopAndroidExtensionWebContentsObserver(
      const DesktopAndroidExtensionWebContentsObserver&) = delete;
  DesktopAndroidExtensionWebContentsObserver& operator=(
      const DesktopAndroidExtensionWebContentsObserver&) = delete;

  ~DesktopAndroidExtensionWebContentsObserver() override;

  // Creates and initializes an instance of this class for the given
  // `web_contents`, if it doesn't already exist.
  static void CreateForWebContents(content::WebContents* web_contents);

 private:
  friend class content::WebContentsUserData<
      DesktopAndroidExtensionWebContentsObserver>;

  explicit DesktopAndroidExtensionWebContentsObserver(
      content::WebContents* web_contents);

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_DESKTOP_ANDROID_DESKTOP_ANDROID_EXTENSION_WEB_CONTENTS_OBSERVER_H_

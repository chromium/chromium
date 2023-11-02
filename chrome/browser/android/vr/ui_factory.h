// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_VR_UI_FACTORY_H_
#define CHROME_BROWSER_ANDROID_VR_UI_FACTORY_H_

#include <memory>

namespace vr {

class AudioDelegate;
class KeyboardDelegate;
class PlatformInputHandler;
class TextInputDelegate;
class UiBrowserInterface;
class UiInterface;
struct UiInitialState;

// This class defines the interface to a UiFactory.  It is implemented by at
// least two variants - a simple factory that does nothing but construct a UI,
// and an app bundle-specific factory that opens a shared library, fishes for a
// factory method, and creates a UI from it.
//
// Either one variant or the other is compiled in.  Alternatively, there could
// be a single factory instance, which switches behavior based on the results of
// an IsBundle() method.  A downside of that is that we start to include
// bundle-specific code in non-bundle builds, and vice-versa.
class UiFactory {
 public:
  UiFactory() {}

  UiFactory(const UiFactory&) = delete;
  UiFactory& operator=(const UiFactory&) = delete;

  virtual ~UiFactory() {}

  virtual std::unique_ptr<UiInterface> Create(
      UiBrowserInterface* browser,
      PlatformInputHandler* content_input_forwarder,
      std::unique_ptr<KeyboardDelegate> keyboard_delegate,
      std::unique_ptr<TextInputDelegate> text_input_delegate,
      std::unique_ptr<AudioDelegate> audio_delegate,
      const UiInitialState& ui_initial_state) = 0;
};

// Creates a UI factory appropriate for the current build.  Bundle builds will
// include a factory that opens the feature module library and uses it to
// instantiate a UI.
std::unique_ptr<UiFactory> CreateUiFactory();

}  // namespace vr

#endif  // CHROME_BROWSER_ANDROID_VR_UI_FACTORY_H_

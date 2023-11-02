// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_BRAILLE_DISPLAY_PRIVATE_STUB_BRAILLE_CONTROLLER_H_
#define CHROME_BROWSER_EXTENSIONS_API_BRAILLE_DISPLAY_PRIVATE_STUB_BRAILLE_CONTROLLER_H_

#include "base/memory/singleton.h"
#include "chrome/browser/extensions/api/braille_display_private/braille_controller.h"

namespace extensions {
namespace api {
namespace braille_display_private {

// Stub implementation for the BrailleController interface.
class StubBrailleController : public BrailleController {
 public:
  StubBrailleController();
  std::unique_ptr<DisplayState> GetDisplayState() override;
  void WriteDots(const std::vector<uint8_t>& cells,
                 unsigned int cols,
                 unsigned int rows) override;
  void AddObserver(BrailleObserver* observer) override;
  void RemoveObserver(BrailleObserver* observer) override;

  static StubBrailleController* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<StubBrailleController>;
};

}  // namespace braille_display_private
}  // namespace api
}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_BRAILLE_DISPLAY_PRIVATE_STUB_BRAILLE_CONTROLLER_H_

// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_BRAILLE_DISPLAY_PRIVATE_MOCK_BRAILLE_CONTROLLER_H_
#define CHROME_BROWSER_EXTENSIONS_API_BRAILLE_DISPLAY_PRIVATE_MOCK_BRAILLE_CONTROLLER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/extensions/api/braille_display_private/stub_braille_controller.h"

namespace extensions {
namespace api {
namespace braille_display_private {

// Mock implementation of the BrailleController interface.
class MockBrailleController : public StubBrailleController {
 public:
  MockBrailleController();

  std::unique_ptr<DisplayState> GetDisplayState() override;
  void AddObserver(BrailleObserver* observer) override;
  void RemoveObserver(BrailleObserver* observer) override;

  // Sets the available flag without calling the observer.
  void SetAvailable(bool available);
  BrailleObserver* GetObserver() const;

 private:
  bool available_;
  raw_ptr<BrailleObserver> observer_;
};

}  // namespace braille_display_private
}  // namespace api
}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_BRAILLE_DISPLAY_PRIVATE_MOCK_BRAILLE_CONTROLLER_H_

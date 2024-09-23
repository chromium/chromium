// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SCREEN_AI_PUBLIC_TEST_FAKE_OPTICAL_CHARACTER_RECOGNIZER_H_
#define CHROME_BROWSER_SCREEN_AI_PUBLIC_TEST_FAKE_OPTICAL_CHARACTER_RECOGNIZER_H_

#include "base/memory/scoped_refptr.h"
#include "chrome/browser/screen_ai/public/optical_character_recognizer.h"

namespace screen_ai {

class FakeOpticalCharacterRecognizer : public OpticalCharacterRecognizer {
 public:
  static scoped_refptr<screen_ai::OpticalCharacterRecognizer> Create(
      bool return_empty);

  void PerformOCR(
      const SkBitmap& image,
      base::OnceCallback<void(mojom::VisualAnnotationPtr)> callback) override;

  void PerformOCR(const SkBitmap& image,
                  base::OnceCallback<void(const ui::AXTreeUpdate& tree_update)>
                      callback) override;

  void FlushForTesting() override;

 private:
  template <typename T, typename... Args>
  friend scoped_refptr<T> base::MakeRefCounted(Args&&... args);

  explicit FakeOpticalCharacterRecognizer(bool return_empty);
  ~FakeOpticalCharacterRecognizer() override;

  // A negative ID for ui::AXNodeID needs to start from -2 as using -1 for this
  // node id is still incorrectly treated as invalid.
  ui::AXNodeID next_node_id_ = -2;

  bool return_empty_;
};

}  // namespace screen_ai

#endif  // CHROME_BROWSER_SCREEN_AI_PUBLIC_TEST_FAKE_OPTICAL_CHARACTER_RECOGNIZER_H_

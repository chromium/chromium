// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SCREEN_AI_PUBLIC_TEST_FAKE_OPTICAL_CHARACTER_RECOGNIZER_H_
#define CHROME_BROWSER_SCREEN_AI_PUBLIC_TEST_FAKE_OPTICAL_CHARACTER_RECOGNIZER_H_

#include <utility>

#include "base/memory/scoped_refptr.h"
#include "chrome/browser/screen_ai/public/optical_character_recognizer.h"
#include "services/screen_ai/public/mojom/screen_ai_service.mojom.h"

namespace screen_ai {

class FakeOpticalCharacterRecognizer : public OpticalCharacterRecognizer {
 public:
  static scoped_refptr<screen_ai::FakeOpticalCharacterRecognizer> Create(
      bool empty_ax_tree_update_result);

  void PerformOCR(
      const SkBitmap& image,
      base::OnceCallback<void(mojom::VisualAnnotationPtr)> callback) override;

  void PerformOCR(const SkBitmap& image,
                  base::OnceCallback<void(const ui::AXTreeUpdate& tree_update)>
                      callback) override;

  void FlushForTesting() override;

  void set_visual_annotation_result(
      mojom::VisualAnnotationPtr visual_annotation_result) {
    visual_annotation_result_ = std::move(visual_annotation_result);
  }

 private:
  template <typename T, typename... Args>
  friend scoped_refptr<T> base::MakeRefCounted(Args&&... args);

  explicit FakeOpticalCharacterRecognizer(bool return_empty);
  ~FakeOpticalCharacterRecognizer() override;

  // A negative ID for ui::AXNodeID needs to start from -2 as using -1 for this
  // node id is still incorrectly treated as invalid.
  ui::AXNodeID next_node_id_ = -2;

  // True if the AX tree update result for `PerformOCR` should be empty.
  bool empty_ax_tree_update_result_;

  // Used to set a fake visual annotation result for `PerformOCR`.
  mojom::VisualAnnotationPtr visual_annotation_result_;
};

}  // namespace screen_ai

#endif  // CHROME_BROWSER_SCREEN_AI_PUBLIC_TEST_FAKE_OPTICAL_CHARACTER_RECOGNIZER_H_

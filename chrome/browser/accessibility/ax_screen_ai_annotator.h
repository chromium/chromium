// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACCESSIBILITY_AX_SCREEN_AI_ANNOTATOR_H_
#define CHROME_BROWSER_ACCESSIBILITY_AX_SCREEN_AI_ANNOTATOR_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/services/screen_ai/public/mojom/screen_ai_service.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/accessibility/ax_tree_id.h"

class Browser;

namespace gfx {
class Image;
}

namespace screen_ai {

class AXScreenAIAnnotator {
 public:
  explicit AXScreenAIAnnotator(Browser* browser);
  virtual ~AXScreenAIAnnotator();
  AXScreenAIAnnotator(const AXScreenAIAnnotator&) = delete;
  AXScreenAIAnnotator& operator=(const AXScreenAIAnnotator&) = delete;

  // Binds |screen_ai_annotator_| to the Screen AI service.
  virtual void BindToScreenAIService();

  // Takes a screenshot and sends it to |OnScreenshotReceived| through an async
  // call.
  void Run();

 private:
  // Receives an screenshot and sends it to ScreenAI library for processing.
  // |ax_tree_id| represents the accessibility tree that is associated with the
  // snapshot at the time of triggering the request.
  virtual void OnScreenshotReceived(const ui::AXTreeID& ax_tree_id,
                                    gfx::Image snapshot);

  // Receives the annotations from ScreenAI service. |ax_tree_id| is the id of
  // the accessibility tree associated with the snapshot that was sent to
  // ScreenAI library.
  void OnAnnotationReceived(const ui::AXTreeID& ax_tree_id,
                            const ui::AXTreeUpdate& updates);

  // Owns us.
  raw_ptr<Browser> const browser_;

  mojo::Remote<screen_ai::mojom::ScreenAIAnnotator> screen_ai_annotator_;

  base::WeakPtrFactory<AXScreenAIAnnotator> weak_ptr_factory_{this};
};

}  // namespace screen_ai

#endif  // CHROME_BROWSER_ACCESSIBILITY_AX_SCREEN_AI_ANNOTATOR_H_

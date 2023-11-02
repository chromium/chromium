// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACCESSIBILITY_AX_SCREEN_AI_ANNOTATOR_H_
#define CHROME_BROWSER_ACCESSIBILITY_AX_SCREEN_AI_ANNOTATOR_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/services/screen_ai/public/cpp/screen_ai_install_state.h"
#include "components/services/screen_ai/public/mojom/screen_ai_service.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/accessibility/ax_tree_id.h"

class Browser;

namespace content {
class BrowserContext;
}

namespace gfx {
class Image;
}

namespace screen_ai {

class AXScreenAIAnnotator : public KeyedService,
                            mojom::ScreenAIAnnotatorClient,
                            ScreenAIInstallState::Observer {
 public:
  explicit AXScreenAIAnnotator(content::BrowserContext* browser_context);
  AXScreenAIAnnotator(const AXScreenAIAnnotator&) = delete;
  AXScreenAIAnnotator& operator=(const AXScreenAIAnnotator&) = delete;
  ~AXScreenAIAnnotator() override;

  // Takes a screenshot and sends it to `OnScreenshotReceived` through an async
  // call.
  void AnnotateScreenshot(Browser* browser);

  // TODO(https://crbug.com/1278249): Add
  // mojom::ScreenAIServiceClient::HandleAXTreeUpdate after service side data is
  // ready.

  // ScreenAIInstallState::Observer:
  void ComponentReady() override;

 private:
  // Binds `screen_ai_annotator_` to the Screen AI service.
  virtual void BindToScreenAIService(content::BrowserContext* browser_context);

  // Receives an screenshot and sends it to ScreenAI library for processing.
  // `ax_tree_id` represents the accessibility tree that is associated with the
  // snapshot at the time of triggering the request.
  virtual void OnScreenshotReceived(const ui::AXTreeID& ax_tree_id,
                                    gfx::Image snapshot);

  // Informs this instance that the Screen AI Service has finished creating the
  // visual annotations. `parent_tree_id` is the ID of the accessibility tree
  // associated with the screenshot that was sent to the Screen AI Service, and
  // `screen_ai_tree_id` is the ID of the accessibility tree that has been
  // created by the Service, containing the visual annotations.
  void OnAnnotationPerformed(const ui::AXTreeID& parent_tree_id,
                             const ui::AXTreeID& screen_ai_tree_id);

  base::ScopedObservation<ScreenAIInstallState, ScreenAIInstallState::Observer>
      component_ready_observer_{this};

  // AXScreenAIAnnotator is created by a factory on this browser context and
  // will be destroyed before browser context gets destroyed.
  content::BrowserContext* browser_context_;

  mojo::Remote<mojom::ScreenAIAnnotator> screen_ai_annotator_;
  mojo::Receiver<mojom::ScreenAIAnnotatorClient> screen_ai_service_client_;

  base::WeakPtrFactory<AXScreenAIAnnotator> weak_ptr_factory_{this};
};

}  // namespace screen_ai

#endif  // CHROME_BROWSER_ACCESSIBILITY_AX_SCREEN_AI_ANNOTATOR_H_

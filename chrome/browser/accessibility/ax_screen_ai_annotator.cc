// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility/ax_screen_ai_annotator.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "components/services/screen_ai/public/cpp/screen_ai_service_router.h"
#include "components/services/screen_ai/public/cpp/screen_ai_service_router_factory.h"
#include "content/public/browser/web_contents.h"
#include "ui/accessibility/ax_tree_manager_map.h"
#include "ui/gfx/image/image.h"
#include "ui/snapshot/snapshot.h"

namespace screen_ai {

AXScreenAIAnnotator::AXScreenAIAnnotator(Browser* browser) : browser_(browser) {
  mojo::PendingReceiver<screen_ai::mojom::ScreenAIAnnotator>
      screen_ai_receiver = screen_ai_annotator_.BindNewPipeAndPassReceiver();
  ScreenAIServiceRouterFactory::GetForBrowserContext(
      static_cast<content::BrowserContext*>(browser->profile()))
      ->BindScreenAIAnnotator(std::move(screen_ai_receiver));
}

AXScreenAIAnnotator::~AXScreenAIAnnotator() = default;

void AXScreenAIAnnotator::Run() {
  // Request screenshot from content area of the main frame.
  content::WebContents* web_contents =
      browser_->tab_strip_model()->GetActiveWebContents();
  if (!web_contents)
    return;
  gfx::NativeView native_view = web_contents->GetContentNativeView();
  if (!native_view)
    return;

  ui::GrabViewSnapshotAsync(
      native_view, gfx::Rect(web_contents->GetSize()),
      base::BindOnce(&AXScreenAIAnnotator::OnScreenshotReceived,
                     weak_ptr_factory_.GetWeakPtr(),
                     web_contents->GetPrimaryMainFrame()->GetAXTreeID()));
}

void AXScreenAIAnnotator::OnScreenshotReceived(const ui::AXTreeID& ax_tree_id,
                                               gfx::Image snapshot) {
  screen_ai_annotator_->Annotate(
      snapshot.AsBitmap(),
      base::BindOnce(&AXScreenAIAnnotator::OnAnnotationReceived,
                     weak_ptr_factory_.GetWeakPtr(), ax_tree_id));
}

void AXScreenAIAnnotator::OnAnnotationReceived(
    const ui::AXTreeID& ax_tree_id,
    const ui::AXTreeUpdate& updates) {
  VLOG(2) << "AxScreenAIAnnotator received:\n" << updates.ToString();

  ui::AXTreeManager* manager =
      ui::AXTreeManagerMap::GetInstance().GetManager(ax_tree_id);

  if (!manager) {
    VLOG(1) << "ScreenAI annotations received, but the corresponding AxTree "
               "does not exist anymore.";
    return;
  }

  // TODO(https://crbug.com/1278249): Use |updates|.
}

}  // namespace screen_ai

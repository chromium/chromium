// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility/ax_screen_ai_annotator.h"

#include "base/strings/strcat.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "components/services/screen_ai/public/cpp/screen_ai_service_router.h"
#include "components/services/screen_ai/public/cpp/screen_ai_service_router_factory.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "ui/accessibility/ax_tree_manager.h"
#include "ui/gfx/image/image.h"
#include "ui/snapshot/snapshot.h"

namespace screen_ai {

AXScreenAIAnnotator::AXScreenAIAnnotator(
    content::BrowserContext* browser_context)
    : browser_context_(browser_context), screen_ai_service_client_(this) {
  component_ready_observer_.Observe(ScreenAIInstallState::GetInstance());
}

AXScreenAIAnnotator::~AXScreenAIAnnotator() = default;

void AXScreenAIAnnotator::ComponentReady() {
  DCHECK(!screen_ai_service_client_.is_bound());
  BindToScreenAIService(browser_context_);
}

void AXScreenAIAnnotator::BindToScreenAIService(
    content::BrowserContext* browser_context) {
  mojo::PendingReceiver<mojom::ScreenAIAnnotator> screen_ai_receiver =
      screen_ai_annotator_.BindNewPipeAndPassReceiver();

  ScreenAIServiceRouter* service_router =
      ScreenAIServiceRouterFactory::GetForBrowserContext(browser_context);

  service_router->BindScreenAIAnnotator(std::move(screen_ai_receiver));
  service_router->BindScreenAIAnnotatorClient(
      screen_ai_service_client_.BindNewPipeAndPassRemote());
}

void AXScreenAIAnnotator::AnnotateScreenshot(Browser* browser) {
  // Request screenshot from content area of the main frame.
  content::WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  if (!web_contents)
    return;
  gfx::NativeView native_view = web_contents->GetContentNativeView();
  if (!native_view)
    return;

// TODO(https://crbug.com/1278249): Add UMA for screenshot timing to ensure
// the sync method is not blocking the browser process.
#if BUILDFLAG(IS_MAC)
  gfx::Image snapshot;
  if (!ui::GrabViewSnapshot(native_view, gfx::Rect(web_contents->GetSize()),
                            &snapshot)) {
    VLOG(1) << "AxScreenAIAnnotator could not grab snapshot.";
    return;
  }

  OnScreenshotReceived(web_contents->GetPrimaryMainFrame()->GetAXTreeID(),
                       std::move(snapshot));
#else
  ui::GrabViewSnapshotAsync(
      native_view, gfx::Rect(web_contents->GetSize()),
      base::BindOnce(&AXScreenAIAnnotator::OnScreenshotReceived,
                     weak_ptr_factory_.GetWeakPtr(),
                     web_contents->GetPrimaryMainFrame()->GetAXTreeID()));
#endif
}

void AXScreenAIAnnotator::OnScreenshotReceived(const ui::AXTreeID& ax_tree_id,
                                               gfx::Image snapshot) {
  DCHECK(screen_ai_annotator_.is_bound());
  screen_ai_annotator_->Annotate(
      snapshot.AsBitmap(),
      base::BindOnce(&AXScreenAIAnnotator::OnAnnotationPerformed,
                     weak_ptr_factory_.GetWeakPtr(), ax_tree_id));
}

void AXScreenAIAnnotator::OnAnnotationPerformed(
    const ui::AXTreeID& parent_tree_id,
    const ui::AXTreeID& screen_ai_tree_id) {
  VLOG(2) << base::StrCat({"AXScreenAIAnnotator received tree ids: parent: ",
                           parent_tree_id.ToString().c_str(), ", ScreenAI: ",
                           screen_ai_tree_id.ToString().c_str()});
  // TODO(https://crbug.com/1278249): Use!
  NOTIMPLEMENTED();
}

}  // namespace screen_ai

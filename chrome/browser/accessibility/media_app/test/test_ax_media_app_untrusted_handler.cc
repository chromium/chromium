// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility/media_app/test/test_ax_media_app_untrusted_handler.h"

#include <utility>

#include "content/public/browser/browser_context.h"
#include "ui/accessibility/ax_tree.h"
#include "ui/accessibility/ax_tree_id.h"
#include "ui/accessibility/ax_tree_update.h"

namespace ash::test {

TestAXMediaAppUntrustedHandler::TestAXMediaAppUntrustedHandler(
    content::BrowserContext& context,
    mojo::PendingRemote<media_app_ui::mojom::OcrUntrustedPage> page)
    : AXMediaAppUntrustedHandler(context, std::move(page)) {}

TestAXMediaAppUntrustedHandler::~TestAXMediaAppUntrustedHandler() = default;

std::string TestAXMediaAppUntrustedHandler::GetDocumentTreeToStringForTesting()
    const {
  if (!document_.ax_tree()) {
    return {};
  }
  return document_.ax_tree()->ToString();
}

void TestAXMediaAppUntrustedHandler::
    EnablePendingSerializedUpdatesForTesting() {
  pending_serialized_updates_for_testing_ =
      std::make_unique<std::vector<const ui::AXTreeUpdate>>();
}

#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
void TestAXMediaAppUntrustedHandler::SetScreenAIAnnotatorForTesting(
    mojo::PendingRemote<screen_ai::mojom::ScreenAIAnnotator>
        screen_ai_annotator) {
  screen_ai_annotator_.reset();
  screen_ai_annotator_.Bind(std::move(screen_ai_annotator));
}

void TestAXMediaAppUntrustedHandler::FlushForTesting() {
  screen_ai_annotator_.FlushForTesting();  // IN-TEST
}
#endif  // BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)

bool TestAXMediaAppUntrustedHandler::IsOcrServiceEnabled() const {
  return is_ocr_service_enabled_for_testing_ ||
         AXMediaAppUntrustedHandler::IsOcrServiceEnabled();
}

void TestAXMediaAppUntrustedHandler::PushDirtyPageForTesting(
    const std::string& dirty_page_id) {
  AXMediaAppUntrustedHandler::PushDirtyPage(dirty_page_id);
}

std::string TestAXMediaAppUntrustedHandler::PopDirtyPageForTesting() {
  return AXMediaAppUntrustedHandler::PopDirtyPage();
}

void TestAXMediaAppUntrustedHandler::OcrNextDirtyPageIfAny() {
  if (delay_calling_ocr_next_dirty_page_) {
    return;
  }
  AXMediaAppUntrustedHandler::OcrNextDirtyPageIfAny();
}

}  // namespace ash::test

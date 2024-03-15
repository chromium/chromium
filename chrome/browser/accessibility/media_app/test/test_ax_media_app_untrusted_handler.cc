// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility/media_app/test/test_ax_media_app_untrusted_handler.h"

#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
#include "third_party/re2/src/re2/re2.h"
#include "ui/accessibility/ax_tree.h"
#endif  // BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)

namespace ash::test {

std::string TestAXMediaAppUntrustedHandler::GetDocumentTreeToStringForTesting()
    const {
  if (!document_.ax_tree()) {
    return {};
  }
  return document_.ax_tree()->ToString();
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
  if (is_ocr_service_enabled_for_testing_) {
    return true;
  }
  return AXMediaAppUntrustedHandler::IsOcrServiceEnabled();
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

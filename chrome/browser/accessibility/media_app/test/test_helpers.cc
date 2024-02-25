// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility/media_app/test/test_helpers.h"

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
  std::string document_tree_string;
  ui::AXTreeData document_tree_data = document_.GetTreeData();
  document_.ax_tree()->UpdateDataForTesting(ui::AXTreeData());  // IN-TEST
  document_tree_string = document_.ax_tree()->ToString();
  document_.ax_tree()->UpdateDataForTesting(document_tree_data);  // IN-TEST

  // Remove the `child_tree_id` from `document_tree_string`, because it changes
  // every time the document tree is created.
  constexpr char pattern[] = "child_tree_id=\\S*";
  re2::RE2::GlobalReplace(&document_tree_string, pattern, "");
  return document_tree_string;
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

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility/media_app/test/test_ax_media_app_untrusted_handler.h"

#include <utility>

#include "content/public/browser/browser_context.h"
#include "ui/accessibility/ax_tree.h"
#include "ui/accessibility/ax_tree_id.h"
#include "ui/accessibility/ax_tree_update.h"

#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
#include "chrome/browser/screen_ai/public/test/fake_optical_character_recognizer.h"
#endif  // BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)

namespace ash::test {

TestAXMediaAppUntrustedHandler::TestAXMediaAppUntrustedHandler(
    content::BrowserContext& context,
    gfx::NativeWindow native_window,
    mojo::PendingRemote<media_app_ui::mojom::OcrUntrustedPage> page)
    : AXMediaAppUntrustedHandler(context, native_window, std::move(page)) {}

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
      std::make_unique<std::vector<ui::AXTreeUpdate>>();
}

#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
void TestAXMediaAppUntrustedHandler::
    CreateFakeOpticalCharacterRecognizerForTesting(bool return_empty) {
  ocr_.reset();
  ocr_ = screen_ai::FakeOpticalCharacterRecognizer::Create(return_empty);
}

void TestAXMediaAppUntrustedHandler::FlushForTesting() {
  ocr_->FlushForTesting();  // IN-TEST
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

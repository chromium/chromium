// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility/media_app/test/test_ax_media_app_untrusted_handler.h"

#include <utility>

#include "chrome/browser/screen_ai/public/test/fake_optical_character_recognizer.h"
#include "content/public/browser/browser_context.h"
#include "ui/accessibility/ax_tree.h"
#include "ui/accessibility/ax_tree_id.h"
#include "ui/accessibility/ax_tree_update.h"

namespace ash::test {

TestAXMediaAppUntrustedHandler::TestAXMediaAppUntrustedHandler(
    content::BrowserContext& context,
    gfx::NativeWindow native_window,
    mojo::PendingRemote<media_app_ui::mojom::OcrUntrustedPage> page)
    : AXMediaAppUntrustedHandler(context, native_window, std::move(page)) {}

TestAXMediaAppUntrustedHandler::~TestAXMediaAppUntrustedHandler() = default;

std::string TestAXMediaAppUntrustedHandler::GetDocumentTreeToStringForTesting()
    const {
  if (!document_ || !document_->ax_tree()) {
    return {};
  }
  return document_->ax_tree()->ToString();
}

void TestAXMediaAppUntrustedHandler::
    EnablePendingSerializedUpdatesForTesting() {
  pending_serialized_updates_for_testing_ =
      std::make_unique<std::vector<ui::AXTreeUpdate>>();
}

void TestAXMediaAppUntrustedHandler::
    CreateFakeOpticalCharacterRecognizerForTesting(bool return_empty,
                                                   bool is_successful) {
  ocr_.reset();
  if (is_successful) {
    ocr_ = screen_ai::FakeOpticalCharacterRecognizer::Create(return_empty);
  }
  OnOCRServiceInitialized(is_successful);
}

void TestAXMediaAppUntrustedHandler::FlushForTesting() {
  ocr_->FlushForTesting();  // IN-TEST
}

bool TestAXMediaAppUntrustedHandler::IsOcrServiceEnabled() const {
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

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility/media_app/test/test_ax_media_app_untrusted_service.h"

#include <utility>

#include "chrome/browser/screen_ai/public/test/fake_optical_character_recognizer.h"
#include "content/public/browser/browser_context.h"
#include "ui/accessibility/ax_tree.h"
#include "ui/accessibility/ax_tree_id.h"
#include "ui/accessibility/ax_tree_update.h"

namespace ash::test {

TestAXMediaAppUntrustedService::TestAXMediaAppUntrustedService(
    content::BrowserContext& context,
    gfx::NativeWindow native_window,
    mojo::PendingRemote<media_app_ui::mojom::OcrUntrustedPage> page)
    : AXMediaAppUntrustedService(context, native_window, std::move(page)) {}

TestAXMediaAppUntrustedService::~TestAXMediaAppUntrustedService() = default;

std::string TestAXMediaAppUntrustedService::GetDocumentTreeToStringForTesting()
    const {
  if (!document_ || !document_->ax_tree()) {
    return {};
  }
  return document_->ax_tree()->ToString();
}

void TestAXMediaAppUntrustedService::
    EnablePendingSerializedUpdatesForTesting() {
  pending_serialized_updates_for_testing_ =
      std::make_unique<std::vector<ui::AXTreeUpdate>>();
}

void TestAXMediaAppUntrustedService::
    CreateFakeOpticalCharacterRecognizerForTesting(bool return_empty,
                                                   bool is_successful) {
  ocr_.reset();
  if (is_successful) {
    ocr_ = screen_ai::FakeOpticalCharacterRecognizer::Create(return_empty);
  }
  OnOCRServiceInitialized(is_successful);
}

void TestAXMediaAppUntrustedService::FlushForTesting() {
  ocr_->FlushForTesting();  // IN-TEST
}

bool TestAXMediaAppUntrustedService::IsOcrServiceEnabled() const {
  return AXMediaAppUntrustedService::IsOcrServiceEnabled();
}

void TestAXMediaAppUntrustedService::PushDirtyPageForTesting(
    const std::string& dirty_page_id) {
  AXMediaAppUntrustedService::PushDirtyPage(dirty_page_id);
}

std::string TestAXMediaAppUntrustedService::PopDirtyPageForTesting() {
  return AXMediaAppUntrustedService::PopDirtyPage();
}

void TestAXMediaAppUntrustedService::OcrNextDirtyPageIfAny() {
  if (delay_calling_ocr_next_dirty_page_) {
    return;
  }
  AXMediaAppUntrustedService::OcrNextDirtyPageIfAny();
}

}  // namespace ash::test

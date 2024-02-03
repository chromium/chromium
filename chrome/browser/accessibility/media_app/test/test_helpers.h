// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACCESSIBILITY_MEDIA_APP_TEST_TEST_HELPERS_H_
#define CHROME_BROWSER_ACCESSIBILITY_MEDIA_APP_TEST_TEST_HELPERS_H_

#include "chrome/browser/accessibility/media_app/ax_media_app.h"
#include "chrome/browser/accessibility/media_app/ax_media_app_untrusted_handler.h"
#include "components/services/screen_ai/buildflags/buildflags.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace ash {
class AXMediaAppUntrustedHandler;
}  // namespace ash

namespace ash::test {

class TestAXMediaAppUntrustedHandler : public AXMediaAppUntrustedHandler {
 public:
  TestAXMediaAppUntrustedHandler(
      content::BrowserContext& context,
      mojo::PendingRemote<media_app_ui::mojom::OcrUntrustedPage> page)
      : AXMediaAppUntrustedHandler(context, std::move(page)) {}

  // TODO(b/309860428): Delete once AXMediaApp is deleted.
  void SetMediaAppForTesting(AXMediaApp* media_app) { media_app_ = media_app; }

  std::string GetDocumentTreeToStringForTesting() const;

  const ui::AXTreeID GetDocumentTreeIDForTesting() const {
    return document_.GetTreeID();
  }

  std::map<const std::string, AXMediaAppPageMetadata>
  GetPageMetadataForTesting() {
    return page_metadata_;
  }

  const std::map<const std::string, std::unique_ptr<ui::AXTreeManager>>&
  GetPagesForTesting() {
    return pages_;
  }

  void SetIsOcrServiceEnabledForTesting() {
    is_ocr_service_enabled_for_testing_ = true;
  }

  // Whether to allow tests to manually allow the OcrNextDirtyPageIfAny() method
  // to be called to better control the order of execution.
  void SetDelayCallingOcrNextDirtyPage(bool delay) {
    delay_calling_ocr_next_dirty_page_ = delay;
  }

#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
  void SetScreenAIAnnotatorForTesting(
      mojo::PendingRemote<screen_ai::mojom::ScreenAIAnnotator>
          screen_ai_annotator);

  void FlushForTesting();
#endif  // BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)

  void PushDirtyPageForTesting(const std::string& dirty_page_id);
  std::string PopDirtyPageForTesting();

  // AXMediaAppUntrustedHandler:
  bool IsOcrServiceEnabled() const override;
  void OcrNextDirtyPageIfAny() override;

 private:
  bool is_ocr_service_enabled_for_testing_ = false;
  bool delay_calling_ocr_next_dirty_page_ = false;
};

}  // namespace ash::test

#endif  // CHROME_BROWSER_ACCESSIBILITY_MEDIA_APP_TEST_TEST_HELPERS_H_

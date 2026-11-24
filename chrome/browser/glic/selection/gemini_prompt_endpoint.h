// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_SELECTION_GEMINI_PROMPT_ENDPOINT_H_
#define CHROME_BROWSER_GLIC_SELECTION_GEMINI_PROMPT_ENDPOINT_H_

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/glic/host/glic.mojom-forward.h"
#include "chrome/browser/glic/public/glic_passkeys.h"
#include "chrome/browser/glic/selection/selection_context.h"
#include "chrome/browser/selection/prompt_endpoint.h"
#include "ui/gfx/geometry/size.h"

namespace glic {

class GlicKeyedService;
struct GlicInvokeOptions;

// Routes Smart Selection prompts to the Glic (Gemini in Chrome) side panel.
class GeminiPromptEndpoint : public ::selection::PromptEndpoint {
 public:
  explicit GeminiPromptEndpoint(
      InvokeWithAutoSubmitPasskey auto_submit_passkey);
  ~GeminiPromptEndpoint() override;

  GeminiPromptEndpoint(const GeminiPromptEndpoint&) = delete;
  GeminiPromptEndpoint& operator=(const GeminiPromptEndpoint&) = delete;

  // Builds a `TabContextResult` from an encoded viewport screenshot and
  // `page_context`, preserving APC, page metadata, screenshot info, and PDF
  // metadata/bytes. Exposed for unit tests.
  static mojom::TabContextResultPtr BuildTabContextResult(
      mojom::TabDataPtr tab_data,
      ::selection::PageContext::EncodedScreenshot encoded_screenshot,
      ::selection::PageContext page_context);

  // `::selection::PromptEndpoint`:
  ::selection::mojom::PromptEndpointId GetId() const override;
  bool IsAvailable(tabs::TabInterface& tab) const override;
  std::u16string GetDisplayName() const override;
  std::u16string GetPlaceholder() const override;
  void Prepare(tabs::TabInterface& tab) override;
  void CancelPendingRequests() override;
  void Submit(tabs::TabInterface& tab,
              std::string prompt,
              ::selection::PageContextWithSelection context) override;

 private:
  void InvokeGlic(GlicKeyedService& service,
                  std::string prompt,
                  GlicInvokeOptions options) const;

  void OnScreenshotEncoded(base::WeakPtr<tabs::TabInterface> weak_tab,
                           std::string prompt,
                           gfx::Size screenshot_size,
                           ::selection::PageContext page_context,
                           CapturedRegions captured_regions,
                           std::optional<std::vector<uint8_t>> encoded_bytes);

  const InvokeWithAutoSubmitPasskey auto_submit_passkey_;
  base::WeakPtrFactory<GeminiPromptEndpoint> weak_factory_{this};
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_SELECTION_GEMINI_PROMPT_ENDPOINT_H_

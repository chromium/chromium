// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/metrics/metrics_provider_common.h"

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "content/public/browser/browser_accessibility_state.h"
#include "ui/accessibility/ax_mode.h"

namespace performance_manager {

namespace {
ui::AXMode::ModeFlagHistogramValue ModeFlagsToEnum(uint32_t mode_flags) {
  switch (mode_flags) {
    case ui::AXMode::kNativeAPIs:
      return ui::AXMode::ModeFlagHistogramValue::UMA_AX_MODE_NATIVE_APIS;
    case ui::AXMode::kWebContents:
      return ui::AXMode::ModeFlagHistogramValue::UMA_AX_MODE_WEB_CONTENTS;
    case ui::AXMode::kInlineTextBoxes:
      return ui::AXMode::ModeFlagHistogramValue::UMA_AX_MODE_INLINE_TEXT_BOXES;
    case ui::AXMode::kScreenReader:
      return ui::AXMode::ModeFlagHistogramValue::UMA_AX_MODE_SCREEN_READER;
    case ui::AXMode::kHTML:
      return ui::AXMode::ModeFlagHistogramValue::UMA_AX_MODE_HTML;
    case ui::AXMode::kHTMLMetadata:
      return ui::AXMode::ModeFlagHistogramValue::UMA_AX_MODE_HTML_METADATA;
    case ui::AXMode::kLabelImages:
      return ui::AXMode::ModeFlagHistogramValue::UMA_AX_MODE_LABEL_IMAGES;
    case ui::AXMode::kPDFPrinting:
      return ui::AXMode::ModeFlagHistogramValue::UMA_AX_MODE_PDF;
    case ui::AXMode::kPDFOcr:
      return ui::AXMode::ModeFlagHistogramValue::UMA_AX_MODE_PDF_OCR;
    case ui::AXMode::kAnnotateMainNode:
      return ui::AXMode::ModeFlagHistogramValue::UMA_AX_MODE_ANNOTATE_MAIN_NODE;
    default:
      return ui::AXMode::ModeFlagHistogramValue::UMA_AX_MODE_MAX;
  }
}

void MaybeRecordAccessibilityModeFlags(const ui::AXMode& mode,
                                       uint32_t mode_flags) {
  if (mode.has_mode(mode_flags)) {
    ui::AXMode::ModeFlagHistogramValue mode_enum = ModeFlagsToEnum(mode_flags);
    if (mode_enum != ui::AXMode::ModeFlagHistogramValue::UMA_AX_MODE_MAX) {
      UMA_HISTOGRAM_ENUMERATION(
          "PerformanceManager.Experimental.AccessibilityModeFlag", mode_enum,
          ui::AXMode::ModeFlagHistogramValue::UMA_AX_MODE_MAX);
    }
  }
}
}  // namespace

MetricsProviderCommon::MetricsProviderCommon() = default;
MetricsProviderCommon::~MetricsProviderCommon() = default;

void MetricsProviderCommon::RecordA11yFlags() {
  const ui::AXMode mode =
      content::BrowserAccessibilityState::GetInstance()->GetAccessibilityMode();
  const bool is_mode_on = !mode.is_mode_off();

  UMA_HISTOGRAM_BOOLEAN(
      "PerformanceManager.Experimental.HasAccessibilityModeFlag", is_mode_on);

  if (is_mode_on) {
    MaybeRecordAccessibilityModeFlags(mode, ui::AXMode::kNativeAPIs);
    MaybeRecordAccessibilityModeFlags(mode, ui::AXMode::kWebContents);
    MaybeRecordAccessibilityModeFlags(mode, ui::AXMode::kInlineTextBoxes);
    MaybeRecordAccessibilityModeFlags(mode, ui::AXMode::kScreenReader);
    MaybeRecordAccessibilityModeFlags(mode, ui::AXMode::kHTML);
    MaybeRecordAccessibilityModeFlags(mode, ui::AXMode::kHTMLMetadata);
    MaybeRecordAccessibilityModeFlags(mode, ui::AXMode::kLabelImages);
    MaybeRecordAccessibilityModeFlags(mode, ui::AXMode::kPDFPrinting);
    MaybeRecordAccessibilityModeFlags(mode, ui::AXMode::kPDFOcr);
    MaybeRecordAccessibilityModeFlags(mode, ui::AXMode::kAnnotateMainNode);
  }
}

void MetricsProviderCommon::ProvideCurrentSessionData(
    metrics::ChromeUserMetricsExtension* /*uma_proto*/) {
  RecordA11yFlags();
}

}  // namespace performance_manager

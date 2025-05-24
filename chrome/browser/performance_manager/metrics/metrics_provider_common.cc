// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/metrics/metrics_provider_common.h"

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "content/public/browser/browser_accessibility_state.h"
#include "ui/accessibility/ax_mode.h"
#include "base/system/sys_info.h"
#include "base/process/process_metrics.h"

namespace performance_manager {

namespace {

uint64_t kBytesPerMb = 1024 * 1024;

#if BUILDFLAG(IS_MAC)
uint64_t kKilobytesPerMb = 1024;
#endif

ui::AXMode::ModeFlagHistogramValue ModeFlagsToEnum(uint32_t mode_flags) {
  switch (mode_flags) {
    case ui::AXMode::kNativeAPIs:
      return ui::AXMode::ModeFlagHistogramValue::UMA_AX_MODE_NATIVE_APIS;
    case ui::AXMode::kWebContents:
      return ui::AXMode::ModeFlagHistogramValue::UMA_AX_MODE_WEB_CONTENTS;
    case ui::AXMode::kInlineTextBoxes:
      return ui::AXMode::ModeFlagHistogramValue::UMA_AX_MODE_INLINE_TEXT_BOXES;
    case ui::AXMode::kExtendedProperties:
      return ui::AXMode::ModeFlagHistogramValue::
          UMA_AX_MODE_EXTENDED_PROPERTIES;
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
    case ui::AXMode::kScreenReader:
      return ui::AXMode::ModeFlagHistogramValue::UMA_AX_MODE_SCREEN_READER;
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

MetricsProviderCommon::MetricsProviderCommon() {
  available_memory_metrics_timer_.Start(
      FROM_HERE, base::Minutes(2),
      base::BindRepeating(&MetricsProviderCommon::RecordAvailableMemoryMetrics,
                          base::Unretained(this)));
}
MetricsProviderCommon::~MetricsProviderCommon() = default;

void MetricsProviderCommon::RecordAvailableMemoryMetrics() {
  auto available_bytes = base::SysInfo::AmountOfAvailablePhysicalMemory();
  auto total_bytes = base::SysInfo::AmountOfPhysicalMemory();

  base::UmaHistogramMemoryLargeMB("Memory.Experimental.AvailableMemoryMB",
                                  available_bytes / kBytesPerMb);
  base::UmaHistogramPercentage("Memory.Experimental.AvailableMemoryPercent",
                               available_bytes * 100 / total_bytes);

#if BUILDFLAG(IS_MAC)
  base::SystemMemoryInfoKB info;
  if (base::GetSystemMemoryInfo(&info)) {
    base::UmaHistogramMemoryLargeMB(
        "Memory.Experimental.MacFileBackedMemoryMB2",
        info.file_backed / kKilobytesPerMb);
    // `info.file_backed` is in kb, so multiply it by 1024 to get the amount of
    // bytes
    base::UmaHistogramPercentage(
        "Memory.Experimental.MacAvailableMemoryPercentFreePageCache2",
        (available_bytes +
         (base::checked_cast<uint64_t>(info.file_backed) * 1024u)) *
            100u / total_bytes);
  }
#endif
}

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
    MaybeRecordAccessibilityModeFlags(mode, ui::AXMode::kExtendedProperties);
    MaybeRecordAccessibilityModeFlags(mode, ui::AXMode::kHTML);
    MaybeRecordAccessibilityModeFlags(mode, ui::AXMode::kHTMLMetadata);
    MaybeRecordAccessibilityModeFlags(mode, ui::AXMode::kLabelImages);
    MaybeRecordAccessibilityModeFlags(mode, ui::AXMode::kPDFPrinting);
    MaybeRecordAccessibilityModeFlags(mode, ui::AXMode::kPDFOcr);
    MaybeRecordAccessibilityModeFlags(mode, ui::AXMode::kAnnotateMainNode);
    // ui::AXMode::kFromPlatform is unconditionally filtered out and is
    // therefore never present in `mode`.
    MaybeRecordAccessibilityModeFlags(mode, ui::AXMode::kScreenReader);
  }
}

void MetricsProviderCommon::ProvideCurrentSessionData(
    metrics::ChromeUserMetricsExtension* /*uma_proto*/) {
  RecordA11yFlags();
}

}  // namespace performance_manager

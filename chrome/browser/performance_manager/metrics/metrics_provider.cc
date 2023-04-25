// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/metrics/metrics_provider.h"

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/process/process_metrics.h"
#include "base/system/sys_info.h"
#include "base/timer/timer.h"
#include "chrome/browser/performance_manager/public/user_tuning/user_performance_tuning_manager.h"
#include "components/performance_manager/public/user_tuning/prefs.h"
#include "components/prefs/pref_service.h"
#include "ui/accessibility/ax_mode.h"
#include "ui/accessibility/platform/ax_platform_node.h"

namespace performance_manager {

namespace {

MetricsProvider* g_metrics_provider = nullptr;

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
    case ui::AXMode::kScreenReader:
      return ui::AXMode::ModeFlagHistogramValue::UMA_AX_MODE_SCREEN_READER;
    case ui::AXMode::kHTML:
      return ui::AXMode::ModeFlagHistogramValue::UMA_AX_MODE_HTML;
    case ui::AXMode::kHTMLMetadata:
      return ui::AXMode::ModeFlagHistogramValue::UMA_AX_MODE_HTML_METADATA;
    case ui::AXMode::kLabelImages:
      return ui::AXMode::ModeFlagHistogramValue::UMA_AX_MODE_LABEL_IMAGES;
    case ui::AXMode::kPDF:
      return ui::AXMode::ModeFlagHistogramValue::UMA_AX_MODE_PDF;
    case ui::AXMode::kPDFOcr:
      return ui::AXMode::ModeFlagHistogramValue::UMA_AX_MODE_PDF_OCR;
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

// static
MetricsProvider* MetricsProvider::GetInstance() {
  DCHECK(g_metrics_provider);
  return g_metrics_provider;
}

MetricsProvider::~MetricsProvider() {
  DCHECK_EQ(this, g_metrics_provider);
  g_metrics_provider = nullptr;
}

void MetricsProvider::Initialize() {
  DCHECK(!initialized_);

  pref_change_registrar_.Init(local_state_);
  pref_change_registrar_.Add(
      performance_manager::user_tuning::prefs::kHighEfficiencyModeEnabled,
      base::BindRepeating(&MetricsProvider::OnTuningModesChanged,
                          base::Unretained(this)));
  performance_manager::user_tuning::UserPerformanceTuningManager::GetInstance()
      ->AddObserver(this);
  battery_saver_enabled_ = performance_manager::user_tuning::
                               UserPerformanceTuningManager::GetInstance()
                                   ->IsBatterySaverActive();

  initialized_ = true;
  current_mode_ = ComputeCurrentMode();
}

void MetricsProvider::RecordA11yFlags() {
  ui::AXMode mode = ui::AXPlatformNode::GetAccessibilityMode();
  bool is_mode_on = !mode.is_mode_off();

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
    MaybeRecordAccessibilityModeFlags(mode, ui::AXMode::kPDF);
    MaybeRecordAccessibilityModeFlags(mode, ui::AXMode::kPDFOcr);
  }
}

void MetricsProvider::ProvideCurrentSessionData(
    metrics::ChromeUserMetricsExtension* uma_proto) {
  // It's valid for this to be called when `initialized_` is false if the finch
  // features controlling battery saver and high efficiency are disabled.
  // TODO(crbug.com/1348590): CHECK(initialized_) when the features are enabled
  // and removed.
  base::UmaHistogramEnumeration("PerformanceManager.UserTuning.EfficiencyMode",
                                current_mode_);

  // Set `current_mode_` to represent the state of the modes as they are now, so
  // that this mode is what is adequately reported at the next report, unless it
  // changes in the meantime.
  current_mode_ = ComputeCurrentMode();

  RecordA11yFlags();
}

MetricsProvider::MetricsProvider(PrefService* local_state)
    : local_state_(local_state) {
  DCHECK(!g_metrics_provider);
  g_metrics_provider = this;

  available_memory_metrics_timer_.Start(
      FROM_HERE, base::Minutes(2),
      base::BindRepeating(&MetricsProvider::RecordAvailableMemoryMetrics,
                          base::Unretained(this)));
}

void MetricsProvider::OnBatterySaverModeChanged(bool is_active) {
  battery_saver_enabled_ = is_active;
  OnTuningModesChanged();
}

void MetricsProvider::OnTuningModesChanged() {
  EfficiencyMode new_mode = ComputeCurrentMode();

  // If the mode changes between UMA reports, mark it as Mixed for this
  // interval.
  if (current_mode_ != new_mode) {
    current_mode_ = EfficiencyMode::kMixed;
  }
}

MetricsProvider::EfficiencyMode MetricsProvider::ComputeCurrentMode() const {
  // It's valid for this to be uninitialized if the battery saver/high
  // efficiency modes are unavailable. In that case, the browser is running in
  // normal mode, so return kNormal.
  // TODO(crbug.com/1348590): Change this to a DCHECK when the features are
  // enabled and removed.
  if (!initialized_) {
    return EfficiencyMode::kNormal;
  }

  // It's possible for this function to be called during shutdown, after
  // UserPerformanceTuningManager is destroyed. Do not access UPTM directly from
  // here.

  bool high_efficiency_enabled = local_state_->GetBoolean(
      performance_manager::user_tuning::prefs::kHighEfficiencyModeEnabled);

  if (high_efficiency_enabled && battery_saver_enabled_) {
    return EfficiencyMode::kBoth;
  }

  if (high_efficiency_enabled) {
    return EfficiencyMode::kHighEfficiency;
  }

  if (battery_saver_enabled_) {
    return EfficiencyMode::kBatterySaver;
  }

  return EfficiencyMode::kNormal;
}

void MetricsProvider::RecordAvailableMemoryMetrics() {
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
        (available_bytes + (info.file_backed * 1024)) * 100 / total_bytes);
  }
#endif
}

}  // namespace performance_manager

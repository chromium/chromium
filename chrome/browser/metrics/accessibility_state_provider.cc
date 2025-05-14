// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/accessibility_state_provider.h"

#include "content/public/browser/browser_accessibility_state.h"
#include "third_party/metrics_proto/system_profile.pb.h"
#include "ui/accessibility/ax_mode.h"

namespace {

metrics::SystemProfileProto::AccessibilityState::AXMode ModeFlagsToProtoEnum(
    uint32_t mode_flags) {
  switch (mode_flags) {
    case ui::AXMode::kNativeAPIs:
      return metrics::SystemProfileProto::AccessibilityState::NATIVE_APIS;
    case ui::AXMode::kWebContents:
      return metrics::SystemProfileProto::AccessibilityState::WEB_CONTENTS;
    case ui::AXMode::kInlineTextBoxes:
      return metrics::SystemProfileProto::AccessibilityState::INLINE_TEXT_BOXES;
    case ui::AXMode::kExtendedProperties:
      return metrics::SystemProfileProto::AccessibilityState::
          EXTENDED_PROPERTIES;
    case ui::AXMode::kHTML:
      return metrics::SystemProfileProto::AccessibilityState::HTML;
    case ui::AXMode::kHTMLMetadata:
      return metrics::SystemProfileProto::AccessibilityState::HTML_METADATA;
    case ui::AXMode::kLabelImages:
      return metrics::SystemProfileProto::AccessibilityState::LABEL_IMAGES;
    case ui::AXMode::kPDFPrinting:
      return metrics::SystemProfileProto::AccessibilityState::PDF_PRINTING;
    case ui::AXMode::kPDFOcr:
      return metrics::SystemProfileProto::AccessibilityState::PDF_OCR;
    case ui::AXMode::kAnnotateMainNode:
      return metrics::SystemProfileProto::AccessibilityState::
          ANNOTATE_MAIN_NODE;
    case ui::AXMode::kScreenReader:
      return metrics::SystemProfileProto::AccessibilityState::SCREEN_READER;
    default:
      NOTREACHED();
  }
}

void MaybeAddAccessibilityModeFlags(
    const ui::AXMode& mode,
    uint32_t mode_flags,
    metrics::SystemProfileProto::AccessibilityState* state) {
  if (mode.has_mode(mode_flags)) {
    state->add_enabled_modes(ModeFlagsToProtoEnum(mode_flags));
  }
}

}  // namespace

AccessibilityStateProvider::AccessibilityStateProvider() = default;
AccessibilityStateProvider::~AccessibilityStateProvider() = default;

void AccessibilityStateProvider::ProvideSystemProfileMetrics(
    metrics::SystemProfileProto* system_profile) {
  const ui::AXMode mode =
      content::BrowserAccessibilityState::GetInstance()->GetAccessibilityMode();
  if (mode.is_mode_off()) {
    return;
  }
  if (content::BrowserAccessibilityState::GetInstance()
          ->IsAccessibilityPerformanceMeasurementExperimentActive()) {
    // An active experiment means that the existing AXMode were not user
    // initiated. We don't want to record those AXModes in the UMA.
    return;
  }

  auto* state = system_profile->mutable_accessibility_state();

  MaybeAddAccessibilityModeFlags(mode, ui::AXMode::kNativeAPIs, state);
  MaybeAddAccessibilityModeFlags(mode, ui::AXMode::kWebContents, state);
  MaybeAddAccessibilityModeFlags(mode, ui::AXMode::kInlineTextBoxes, state);
  MaybeAddAccessibilityModeFlags(mode, ui::AXMode::kExtendedProperties, state);
  MaybeAddAccessibilityModeFlags(mode, ui::AXMode::kHTML, state);
  MaybeAddAccessibilityModeFlags(mode, ui::AXMode::kHTMLMetadata, state);
  MaybeAddAccessibilityModeFlags(mode, ui::AXMode::kLabelImages, state);
  MaybeAddAccessibilityModeFlags(mode, ui::AXMode::kPDFPrinting, state);
  MaybeAddAccessibilityModeFlags(mode, ui::AXMode::kPDFOcr, state);
  MaybeAddAccessibilityModeFlags(mode, ui::AXMode::kAnnotateMainNode, state);
  // ui::AXMode::kFromPlatform is unconditionally filtered out and is therefore
  // never present in `mode`.
  CHECK(!mode.has_mode(ui::AXMode::kFromPlatform));
  MaybeAddAccessibilityModeFlags(mode, ui::AXMode::kScreenReader, state);
}

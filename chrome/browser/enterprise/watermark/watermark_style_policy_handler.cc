// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/watermark/watermark_style_policy_handler.h"

#include "components/enterprise/connectors/core/connectors_prefs.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"
#include "components/strings/grit/components_strings.h"

WatermarkStylePolicyHandler::WatermarkStylePolicyHandler(policy::Schema schema)
    : CloudOnlyPolicyHandler(
          policy::key::kWatermarkStyle,
          schema.GetKnownProperty(policy::key::kWatermarkStyle),
          policy::SchemaOnErrorStrategy::SCHEMA_ALLOW_UNKNOWN) {}

WatermarkStylePolicyHandler::~WatermarkStylePolicyHandler() = default;

void WatermarkStylePolicyHandler::ApplyPolicySettings(
    const policy::PolicyMap& policies,
    PrefValueMap* prefs) {
  const base::Value* value =
      policies.GetValue(policy::key::kWatermarkStyle, base::Value::Type::DICT);
  if (!value) {
    return;
  }

  std::optional<int> fill_opacity = value->GetDict().FindInt(
      enterprise_connectors::kWatermarkStyleFillOpacityFieldName);
  if (fill_opacity) {
    prefs->SetInteger(enterprise_connectors::kWatermarkStyleFillOpacityPref,
                      fill_opacity.value());
  }

  std::optional<int> outline_opacity = value->GetDict().FindInt(
      enterprise_connectors::kWatermarkStyleOutlineOpacityFieldName);
  if (outline_opacity) {
    prefs->SetInteger(enterprise_connectors::kWatermarkStyleOutlineOpacityPref,
                      outline_opacity.value());
  }

  std::optional<int> font_size = value->GetDict().FindInt(
      enterprise_connectors::kWatermarkStyleFontSizeFieldName);
  if (font_size) {
    prefs->SetInteger(enterprise_connectors::kWatermarkStyleFontSizePref,
                      font_size.value());
  }
}

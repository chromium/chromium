// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/compose/compose_text_usage_logger.h"

#include <algorithm>
#include <bit>
#include <cstdint>

#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_tokenizer.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/signatures.h"
#include "components/compose/core/browser/compose_features.h"
#include "services/metrics/public/cpp/metrics_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"

namespace compose {
DOCUMENT_USER_DATA_KEY_IMPL(ComposeTextUsageLogger);

namespace {
constexpr int MAX_FIELD_METRIC_COUNT = 100;
// Note: Although OnAfterTextFieldDidChange is only called for user
// actions like typing or pastes, we need to handle the case where the
// user edits the field after it was modified by the page. In this case,
// we'd rather be conservative when recording text changes as user typing. If
// the text length changes by more than this number, we'll ignore the change.
constexpr int MAX_CHARS_TYPED_AT_ONCE = 10;
constexpr base::TimeDelta EDITING_TIME_IDLE_TIMEOUT = base::Seconds(5);

int64_t CountWords(const std::u16string& value) {
  int64_t words = 0;
  base::String16Tokenizer tokenizer(
      value, u"", base::String16Tokenizer::WhitespacePolicy::kSkipOver);
  while (tokenizer.GetNext()) {
    ++words;
  }
  return words;
}

size_t RoundDownToPowerOfTwo(int64_t n) {
  // We use -1 as a special value indicating unknown.
  if (n < 0) {
    return -1;
  }
  return std::bit_floor<uint64_t>(n);
}

}  // namespace

ComposeTextUsageLogger::FieldMetrics::FieldMetrics() noexcept = default;
ComposeTextUsageLogger::FieldMetrics::~FieldMetrics() = default;

ComposeTextUsageLogger::ComposeTextUsageLogger(content::RenderFrameHost* rfh)
    : content::DocumentUserData<ComposeTextUsageLogger>(rfh) {
  autofill::ContentAutofillDriver* driver =
      autofill::ContentAutofillDriver::GetForRenderFrameHost(rfh);
  if (driver) {
    driver->GetAutofillManager().AddObserver(this);
  }
}

ComposeTextUsageLogger::~ComposeTextUsageLogger() {
  Reset();

  autofill::ContentAutofillDriver* driver =
      autofill::ContentAutofillDriver::GetForRenderFrameHost(
          &render_frame_host());
  if (driver) {
    driver->GetAutofillManager().RemoveObserver(this);
  }
}

void ComposeTextUsageLogger::OnAfterTextFieldDidChange(
    autofill::AutofillManager& manager,
    autofill::FormGlobalId form,
    autofill::FieldGlobalId field,
    const std::u16string& text_value) {
  autofill::FormType form_type = autofill::FormType::kUnknownFormType;
  int64_t form_control_type = -1;
  autofill::FieldSignature field_signature;
  autofill::FormSignature form_signature;
  autofill::FormStructure* form_structure = manager.FindCachedFormById(form);
  bool is_long_field = false;
  if (form_structure) {
    form_signature = form_structure->form_signature();
    const autofill::AutofillField* field_data =
        form_structure->GetFieldById(field);
    if (field_data) {
      form_type = FieldTypeGroupToFormType(field_data->Type().group());
      form_control_type = static_cast<int64_t>(field_data->form_control_type());

      switch (field_data->form_control_type()) {
        case autofill::FormControlType::kContentEditable:
        case autofill::FormControlType::kTextArea:
          is_long_field = true;
          break;
        default:
          break;
      }
      field_signature = field_data->GetFieldSignature();
    }
  }

  // The page UKM source ID should not change while this object is alive. Keep
  // a copy of it stored so that we can log safely in the destructor.
  DCHECK(source_id_ == ukm::SourceId() ||
         source_id_ == render_frame_host().GetPageUkmSourceId())
      << "source_id shouldn't change";

  if (source_id_ == ukm::SourceId()) {
    source_id_ = render_frame_host().GetPageUkmSourceId();
  }

  if (field_metrics_.size() >= MAX_FIELD_METRIC_COUNT) {
    Reset();
  }

  FieldMetrics& metrics = field_metrics_[field];
  if (!metrics.initialized) {
    if (text_value.length() > MAX_CHARS_TYPED_AT_ONCE) {
      metrics.initial_text = text_value;
      metrics.previous_text_length = text_value.length();
    }
    metrics.initialized = true;
  } else {
    base::TimeDelta additional_editing_time =
        std::min(base::TimeTicks::Now() - metrics.last_update_time,
                 EDITING_TIME_IDLE_TIMEOUT);
    metrics.editing_time += additional_editing_time;
  }
  metrics.last_update_time = base::TimeTicks::Now();

  switch (form_type) {
    case autofill::FormType::kUnknownFormType:
      break;
    case autofill::FormType::kAddressForm:
      metrics.is_autofill_field_type = true;
      break;
    case autofill::FormType::kStandaloneCvcForm:
    case autofill::FormType::kCreditCardForm:
    case autofill::FormType::kPasswordForm:
      metrics.sensitive_field = true;
      metrics.is_autofill_field_type = true;
      break;
  }

  // Note that field_data->value doesn't have the current value, so we use
  // text_value instead.
  const int64_t new_length = text_value.size();
  const int64_t delta = new_length - metrics.previous_text_length;
  if (delta > 0 && delta <= MAX_CHARS_TYPED_AT_ONCE) {
    metrics.estimate_typed_characters += delta;
  }

  metrics.form_control_type = form_control_type;

  metrics.is_long_field = is_long_field;
  metrics.field_signature = field_signature;
  metrics.form_signature = form_signature;

  metrics.previous_text_length = text_value.length();
  metrics.final_text = std::move(text_value);
}

void ComposeTextUsageLogger::Reset() {
  if (field_metrics_.empty()) {
    return;
  }

  for (const auto& entry : field_metrics_) {
    const FieldMetrics& metrics = entry.second;
    if (metrics.final_text.size() == 0) {
      continue;
    }

    int64_t typed_chars = -1;
    int64_t typed_words = -1;
    if (!metrics.sensitive_field) {
      int64_t size_change = static_cast<int64_t>(metrics.final_text.size()) -
                            static_cast<int64_t>(metrics.initial_text.size());
      typed_chars = std::min(metrics.estimate_typed_characters, size_change);

      typed_words = std::max<int64_t>(
          0, CountWords(metrics.final_text) - CountWords(metrics.initial_text));
    }

    ukm::builders::Compose_TextElementUsage builder(source_id_);
    builder.SetAutofillFormControlType(metrics.form_control_type)
        .SetTypedCharacterCount(RoundDownToPowerOfTwo(typed_chars))
        .SetTypedWordCount(RoundDownToPowerOfTwo(typed_words))
        .SetIsAutofillFieldType(metrics.is_autofill_field_type);

    if (base::FeatureList::IsEnabled(features::kEnableAdditionalTextMetrics)) {
      builder
          .SetFieldSignature(
              autofill::HashFieldSignature(metrics.field_signature))
          .SetFormSignature(autofill::HashFormSignature(metrics.form_signature))
          .SetEditingTime(ukm::GetExponentialBucketMinForUserTiming(
              metrics.editing_time.InSeconds()));
      if (metrics.is_long_field) {
        base::UmaHistogramCustomTimes(
            "Compose.TextElementUsage.LongField.EditingTime",
            metrics.editing_time, base::Seconds(2), base::Minutes(20), 50);
      }
    }

    builder.Record(ukm::UkmRecorder::Get());
  }
  field_metrics_.clear();
}

}  // namespace compose

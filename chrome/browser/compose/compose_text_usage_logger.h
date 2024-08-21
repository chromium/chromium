// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPOSE_COMPOSE_TEXT_USAGE_LOGGER_H_
#define CHROME_BROWSER_COMPOSE_COMPOSE_TEXT_USAGE_LOGGER_H_

#include "components/autofill/core/browser/autofill_manager.h"
#include "components/autofill/core/common/signatures.h"
#include "content/public/browser/document_user_data.h"

namespace compose {

// Logs UKM for text writing use on web pages. There is one instance of this
// per page.
class ComposeTextUsageLogger
    : public content::DocumentUserData<ComposeTextUsageLogger>,
      public autofill::AutofillManager::Observer {
 public:
  ~ComposeTextUsageLogger() override;

  // autofill::AutofillManager::Observer:
  void OnAfterTextFieldDidChange(autofill::AutofillManager& manager,
                                 autofill::FormGlobalId form,
                                 autofill::FieldGlobalId field,
                                 const std::u16string& text_value) override;

 private:
  // No public constructors to force going through static methods of
  // DocumentUserData (e.g. CreateForCurrentDocument).
  explicit ComposeTextUsageLogger(content::RenderFrameHost* rfh);

  // Flushes pending logs.
  void Reset();

  struct FieldMetrics {
    FieldMetrics() noexcept;
    ~FieldMetrics();
    bool initialized = false;
    bool sensitive_field = false;
    int64_t estimate_typed_characters = 0;
    int64_t form_control_type = -1;
    autofill::FieldSignature field_signature;
    autofill::FormSignature form_signature;
    base::TimeTicks last_update_time;
    base::TimeDelta editing_time;

    std::u16string initial_text;
    std::u16string final_text;
    int64_t previous_text_length = 0;
    bool is_autofill_field_type = false;
    // Is it either a textarea or a contenteditable.
    bool is_long_field = false;
  };

  raw_ptr<ukm::UkmRecorder> ukm_recorder_;
  ukm::SourceId source_id_ = ukm::SourceId();

  std::map<autofill::FieldGlobalId, FieldMetrics> field_metrics_;

  friend DocumentUserData;
  DOCUMENT_USER_DATA_KEY_DECL();
};

}  // namespace compose

#endif  // CHROME_BROWSER_COMPOSE_COMPOSE_TEXT_USAGE_LOGGER_H_

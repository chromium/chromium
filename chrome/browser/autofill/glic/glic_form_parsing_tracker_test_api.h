// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_GLIC_GLIC_FORM_PARSING_TRACKER_TEST_API_H_
#define CHROME_BROWSER_AUTOFILL_GLIC_GLIC_FORM_PARSING_TRACKER_TEST_API_H_

#include "base/check_deref.h"
#include "chrome/browser/autofill/glic/glic_form_parsing_tracker.h"

namespace autofill {

class GlicFormParsingTrackerTestApi {
 public:
  explicit GlicFormParsingTrackerTestApi(GlicFormParsingTracker* tracker)
      : tracker_(CHECK_DEREF(tracker)) {}

  const absl::flat_hash_map<FormGlobalId,
                            GlicFormParsingTracker::FormParsingStatus>&
  form_parsing_status() const {
    return tracker_->form_parsing_status_;
  }

  size_t num_callbacks() const { return tracker_->callbacks_.size(); }

 private:
  const raw_ref<GlicFormParsingTracker> tracker_;
};

GlicFormParsingTrackerTestApi test_api(GlicFormParsingTracker& tracker) {
  return GlicFormParsingTrackerTestApi(&tracker);
}

}  // namespace autofill

#endif  // CHROME_BROWSER_AUTOFILL_GLIC_GLIC_FORM_PARSING_TRACKER_TEST_API_H_

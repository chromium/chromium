// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_EXPERIMENTAL_OPT_IN_GLIC_EXPERIMENTAL_OPT_IN_METRICS_H_
#define CHROME_BROWSER_GLIC_EXPERIMENTAL_OPT_IN_GLIC_EXPERIMENTAL_OPT_IN_METRICS_H_

#include "chrome/browser/glic/public/glic_enabling.h"

namespace glic {

// Called when the Glic Experimental Opt-In dialog is shown.
void RecordExperimentalOptInShown(RequiredExperimentalOptIn required_state);

// Called when the Glic Experimental Opt-In dialog is accepted.
void RecordExperimentalOptInAccepted(RequiredExperimentalOptIn required_state);

// Called when the Glic Experimental Opt-In dialog is rejected/cancelled.
void RecordExperimentalOptInRejected(RequiredExperimentalOptIn required_state);

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_EXPERIMENTAL_OPT_IN_GLIC_EXPERIMENTAL_OPT_IN_METRICS_H_

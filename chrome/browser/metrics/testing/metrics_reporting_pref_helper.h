// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_TESTING_METRICS_REPORTING_PREF_HELPER_H_
#define CHROME_BROWSER_METRICS_TESTING_METRICS_REPORTING_PREF_HELPER_H_

#include "base/files/file_path.h"
#include "base/path_service.h"

namespace metrics {

// Configures on-disk prefs to mark that metrics reporting is enabled/disabled
// based on input |is_enabled|. This is used in browser tests to setup the
// correct input conditions to be validated. This should generally be called
// within SetUpUserDataDirectory. Returns the filepath of the local state file
// which can be used for future verification. Returns empty filepath if there
// was an error.
base::FilePath SetUpUserDataDirectoryForTesting(bool is_enabled);

}  // namespace metrics

#endif  // CHROME_BROWSER_METRICS_TESTING_METRICS_REPORTING_PREF_HELPER_H_

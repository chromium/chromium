// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FEEDBACK_SYSTEM_LOGS_CHROME_SYSTEM_LOGS_FETCHER_H_
#define CHROME_BROWSER_FEEDBACK_SYSTEM_LOGS_CHROME_SYSTEM_LOGS_FETCHER_H_

class Profile;

namespace system_logs {

class SystemLogsFetcher;

// Creates a SystemLogsFetcher to aggregate the scrubbed logs for sending with
// feedback reports. If |scrub_data| is true then the logs are scrubbed of PII.
// The fetcher deletes itself once it finishes fetching data.
SystemLogsFetcher* BuildChromeSystemLogsFetcher(Profile* profile,
                                                bool scrub_data);

}  // namespace system_logs

#endif  // CHROME_BROWSER_FEEDBACK_SYSTEM_LOGS_CHROME_SYSTEM_LOGS_FETCHER_H_

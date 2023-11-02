// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LACROS_SYSTEM_LOGS_LACROS_SYSTEM_LOG_FETCHER_H_
#define CHROME_BROWSER_LACROS_SYSTEM_LOGS_LACROS_SYSTEM_LOG_FETCHER_H_

namespace system_logs {

class SystemLogsFetcher;

// Creates a SystemLogsFetcher to aggregate the scrubbed lacros logs for sending
// with unified feedback reports. If |scrub_data| is true then the logs are
// scrubbed of PII.
// The fetcher deletes itself once it finishes fetching data.
SystemLogsFetcher* BuildLacrosSystemLogsFetcher(bool scrub_data);

}  // namespace system_logs

#endif  // CHROME_BROWSER_LACROS_SYSTEM_LOGS_LACROS_SYSTEM_LOG_FETCHER_H_

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DATA_REDUCTION_PROXY_DATA_REDUCTION_PROXY_CHROME_SETTINGS_H_
#define CHROME_BROWSER_DATA_REDUCTION_PROXY_DATA_REDUCTION_PROXY_CHROME_SETTINGS_H_

#include "base/memory/raw_ptr.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_settings.h"
#include "components/keyed_service/core/keyed_service.h"

class Profile;

namespace base {
class SequencedTaskRunner;
}  // namespace base

// Data reduction proxy settings class suitable for use with a Chrome browser.
// It is keyed to a browser context.
class DataReductionProxyChromeSettings
    : public data_reduction_proxy::DataReductionProxySettings,
      public KeyedService {
 public:
  // Constructs a settings object. Construction and destruction must happen on
  // the UI thread.
  explicit DataReductionProxyChromeSettings(bool is_off_the_record_profile);

  DataReductionProxyChromeSettings(const DataReductionProxyChromeSettings&) =
      delete;
  DataReductionProxyChromeSettings& operator=(
      const DataReductionProxyChromeSettings&) = delete;

  // Destructs the settings object.
  ~DataReductionProxyChromeSettings() override;

  // Overrides KeyedService::Shutdown:
  void Shutdown() override;

  // Initialize the settings object.
  void InitDataReductionProxySettings(
      Profile* profile,
      const scoped_refptr<base::SequencedTaskRunner>& db_task_runner);
};

#endif  // CHROME_BROWSER_DATA_REDUCTION_PROXY_DATA_REDUCTION_PROXY_CHROME_SETTINGS_H_

// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_NOTIFIER_DATASET_H_
#define CHROME_BROWSER_NOTIFICATIONS_NOTIFIER_DATASET_H_

#include <string>

struct NotifierDataset {
  NotifierDataset(const std::string& app_id,
                  const std::string& app_name,
                  const std::string& publisher_id,
                  bool enabled);
  NotifierDataset(const NotifierDataset&) = delete;
  NotifierDataset& operator=(const NotifierDataset&) = delete;
  // Move ctors are needed to push_back() into a vector.
  NotifierDataset(NotifierDataset&& notifier_dataset);
  ~NotifierDataset();

  const std::string app_id;
  const std::string app_name;
  const std::string publisher_id;
  const bool enabled;
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_NOTIFIER_DATASET_H_

// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/notifier_dataset.h"

NotifierDataset::NotifierDataset(const std::string& app_id,
                                 const std::string& app_name,
                                 const std::string& publisher_id,
                                 bool enabled)
    : app_id(app_id),
      app_name(app_name),
      publisher_id(publisher_id),
      enabled(enabled) {}

NotifierDataset::NotifierDataset(NotifierDataset&& other) = default;

NotifierDataset::~NotifierDataset() = default;

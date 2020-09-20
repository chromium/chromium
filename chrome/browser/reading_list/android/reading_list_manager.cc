// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/reading_list/android/reading_list_manager.h"

#include "components/reading_list/core/reading_list_model.h"

ReadingListManager::ReadingListManager(ReadingListModel* reading_list_model)
    : reading_list_model_(reading_list_model) {
  DCHECK(reading_list_model_);
}

ReadingListManager::~ReadingListManager() = default;

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dips/chrome_dips_delegate.h"

#include "base/types/pass_key.h"
#include "chrome/browser/browsing_data/chrome_browsing_data_remover_constants.h"

ChromeDipsDelegate::ChromeDipsDelegate(
    base::PassKey<ChromeContentBrowserClient>) {}

bool ChromeDipsDelegate::ShouldDeleteInteractionRecords(uint64_t remove_mask) {
  return remove_mask & chrome_browsing_data_remover::DATA_TYPE_HISTORY;
}

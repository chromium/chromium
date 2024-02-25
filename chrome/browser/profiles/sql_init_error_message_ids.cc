// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/sql_init_error_message_ids.h"

#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"

int SqlInitStatusToMessageId(sql::InitStatus status) {
  if (status == sql::INIT_OK_WITH_DATA_LOSS)
    return IDS_OPEN_PROFILE_DATA_LOSS;

  if (status == sql::INIT_FAILURE)
    return IDS_COULDNT_OPEN_PROFILE_ERROR;

  return IDS_PROFILE_TOO_NEW_ERROR;
}

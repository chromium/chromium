// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_SQL_INIT_ERROR_MESSAGE_IDS_H_
#define CHROME_BROWSER_PROFILES_SQL_INIT_ERROR_MESSAGE_IDS_H_

#include "sql/init_status.h"

// Gets the ID of the localized error message that will be shown in the profile
// error dialog which corresponds to the SQL initialization |status|.
int SqlInitStatusToMessageId(sql::InitStatus status);

#endif  // CHROME_BROWSER_PROFILES_SQL_INIT_ERROR_MESSAGE_IDS_H_

// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BOOKMARKS_URL_AND_ID_H_
#define CHROME_BROWSER_BOOKMARKS_URL_AND_ID_H_

#include "url/gurl.h"

struct UrlAndId {
  GURL url;
  int64_t id = -1;
};

#endif  // CHROME_BROWSER_BOOKMARKS_URL_AND_ID_H_

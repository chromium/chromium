// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREFETCH_SEARCH_PREFETCH_SEARCH_PREFETCH_SERVICE_H_
#define CHROME_BROWSER_PREFETCH_SEARCH_PREFETCH_SEARCH_PREFETCH_SERVICE_H_

#include "components/keyed_service/core/keyed_service.h"

class Profile;

class SearchPrefetchService : public KeyedService {
 public:
  explicit SearchPrefetchService(Profile* profile);
  ~SearchPrefetchService() override;

 private:
  Profile* profile_;
};

#endif  // CHROME_BROWSER_PREFETCH_SEARCH_PREFETCH_SEARCH_PREFETCH_SERVICE_H_

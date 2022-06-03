// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_WEB_APPLICATIONS_CROSH_LOADER_H_
#define CHROME_BROWSER_ASH_WEB_APPLICATIONS_CROSH_LOADER_H_

#include "components/keyed_service/core/keyed_service.h"

class Profile;

// Loads crosh DataSource at startup.
// TODO(crbug.com/1080384): This service can be removed once chrome-untrusted
// has WebUIControllers.
class CroshLoader : public KeyedService {
 public:
  explicit CroshLoader(Profile* profile);
  ~CroshLoader() override;
  CroshLoader(const CroshLoader&) = delete;
  CroshLoader& operator=(const CroshLoader&) = delete;
};

#endif  // CHROME_BROWSER_ASH_WEB_APPLICATIONS_CROSH_LOADER_H_

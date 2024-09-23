// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TPCD_METADATA_MANAGER_FACTORY_H_
#define CHROME_BROWSER_TPCD_METADATA_MANAGER_FACTORY_H_

#include "chrome/browser/profiles/profile.h"
#include "components/tpcd/metadata/browser/manager.h"

namespace tpcd::metadata {

class ManagerFactory : public Manager {
 public:
  static Manager* GetForProfile(Profile* profile);

  ManagerFactory() = delete;
  ManagerFactory(const ManagerFactory&) = delete;
  ManagerFactory& operator=(const ManagerFactory&) = delete;

 private:
  ~ManagerFactory() override = default;
};

}  // namespace tpcd::metadata
#endif  // CHROME_BROWSER_TPCD_METADATA_MANAGER_FACTORY_H_

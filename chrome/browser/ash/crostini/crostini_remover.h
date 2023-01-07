// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSTINI_CROSTINI_REMOVER_H_
#define CHROME_BROWSER_ASH_CROSTINI_CROSTINI_REMOVER_H_

#include "chrome/browser/ash/crostini/crostini_manager.h"

namespace crostini {

class CrostiniRemover : public base::RefCountedThreadSafe<CrostiniRemover> {
 public:
  CrostiniRemover(Profile* profile,
                  std::string vm_name,
                  CrostiniManager::RemoveCrostiniCallback callback);

  CrostiniRemover(const CrostiniRemover&) = delete;
  CrostiniRemover& operator=(const CrostiniRemover&) = delete;

  void RemoveCrostini();

 private:
  friend class base::RefCountedThreadSafe<CrostiniRemover>;

  ~CrostiniRemover();

  void StopVmFinished(crostini::CrostiniResult result);
  void DestroyDiskImageFinished(bool success);
  void UninstallTerminaFinished(bool success);

  Profile* profile_;
  std::string vm_name_;
  CrostiniManager::RemoveCrostiniCallback callback_;
};

}  // namespace crostini

#endif  // CHROME_BROWSER_ASH_CROSTINI_CROSTINI_REMOVER_H_

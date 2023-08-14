// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_EARLY_PREFS_EARLY_PREFS_EXPORT_SERVICE_H_
#define CHROME_BROWSER_ASH_EARLY_PREFS_EARLY_PREFS_EXPORT_SERVICE_H_

#include <memory>

#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "chromeos/ash/components/early_prefs/early_prefs_writer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"

namespace ash {

class EarlyPrefsExportService : public KeyedService {
 public:
  EarlyPrefsExportService(const base::FilePath& root_dir,
                          PrefService* user_prefs);
  ~EarlyPrefsExportService() override;
  void Shutdown() override;

 private:
  void StoreAndTrackPref(const std::string& pref_name);
  void OnPrefChanged(const std::string& pref_name);

  raw_ptr<PrefService> prefs_;
  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;
  std::unique_ptr<EarlyPrefsWriter> writer_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_EARLY_PREFS_EARLY_PREFS_EXPORT_SERVICE_H_

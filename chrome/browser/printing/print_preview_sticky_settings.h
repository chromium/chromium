// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRINTING_PRINT_PREVIEW_STICKY_SETTINGS_H_
#define CHROME_BROWSER_PRINTING_PRINT_PREVIEW_STICKY_SETTINGS_H_

#include <optional>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "printing/print_job_constants.h"

class PrefService;

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace printing {

// Holds all the settings that should be remembered (sticky) in print preview.
// A sticky setting will be restored next time the user launches print preview.
// Only one instance of this class is instantiated.
class PrintPreviewStickySettings {
 public:
  static PrintPreviewStickySettings* GetInstance();

  PrintPreviewStickySettings();
  ~PrintPreviewStickySettings();

  const std::string* printer_app_state() const;

  // Stores app state for the last used printer.
  void StoreAppState(const std::string& app_state);

  void SaveInPrefs(PrefService* profile) const;
  void RestoreFromPrefs(PrefService* profile);

  // Parses serialized printing sticky settings state and extracts the list of
  // recently used printers. Returns a map with printers ids and their ranks.
  // The rank is the position in the list of recently used printers. The lower
  // the rank the more recent the printer was used.
  base::flat_map<std::string, int> GetPrinterRecentlyUsedRanks();

  // Parses serialized printing sticky settings state and extracts the list of
  // recently used printers. The list is ordered from most recently used to
  // least recently used.
  std::vector<std::string> GetRecentlyUsedPrinters();

  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

 private:
  std::optional<std::string> printer_app_state_;
};

}  // namespace printing

#endif  // CHROME_BROWSER_PRINTING_PRINT_PREVIEW_STICKY_SETTINGS_H_

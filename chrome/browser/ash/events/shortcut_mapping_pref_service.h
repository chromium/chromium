// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_EVENTS_SHORTCUT_MAPPING_PREF_SERVICE_H_
#define CHROME_BROWSER_ASH_EVENTS_SHORTCUT_MAPPING_PREF_SERVICE_H_

#include "base/memory/raw_ref.h"
#include "ui/base/shortcut_mapping_pref_delegate.h"

class PrefService;

namespace ash {

// TODO(crbug.com/40203434): Remove this class once kDeviceI18nShortcutsEnabled
// policy is deprecated.
class ShortcutMappingPrefService : public ui::ShortcutMappingPrefDelegate {
 public:
  explicit ShortcutMappingPrefService(PrefService& local_state);
  ShortcutMappingPrefService(const ShortcutMappingPrefService&) = delete;
  ShortcutMappingPrefService operator=(const ShortcutMappingPrefService&) =
      delete;
  ~ShortcutMappingPrefService() override;

  // ShortcutMappingPrefDelegate:
  bool IsDeviceEnterpriseManaged() const override;
  bool IsI18nShortcutPrefEnabled() const override;

 private:
  const raw_ref<PrefService> local_state_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_EVENTS_SHORTCUT_MAPPING_PREF_SERVICE_H_

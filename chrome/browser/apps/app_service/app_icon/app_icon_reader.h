// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_APP_ICON_APP_ICON_READER_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_APP_ICON_APP_ICON_READER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/services/app_service/public/cpp/icon_effects.h"
#include "components/services/app_service/public/cpp/icon_types.h"

class Profile;

namespace apps {

class AppIconDecoder;

// AppIconReader reads app icons and shortcut icons from the icon image files in
// the local disk and provides an ImageSkia for UI code to use.
class AppIconReader {
 public:
  explicit AppIconReader(Profile* profile);
  AppIconReader(const AppIconReader&) = delete;
  AppIconReader& operator=(const AppIconReader&) = delete;
  ~AppIconReader();

  // Reads specified app icons from the local disk for a app service item
  // identified by `id`. The id can be app_id for apps, and shortcut_id for
  // shortcuts.
  void ReadIcons(const std::string& id,
                 int32_t size_in_dip,
                 const IconKey& icon_key,
                 IconType icon_type,
                 LoadIconCallback callback);

 private:
  void OnUncompressedIconRead(int32_t size_in_dip,
                              IconEffects icon_effects,
                              IconType icon_type,
                              const std::string& id,
                              LoadIconCallback callback,
                              AppIconDecoder* decoder,
                              IconValuePtr iv);

  void OnCompleteWithIconValue(int32_t size_in_dip,
                               IconType icon_type,
                               LoadIconCallback callback,
                               IconValuePtr iv);

  void OnCompleteWithCompressedData(LoadIconCallback callback,
                                    std::vector<uint8_t> icon_data);

  const raw_ptr<Profile> profile_;

  // Contains pending image app icon decoders.
  std::vector<std::unique_ptr<AppIconDecoder>> decodes_;

  base::WeakPtrFactory<AppIconReader> weak_ptr_factory_{this};
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_APP_ICON_APP_ICON_READER_H_

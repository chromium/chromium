// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_APP_ICON_APP_ICON_WRITER_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_APP_ICON_APP_ICON_WRITER_H_

#include <map>
#include <set>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/services/app_service/public/cpp/icon_types.h"
#include "ui/base/resource/resource_scale_factor.h"

class Profile;

namespace apps {

class CompressedIconGetter;

// AppIconWriter writes app icons and shortcut icons to the icon image files in
// the local disk.
class AppIconWriter {
 public:
  explicit AppIconWriter(Profile* profile);
  AppIconWriter(const AppIconWriter&) = delete;
  AppIconWriter& operator=(const AppIconWriter&) = delete;
  ~AppIconWriter();

  // Calls `compressed_icon_getter`'s GetCompressedIconData to get the
  // compressed icon data for `id`, and saves the icon data to the local disk.
  // Calls `callback` with true if saving and loading the data was successful.
  void InstallIcon(CompressedIconGetter* compressed_icon_getter,
                   const std::string& id,
                   int32_t size_in_dip,
                   base::OnceCallback<void(bool)> callback);

 private:
  // Key contains the arguments of InstallIcon. It implements operator<, so that
  // it can be the "K" in a "map<K, V>".
  class Key {
   public:
    Key(const std::string& id, int32_t size_in_dip);

    Key(const Key&) = delete;
    Key& operator=(const Key&) = delete;

    ~Key();

    Key(Key&&) = default;
    Key& operator=(Key&&) = default;

    bool operator<(const Key& other) const;

    std::string id_;
    int32_t size_in_dip_;
  };

  // Contains the scale factors and the callback for the compressed app icon
  // data requests.
  struct PendingResult {
    PendingResult();

    PendingResult(const PendingResult&) = delete;
    PendingResult& operator=(const PendingResult&) = delete;

    ~PendingResult();

    PendingResult(PendingResult&&);
    PendingResult& operator=(PendingResult&&);

    // The requested scale factors for the icon requests with `id` and
    // `size_in_dip`.
    std::set<ui::ResourceScaleFactor> scale_factors;

    // The finished icon requested for scale factors. E.g. the icon data for the
    // scale factor(k100Percent) has been fetched, and saved in the icon file,
    // and the icon data for the scale factor(k200Percent) has not been fetched.
    // Then we have:
    // scale_factors = {k100Percent, k200Percent}
    // complete_scale_factors = {k100Percent}
    std::set<ui::ResourceScaleFactor> complete_scale_factors;

    // The callbacks for the icon requests with `id` and `size_in_dip`.
    std::vector<base::OnceCallback<void(bool)>> callbacks;
  };

  // Saves the compressed icon data in `iv` to the local disk.
  void OnIconLoad(const std::string& id,
                  int32_t size_in_dip,
                  ui::ResourceScaleFactor scale_factor,
                  IconValuePtr iv);

  void OnWriteIconFile(const std::string& id,
                       int32_t size_in_dip,
                       ui::ResourceScaleFactor scale_factor,
                       bool ret);

  const raw_ptr<Profile> profile_;

  // The map from the app id to PendingResult, which contains pending app icon
  // requests.
  std::map<Key, PendingResult> pending_results_;

  base::WeakPtrFactory<AppIconWriter> weak_ptr_factory_{this};
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_APP_ICON_APP_ICON_WRITER_H_

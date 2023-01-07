// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_FILE_SYSTEM_VOLUME_LIST_PROVIDER_LACROS_H_
#define CHROME_BROWSER_EXTENSIONS_API_FILE_SYSTEM_VOLUME_LIST_PROVIDER_LACROS_H_

#include <vector>

#include "base/callback.h"
#include "base/memory/raw_ptr.h"
#include "chromeos/crosapi/mojom/volume_manager.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"

class Profile;

namespace extensions {

// Class to monitor volume list change from Ash, and dispatch the result to
// extensions that listens to chrome.fileSystem.onVolumeListChange.
class VolumeListProviderLacros : public crosapi::mojom::VolumeListObserver {
 public:
  using Listener =
      base::RepeatingCallback<void(std::vector<crosapi::mojom::VolumePtr>)>;

  explicit VolumeListProviderLacros(Profile* profile);
  VolumeListProviderLacros(const VolumeListProviderLacros&) = delete;
  VolumeListProviderLacros& operator=(const VolumeListProviderLacros&) = delete;
  ~VolumeListProviderLacros() override;

  void Start();

 private:
  // crosapi::mojom::VolumeListObserver:
  void OnVolumeListChanged(
      std::vector<crosapi::mojom::VolumePtr> volume_list) override;

  // Pointer to owner, so okay to keep as raw pointer.
  base::raw_ptr<Profile> profile_;

  // Receives mojo messages from ash-chrome.
  mojo::Receiver<crosapi::mojom::VolumeListObserver> receiver_{this};
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_FILE_SYSTEM_VOLUME_LIST_PROVIDER_LACROS_H_

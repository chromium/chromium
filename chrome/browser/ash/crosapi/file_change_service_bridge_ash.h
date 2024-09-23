// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_FILE_CHANGE_SERVICE_BRIDGE_ASH_H_
#define CHROME_BROWSER_ASH_CROSAPI_FILE_CHANGE_SERVICE_BRIDGE_ASH_H_

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/profiles/profile_observer.h"
#include "chromeos/crosapi/mojom/file_change_service_bridge.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

class Profile;

namespace crosapi {

// The bridge implemented in Ash which is connected to the
// `FileChangeServiceBridge` in Lacros via crosapi. This bridge enables file
// change events originating from Lacros to be propagated to the
// `FileChangeService`, and its observers, in Ash.
class FileChangeServiceBridgeAsh : public mojom::FileChangeServiceBridge,
                                   public ProfileObserver {
 public:
  explicit FileChangeServiceBridgeAsh(Profile* profile);
  FileChangeServiceBridgeAsh(const FileChangeServiceBridgeAsh&) = delete;
  FileChangeServiceBridgeAsh& operator=(const FileChangeServiceBridgeAsh&) =
      delete;
  ~FileChangeServiceBridgeAsh() override;

  // Binds the specified `receiver` to `this` for use by crosapi.
  void BindReceiver(
      mojo::PendingReceiver<mojom::FileChangeServiceBridge> receiver);

  // ProfileObserver:
  void OnProfileWillBeDestroyed(Profile* profile) override;

 private:
  // mojom::FileChangeServiceBridge:
  void OnFileCreatedFromShowSaveFilePicker(
      const GURL& file_picker_binding_context,
      const base::FilePath& file_path) override;

  // The Ash profile associated with the `FileChangeService` for which this
  // bridge exists.
  raw_ptr<Profile> profile_;

  base::ScopedObservation<Profile, ProfileObserver> profile_observation_{this};

  // The set of receivers bound to `this` for use by crosapi.
  mojo::ReceiverSet<mojom::FileChangeServiceBridge> receivers_;
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_FILE_CHANGE_SERVICE_BRIDGE_ASH_H_

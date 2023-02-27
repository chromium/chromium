// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SMART_READER_SMART_READER_MANAGER_ASH_H_
#define CHROME_BROWSER_ASH_SMART_READER_SMART_READER_MANAGER_ASH_H_

#include "base/functional/callback.h"
#include "chromeos/crosapi/mojom/smart_reader.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace ash {

class SmartReaderManagerAsh {
 public:
  SmartReaderManagerAsh();

  SmartReaderManagerAsh(const SmartReaderManagerAsh&) = delete;
  SmartReaderManagerAsh& operator=(const SmartReaderManagerAsh&) = delete;

  ~SmartReaderManagerAsh();

  // Binds a pending remote connected to a lacros mojo client to the manager.
  void BindRemote(
      mojo::PendingRemote<crosapi::mojom::SmartReaderClient> remote);

 private:
  mojo::Remote<crosapi::mojom::SmartReaderClient> smart_reader_client_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_SMART_READER_SMART_READER_MANAGER_ASH_H_

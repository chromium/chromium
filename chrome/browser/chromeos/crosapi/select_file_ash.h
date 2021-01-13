// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROSAPI_SELECT_FILE_ASH_H_
#define CHROME_BROWSER_CHROMEOS_CROSAPI_SELECT_FILE_ASH_H_

#include "chromeos/crosapi/mojom/select_file.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace crosapi {

// Implements the SelectFile mojo interface for open/save dialogs. Wraps the
// underlying Chrome OS SelectFileExtension implementation, which uses the WebUI
// file manager to provide the dialogs. Lives on the UI thread.
class SelectFileAsh : public mojom::SelectFile {
 public:
  explicit SelectFileAsh(mojo::PendingReceiver<mojom::SelectFile> receiver);
  SelectFileAsh(const SelectFileAsh&) = delete;
  SelectFileAsh& operator=(const SelectFileAsh&) = delete;
  ~SelectFileAsh() override;

  // crosapi::mojom::SelectFile:
  void Select(mojom::SelectFileOptionsPtr options,
              SelectCallback callback) override;

 private:
  mojo::Receiver<mojom::SelectFile> receiver_;
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_CHROMEOS_CROSAPI_SELECT_FILE_ASH_H_

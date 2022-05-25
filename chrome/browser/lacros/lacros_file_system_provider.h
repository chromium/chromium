// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LACROS_LACROS_FILE_SYSTEM_PROVIDER_H_
#define CHROME_BROWSER_LACROS_LACROS_FILE_SYSTEM_PROVIDER_H_

#include "chromeos/crosapi/mojom/file_system_provider.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"

// This class is responsible for receiving file system provider events from Ash.
// These are forwarded to the corresponding main profile file system provider
// extension.
class LacrosFileSystemProvider : public crosapi::mojom::FileSystemProvider {
 public:
  LacrosFileSystemProvider();
  ~LacrosFileSystemProvider() override;
  LacrosFileSystemProvider(const LacrosFileSystemProvider&) = delete;
  LacrosFileSystemProvider& operator=(const LacrosFileSystemProvider&) = delete;

  // crosapi::mojom::FileSystemProvider
  void ForwardOperation(const std::string& provider,
                        int32_t histogram_value,
                        const std::string& event_name,
                        std::vector<base::Value> args) override;

 private:
  // Mojo endpoint that's responsible for receiving messages from Ash.
  mojo::Receiver<crosapi::mojom::FileSystemProvider> receiver_;

  base::WeakPtrFactory<LacrosFileSystemProvider> weak_factory_{this};
};

#endif  // CHROME_BROWSER_LACROS_LACROS_FILE_SYSTEM_PROVIDER_H_

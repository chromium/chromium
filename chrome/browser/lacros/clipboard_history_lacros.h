// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LACROS_CLIPBOARD_HISTORY_LACROS_H_
#define CHROME_BROWSER_LACROS_CLIPBOARD_HISTORY_LACROS_H_

#include <vector>

#include "chromeos/crosapi/mojom/clipboard_history.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"

class ClipboardHistoryRefreshLacrosTest;

namespace crosapi {

// The Lacros implementation of `mojom::ClipboardHistoryClient`. A singleton
// that caches the clipboard history item descriptors received from Ash. Created
// only if the clipboard history refresh feature is enabled.
class ClipboardHistoryLacros : public mojom::ClipboardHistoryClient {
 public:
  explicit ClipboardHistoryLacros(mojom::ClipboardHistory* remote);
  ClipboardHistoryLacros(const ClipboardHistoryLacros&) = delete;
  ClipboardHistoryLacros& operator=(const ClipboardHistoryLacros&) = delete;
  ~ClipboardHistoryLacros() override;

  static ClipboardHistoryLacros* Get();

  const std::vector<mojom::ClipboardHistoryItemDescriptor>& cached_descriptors()
      const {
    return cached_descriptors_;
  }

 private:
  friend ClipboardHistoryRefreshLacrosTest;

  // mojom::ClipboardHistoryClient:
  void SetClipboardHistoryItemDescriptors(
      std::vector<mojom::ClipboardHistoryItemDescriptorPtr> descriptor_ptrs)
      override;

  // Called when the communication channel with Ash is disconnected.
  void OnDisconnected();

  // The cached clipboard history item descriptors. Updated by Ash.
  std::vector<mojom::ClipboardHistoryItemDescriptor> cached_descriptors_;

  // Mojo endpoint that's responsible for receiving messages from Ash.
  mojo::Receiver<mojom::ClipboardHistoryClient> receiver_;
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_LACROS_CLIPBOARD_HISTORY_LACROS_H_

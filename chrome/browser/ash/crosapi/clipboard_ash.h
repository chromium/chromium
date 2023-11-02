// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_CLIPBOARD_ASH_H_
#define CHROME_BROWSER_ASH_CROSAPI_CLIPBOARD_ASH_H_

#include "base/memory/weak_ptr.h"
#include "chromeos/crosapi/mojom/clipboard.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace crosapi {

// Implements the crosapi interface for clipboard. Lives in Ash-Chrome on the UI
// thread.
//
// This class provides minimal access to ui::Clipboard. It exists as a
// short-term workaround to the problem that wayland/ozone clipboard access is
// not fully stable. See https://crbug.com/913422 and https://crbug.com/1155662
// for details.
class ClipboardAsh : public mojom::Clipboard {
 public:
  ClipboardAsh();
  ClipboardAsh(const ClipboardAsh&) = delete;
  ClipboardAsh& operator=(const ClipboardAsh&) = delete;
  ~ClipboardAsh() override;

  void BindReceiver(mojo::PendingReceiver<mojom::Clipboard> receiver);

  // crosapi::mojom::Clipboard:
  void GetCopyPasteText(GetCopyPasteTextCallback callback) override;

 private:
  // This class supports any number of connections. This allows the client to
  // have multiple, potentially thread-affine, remotes.
  mojo::ReceiverSet<mojom::Clipboard> receivers_;

  base::WeakPtrFactory<ClipboardAsh> weak_factory_{this};
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_CLIPBOARD_ASH_H_

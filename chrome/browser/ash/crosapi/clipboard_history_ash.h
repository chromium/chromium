// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_CLIPBOARD_HISTORY_ASH_H_
#define CHROME_BROWSER_ASH_CROSAPI_CLIPBOARD_HISTORY_ASH_H_

#include "chromeos/crosapi/mojom/clipboard_history.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace gfx {
class Rect;
}  // namespace gfx

namespace crosapi {

class ClipboardHistoryAsh : public mojom::ClipboardHistory {
 public:
  ClipboardHistoryAsh();
  ClipboardHistoryAsh(const ClipboardHistoryAsh&) = delete;
  ClipboardHistoryAsh& operator=(const ClipboardHistoryAsh&) = delete;
  ~ClipboardHistoryAsh() override;

  void BindReceiver(mojo::PendingReceiver<mojom::ClipboardHistory> receiver);

  // crosapi::mojom::ClipboardHistory:
  void ShowClipboard(
      const gfx::Rect& anchor_point,
      ui::MenuSourceType menu_source_type,
      mojom::ClipboardHistoryControllerShowSource show_source) override;

 private:
  // This class supports any number of connections. This allows the client to
  // have multiple, potentially thread-affine, remotes.
  mojo::ReceiverSet<mojom::ClipboardHistory> receivers_;
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_CLIPBOARD_HISTORY_ASH_H_

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/annotator/test/mock_untrusted_annotator_page.h"

namespace ash {
MockUntrustedAnnotatorPage::MockUntrustedAnnotatorPage() = default;
MockUntrustedAnnotatorPage::~MockUntrustedAnnotatorPage() = default;
void MockUntrustedAnnotatorPage::FlushReceiverForTesting() {
  receiver_.FlushForTesting();
}

void MockUntrustedAnnotatorPage::FlushRemoteForTesting() {
  remote_.FlushForTesting();
}

void MockUntrustedAnnotatorPage::SendUndoRedoAvailableChanged(
    bool undo_available,
    bool redo_available) {
  remote_->OnUndoRedoAvailabilityChanged(undo_available, redo_available);
}

void MockUntrustedAnnotatorPage::SendCanvasInitialized(bool success) {
  remote_->OnCanvasInitialized(success);
}

mojo::Receiver<annotator::mojom::UntrustedAnnotatorPage>&
MockUntrustedAnnotatorPage::receiver() {
  return receiver_;
}
mojo::Remote<annotator::mojom::UntrustedAnnotatorPageHandler>&
MockUntrustedAnnotatorPage::remote() {
  return remote_;
}
}  // namespace ash

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_ANNOTATOR_TEST_MOCK_UNTRUSTED_ANNOTATOR_PAGE_H_
#define ASH_WEBUI_ANNOTATOR_TEST_MOCK_UNTRUSTED_ANNOTATOR_PAGE_H_

#include "ash/webui/annotator/mojom/untrusted_annotator.mojom.h"
#include "ash/webui/annotator/public/mojom/annotator_structs.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ash {
// MOCK the annotator instance in the WebUI renderer.
class MockUntrustedAnnotatorPage
    : public annotator::mojom::UntrustedAnnotatorPage {
 public:
  MockUntrustedAnnotatorPage();
  MockUntrustedAnnotatorPage(const MockUntrustedAnnotatorPage&) = delete;
  MockUntrustedAnnotatorPage& operator=(const MockUntrustedAnnotatorPage&) =
      delete;
  ~MockUntrustedAnnotatorPage() override;

  MOCK_METHOD0(Clear, void());
  MOCK_METHOD0(Undo, void());
  MOCK_METHOD0(Redo, void());
  MOCK_METHOD1(SetTool, void(annotator::mojom::AnnotatorToolPtr tool));

  void FlushReceiverForTesting();

  void FlushRemoteForTesting();

  void SendUndoRedoAvailableChanged(bool undo_available, bool redo_available);

  void SendCanvasInitialized(bool success);

  mojo::Receiver<annotator::mojom::UntrustedAnnotatorPage>& receiver();
  mojo::Remote<annotator::mojom::UntrustedAnnotatorPageHandler>& remote();

 private:
  mojo::Receiver<annotator::mojom::UntrustedAnnotatorPage> receiver_{this};
  mojo::Remote<annotator::mojom::UntrustedAnnotatorPageHandler> remote_;
};

}  // namespace ash

#endif  // ASH_WEBUI_ANNOTATOR_TEST_MOCK_UNTRUSTED_ANNOTATOR_PAGE_H_

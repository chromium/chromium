// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {FakePrintPreviewPageHandler} from '../fakes/fake_print_preview_page_handler.js';
import type {FakeGeneratePreviewObserver, PreviewTicket, PrintPreviewPageHandlerCompositeInterface, PrintRequestOutcome, PrintTicket, SessionContext} from '../utils/print_preview_cros_app_types.js';

/**
 * @fileoverview
 * 'print_preview_page_handler_composite' provides a temporary structure to
 * support the mojo implementation of the PrintPreviewPageHandler mojom
 * interface combined with fake implementations until all methods can be mojo
 * implemented.
 */

export class PrintPreviewPageHandlerComposite implements
    PrintPreviewPageHandlerCompositeInterface {
  readonly fakePageHandler = new FakePrintPreviewPageHandler();

  startSession(dialogArgs: string): Promise<{sessionContext: SessionContext}> {
    return this.fakePageHandler.startSession(dialogArgs);
  }

  print(ticket: PrintTicket):
      Promise<{printRequestOutcome: PrintRequestOutcome}> {
    return this.fakePageHandler.print(ticket);
  }

  cancel(): void {
    return this.fakePageHandler.cancel();
  }

  generatePreview(previewTicket: PreviewTicket): Promise<void> {
    return this.fakePageHandler.generatePreview(previewTicket);
  }

  observePreviewReady(observer: FakeGeneratePreviewObserver): Promise<void> {
    return this.fakePageHandler.observePreviewReady(observer);
  }
}

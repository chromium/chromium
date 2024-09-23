// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.js';
import {EventTracker} from 'chrome://resources/js/event_tracker.js';

import {getFakePreviewTicket} from '../fakes/fake_data.js';
import {createCustomEvent} from '../utils/event_utils.js';
import {getPrintPreviewPageHandler} from '../utils/mojo_data_providers.js';
import {FakeGeneratePreviewObserver, type PrintPreviewPageHandlerCompositeInterface, SessionContext} from '../utils/print_preview_cros_app_types.js';

/**
 * @fileoverview
 * 'preview_ticket_manager' responsible for tracking the active preview ticket
 * and signaling updates to subscribed listeners.
 */

export const PREVIEW_REQUEST_STARTED_EVENT =
    'preview-ticket-manager.preview-request-started';
export const PREVIEW_REQUEST_FINISHED_EVENT =
    'preview-ticket-manager.preview-request-finished';
export const PREVIEW_TICKET_MANAGER_SESSION_INITIALIZED =
    'preview-ticket-manager.session-initialized';

export class PreviewTicketManager extends EventTarget implements
    FakeGeneratePreviewObserver {
  private static instance: PreviewTicketManager|null = null;

  static getInstance(): PreviewTicketManager {
    if (PreviewTicketManager.instance === null) {
      PreviewTicketManager.instance = new PreviewTicketManager();
    }

    return PreviewTicketManager.instance;
  }

  static resetInstanceForTesting(): void {
    PreviewTicketManager.instance = null;
  }

  // Non-static properties:
  private printPreviewPageHandler: PrintPreviewPageHandlerCompositeInterface|
      null;
  private previewLoaded = false;
  private sessionContext: SessionContext;
  private eventTracker = new EventTracker();
  // Represents the request id for the latest preview request. All responses
  // for ids below this will be ignored.
  private activeRequestId = 0;

  // Prevent additional initialization.
  private constructor() {
    super();

    // Setup mojo data providers.
    this.printPreviewPageHandler = getPrintPreviewPageHandler();
    this.printPreviewPageHandler.observePreviewReady(this);
  }

  // `initializeSession` is only intended to be called once from the
  // `PrintPreviewCrosAppController`.
  initializeSession(sessionContext: SessionContext): void {
    assert(
        !this.sessionContext, 'SessionContext should only be configured once');
    this.sessionContext = sessionContext;

    this.dispatchEvent(
        createCustomEvent(PREVIEW_TICKET_MANAGER_SESSION_INITIALIZED));

    this.sendPreviewRequest();
  }

  isPreviewLoaded(): boolean {
    return this.previewLoaded;
  }

  // Send a request to generate a preview PDF with the desired print settings.
  // TODO(b/323421684): Rely on an observer to determine when the request is
  // finished.
  sendPreviewRequest(): void {
    ++this.activeRequestId;
    this.previewLoaded = false;
    this.dispatchEvent(createCustomEvent(PREVIEW_REQUEST_STARTED_EVENT));

    // TODO(b/323421684): Replace with actual preview settings.
    this.printPreviewPageHandler!.generatePreview(
        getFakePreviewTicket(this.activeRequestId));
  }

  // FakeGeneratePreviewObserver:
  onDocumentReady(previewRequestId: number): void {
    // Only acknowledge responses for the latest preview request.
    if (previewRequestId !== this.activeRequestId) {
      return;
    }

    this.previewLoaded = true;
    this.dispatchEvent(createCustomEvent(PREVIEW_REQUEST_FINISHED_EVENT));
  }

  // Returns true only after the `initializeSession` function has been called
  // with a valid `SessionContext`.
  isSessionInitialized(): boolean {
    return !!this.sessionContext;
  }
}

declare global {
  interface HTMLElementEventMap {
    [PREVIEW_REQUEST_FINISHED_EVENT]: CustomEvent<void>;
    [PREVIEW_REQUEST_STARTED_EVENT]: CustomEvent<void>;
  }
}

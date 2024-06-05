// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CapabilitiesManager} from './data/capabilities_manager.js';
import {DestinationManager} from './data/destination_manager.js';
import {PreviewTicketManager} from './data/preview_ticket_manager.js';
import {PrintTicketManager} from './data/print_ticket_manager.js';
import {getPrintPreviewPageHandler} from './utils/mojo_data_providers.js';
import {SessionContext} from './utils/print_preview_cros_app_types.js';

/**
 * @fileoverview
 * 'print-preview-cros-app-controller' is responsible for starting the print
 * preview session and providing the session context to the data managers.
 * Session context provides the unique ID necessary for communicating with
 * the CrOS preview backend and required initialization information.
 */

// WebUI property key used to store dialog args.
export const DIALOG_ARG_PROPERTY_KEY = 'dialogArguments';

export class PrintPreviewCrosAppController extends EventTarget {
  private printPreviewPageHandler = getPrintPreviewPageHandler();
  private sessionContext: SessionContext;
  private capabilitiesManager = CapabilitiesManager.getInstance();
  private destinationManager = DestinationManager.getInstance();
  private previewTicketManager = PreviewTicketManager.getInstance();
  private printTicketManager = PrintTicketManager.getInstance();
  private dialogArgs: string;

  constructor() {
    super();

    this.dialogArgs = chrome.getVariableValue(DIALOG_ARG_PROPERTY_KEY);
    this.printPreviewPageHandler.startSession(this.dialogArgs)
        .then((response: {sessionContext: SessionContext}): void => {
          this.sessionContext = response.sessionContext;
          this.capabilitiesManager.initializeSession(this.sessionContext);
          this.destinationManager.initializeSession(this.sessionContext);
          this.previewTicketManager.initializeSession(this.sessionContext);
          this.printTicketManager.initializeSession(this.sessionContext);
        });
  }

  getDialogArgsForTesting(): string {
    return this.dialogArgs;
  }
}

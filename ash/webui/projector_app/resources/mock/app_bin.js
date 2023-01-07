// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

/*
 * Mock ProjectorApp class for testing.
 * @implements {projectorApp.AppApi}
 */
class ProjectorApp extends PolymerElement {
  static get is() {
    return 'projector-app';
  }
  static get properties() {
    return {clientDelegate_: Object};
  }

  // Implements AppApi:
  onNewScreencastPreconditionChanged(state) {}

  onScreencastsStateChange(screencasts) {}

  setClientDelegate(clientDelegate) {
    this.clientDelegate_ = clientDelegate;
  }

  getClientDelegateForTesting() {
    return this.clientDelegate_;
  }

  onSodaInstallProgressUpdated(progress) {}

  onSodaInstalled() {}

  onSodaInstallError() {}
}

customElements.define(ProjectorApp.is, ProjectorApp);
const projectorApp = document.createElement('projector-app');
projectorApp.textContent = `Please build branded Chrome (set is_chrome_branded =
true) for the official Projector app.`;
document.body.appendChild(projectorApp);

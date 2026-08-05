// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './install_tab.css.js';

/**
 * Abstract base class for all tab sub-components in the IWA installation
 * dialog.
 */
export abstract class IwaDevInstallTabElement extends CrLitElement {
  static override get styles() {
    return getCss();
  }

  /**
   * Notifies parent component of the tab's current validity state.
   */
  protected notifyValidChanged() {
    this.fire('valid-changed', {isValid: this.isValid()});
  }

  /**
   * Returns true if the form inputs within the tab are currently valid for
   * submission.
   */
  abstract isValid(): boolean;

  /**
   * Validates and submits the tab form, firing an install request event if
   * valid.
   */
  abstract submit(): void;
}

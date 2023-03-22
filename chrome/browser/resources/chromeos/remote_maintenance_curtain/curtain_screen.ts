// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import '//resources/polymer/v3_0/iron-media-query/iron-media-query.js';
import './components/common_styles/oobe_dialog_host_styles.css.js';
import './components/dialogs/oobe_adaptive_dialog.js';
import './components/oobe_icons.html.js';
import './components/common_styles/oobe_common_styles.css.js';

import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './curtain_screen.html.js';

function setDialogSizeAndOrientation() {
  document.documentElement.style.setProperty(
      '--oobe-oobe-dialog-height-base', window.innerHeight + 'px');
  document.documentElement.style.setProperty(
      '--oobe-oobe-dialog-width-base', window.innerWidth + 'px');

  // Screen orientation value needs to be kept up-to-date.
  document.documentElement.setAttribute(
      'orientation',
      window.innerWidth > window.innerHeight ? 'horizontal' : 'vertical');
}

// TODO(b/274059668): Remove when OOBE is migrated to TS.
type OobeAdaptiveDialog = HTMLElement&{
  onBeforeShow(): void,
};

// Inform the compiler that the`this.$.mainCurtainDialog` statement below
// returns an element of type `OobeAdaptiveDialog`.
interface CurtainScreenElement {
  $: {
    mainCurtainDialog: OobeAdaptiveDialog,
  };
}

class CurtainScreenElement extends PolymerElement {
  static get is() {
    return 'curtain-screen' as const;
  }

  static get template() {
    return getTemplate();
  }

  override ready() {
    super.ready();
    this.$.mainCurtainDialog.onBeforeShow();
    setDialogSizeAndOrientation();
  }

  override connectedCallback() {
    super.connectedCallback();
    window.addEventListener('resize', setDialogSizeAndOrientation);
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    window.removeEventListener('resize', setDialogSizeAndOrientation);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [CurtainScreenElement.is]: CurtainScreenElement;
  }
}

customElements.define(CurtainScreenElement.is, CurtainScreenElement);

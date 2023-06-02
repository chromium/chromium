// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/cr_checkbox/cr_checkbox.js';
import '../demo.css.js';

import {getInstance as getAnnouncerInstance} from '//resources/cr_elements/cr_a11y_announcer/cr_a11y_announcer.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './cr_a11y_announcer_demo.html.js';

interface CrA11yAnnouncerDemoElement {
  $: {
    announcerContainer: HTMLElement,
  };
}

class CrA11yAnnouncerDemoElement extends PolymerElement {
  static get is() {
    return 'cr-a11y-announcer-demo';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      forceShowAnnouncer_: {
        type: Boolean,
        reflectToAttribute: true,
        value: false,
      },
    };
  }

  private announcementTextCount_: number = 0;
  private forceShowAnnouncer_: boolean;

  private onAnnounceTextClick_() {
    const announcer = this.forceShowAnnouncer_ ?
        getAnnouncerInstance(this.$.announcerContainer) :
        getAnnouncerInstance();
    announcer.announce(`Announcement number ${++this.announcementTextCount_}`);
  }

  private onAnnounceMultipleTextsClick_() {
    const announcer = this.forceShowAnnouncer_ ?
        getAnnouncerInstance(this.$.announcerContainer) :
        getAnnouncerInstance();
    announcer.announce('Page is loading...');
    announcer.announce('Page has loaded.');
  }
}

export const tagName = CrA11yAnnouncerDemoElement.is;

customElements.define(
    CrA11yAnnouncerDemoElement.is, CrA11yAnnouncerDemoElement);

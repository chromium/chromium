// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './strings.m.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {getTemplate} from './profile_internals_app.html.js';

export class ProfileInternalsAppElement extends PolymerElement {
  static get is() {
    return 'profile-internals-app';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      message_: {
        type: String,
        value: () => loadTimeData.getString('message'),
      },
    };
  }

  private message_: string;
}

customElements.define(
    ProfileInternalsAppElement.is, ProfileInternalsAppElement);
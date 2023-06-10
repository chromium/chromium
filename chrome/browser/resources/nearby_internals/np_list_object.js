// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './np_list_object.html.js';
import {PresenceDevice} from './types.js';

Polymer({
  is: 'np-object',

  _template: getTemplate(),

  properties: {
    /**
     * Type: {!PresenceDevice}
     */
    PresenceDevice: {
      type: Object,
    },
  },

});

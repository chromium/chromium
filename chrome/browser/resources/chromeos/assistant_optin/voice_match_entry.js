// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/* #js_imports_placeholder */

/**
 * @constructor
 * @extends {PolymerElement}
 */
 const VoiceMatchEntryBase = Polymer.mixinBehaviors(
  [OobeI18nBehavior],
  Polymer.Element);

/**
 * @polymer
 */
class VoiceMatchEntry extends VoiceMatchEntryBase {
  static get is() {
    return 'voice-match-entry';
  }

  /* #html_template_placeholder */

  static get properties() {
    return {
      active: {
        type: Boolean,
        value: false,
      },

      completed: {
        type: Boolean,
        value: false,
      },
    };
  }
}

customElements.define(VoiceMatchEntry.is, VoiceMatchEntry);

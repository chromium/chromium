// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'gaia-input-form',

  properties: {
    disabled: {
      type: Boolean,
      observer: 'onDisabledChanged_',
    },

    buttonText: {
      type: String,
      value: '',
    }
  },

  /** @public */
  reset: function() {
    var inputs = this.getInputs_();
    for (var i = 0; i < inputs.length; ++i) {
      inputs[i].value = '';
      inputs[i].isInvalid = false;
    }
  },

  submit: function() {
    this.fire('submit');
  },

  /** @private */
  onButtonClicked_: function() {
    this.submit();
  },

  /**
   * @private
   * @return {!Array<!Node>}
   */
  getInputs_: function() {
    return Polymer.dom(this.$.inputs).getDistributedNodes();
  },

  /** @private */
  onKeyDown_: function(e) {
    if (e.keyCode != 13 || this.$.button.disabled)
      return;
    if (this.getInputs_().indexOf(e.target) == -1)
      return;
    this.onButtonClicked_();
  },

  /**
   * @private
   * @return {!Array<!Element>}
   */
  getControls_: function() {
    var controls = this.getInputs_();
    controls.push(this.$.button);
    return controls.concat(Polymer.dom(this).querySelectorAll('gaia-button'));
  },

  /** @private */
  onDisabledChanged_: function(disabled) {
    this.getControls_().forEach(function(control) {
      control.disabled = disabled;
    });
  }
});

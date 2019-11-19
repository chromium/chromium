// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

{
  const INPUT_EMAIL_PATTERN = '^[a-zA-Z0-9.!#$%&\'*+=?^_`{|}~-]+(@[^\\s@]+)?$';

  Polymer({
    is: 'gaia-input',

    properties: {
      value: {notify: true, observer: 'updateDomainVisibility_', type: String},

      type: {observer: 'typeChanged_', type: String},

      domain: {observer: 'updateDomainVisibility_', type: String},

      disabled: Boolean,

      required: Boolean,

      isInvalid: {type: Boolean, notify: true},

      pattern: String
    },

    /** @override */
    attached: function() {
      this.typeChanged_();
    },

    onKeyDown: function(e) {
      this.isInvalid = false;
    },

    /** @private */
    updateDomainVisibility_: function() {
      this.$.domainLabel.hidden = (this.type !== 'email') || !this.domain ||
          (this.value && this.value.indexOf('@') !== -1);
    },

    onTap: function() {
      this.isInvalid = false;
    },

    focus: function() {
      this.$.input.focus();
    },

    /** @return {!boolean} */
    checkValidity: function() {
      var valid = this.$.ironInput.validate();
      this.isInvalid = !valid;
      return valid;
    },

    /** @private */
    typeChanged_: function() {
      if (this.type == 'email') {
        this.$.input.pattern = INPUT_EMAIL_PATTERN;
        this.$.input.type = 'text';
      } else {
        this.$.input.type = this.type;
        if (this.pattern) {
          this.$.input.pattern = this.pattern;
        } else {
          this.$.input.removeAttribute('pattern');
        }
      }
      this.updateDomainVisibility_();
    },
  });
}

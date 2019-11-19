// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// <include src="html-echo.js">

Polymer({
  is: 'saml-interstitial',

  properties: {
    /** @type {Element} */
    changeAccountLink: {
      type: HTMLElement,
    },

    domain: {type: String, observer: 'onDomainChanged_'},

    showDomainMessages_: {type: Boolean, value: false}
  },
  ready: function() {
    this.changeAccountLink = this.$.changeAccountLink;
  },
  submit: function() {
    this.$.samlInterstitialForm.submit();
  },
  onDomainChanged_: function() {
    this.$.managedBy.textContent =
        loadTimeData.getStringF('enterpriseInfoMessage', this.domain);
    this.$.message.content =
        loadTimeData.getStringF('samlInterstitialMessage', this.domain);
    this.showDomainMessages_ = !!this.domain.length;
  },
  onSamlPageNextClicked_: function() {
    this.fire('samlPageNextClicked');
  },
  onSamlPageChangeAccountClicked_: function() {
    this.fire('samlPageChangeAccountClicked');
  }
});

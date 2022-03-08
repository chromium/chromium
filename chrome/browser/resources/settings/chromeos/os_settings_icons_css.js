// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import '//resources/cr_elements/shared_vars_css.m.js';

const template = document.createElement('template');
template.innerHTML = `
<dom-module id="os-settings-icons" assetpath="chrome://resources/">
  {__html_template__}
</dom-module>
`;
document.body.appendChild(template.content.cloneNode(true));

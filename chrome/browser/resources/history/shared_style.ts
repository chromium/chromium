// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './shared_vars.js';
import 'chrome://resources/cr_elements/hidden_style_css.m.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';

const template = document.createElement('template');
template.innerHTML = `
<dom-module id="shared-style">{__html_template__}</dom-module>
`;
document.body.appendChild(template.content.cloneNode(true));

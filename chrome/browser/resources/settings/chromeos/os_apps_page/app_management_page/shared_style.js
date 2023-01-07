// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/chromeos/cros_color_overrides.css.js';
import 'chrome://resources/cr_components/app_management/app_management_shared_style.css.js';

const template = document.createElement('template');
template.innerHTML = `
<dom-module id="app-management-cros-shared-css" assetpath="chrome://resources/">
{__html_template__}</dom-module>
`;
document.body.appendChild(template.content.cloneNode(true));

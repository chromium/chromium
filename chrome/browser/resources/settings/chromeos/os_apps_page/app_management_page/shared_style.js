// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/chromeos/cros_color_overrides.m.js';
import '//resources/cr_components/app_management/shared_style.js';

const template = document.createElement('template');
template.innerHTML = `
<dom-module id="app-management-cros-shared-css" assetpath="chrome://resources/">
{__html_template__}</dom-module>
`;
document.body.appendChild(template.content.cloneNode(true));

// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const $_documentContainer = document.createElement('template');
$_documentContainer.innerHTML = `{__html_template__}`;
document.head.appendChild($_documentContainer.content);

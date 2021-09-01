// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import {AppManagementUserAction} from './constants.m.js';
// #import {recordAppManagementUserAction} from './util.m.js';
// clang-format on

Polymer({
  is: 'app-management-more-permissions-item',

  properties: {
    /** @type {!App} */
    app: Object,
  },

  listeners: {
    click: 'onClick_',
  },

  onClick_() {
    app_management.BrowserProxy.getInstance().handler.openNativeSettings(
        this.app.id);
    app_management.util.recordAppManagementUserAction(
        this.app.type, AppManagementUserAction.NativeSettingsOpened);
  },
});

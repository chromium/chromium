// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import {sendWithPromise, addSingletonGetter} from 'chrome://resources/js/cr.m.js';
// clang-format on

/**
 * These values should remain consistent with their C++ counterpart
 * (chrome/browser/ash/plugin_vm/plugin_vm_manager.h).
 * @enum {number}
 */
const PermissionType = {
  CAMERA: 0,
  MICROPHONE: 1,
};

/**
 * @typedef {{permissionType: !PermissionType,
 *            proposedValue: boolean}}
 */
let PermissionSetting;

/**
 * @fileoverview A helper object used by the Plugin VM section
 * to manage the Plugin VM.
 */
cr.define('settings', function() {
  /** @interface */
  /* #export */ class PluginVmBrowserProxy {
    /**
     * @return {!Promise<boolean>} Whether Plugin VM needs to be relaunched for
     *     permissions to take effect.
     */
    isRelaunchNeededForNewPermissions() {}

    /**
     * Relaunches Plugin VM.
     */
    relaunchPluginVm() {}
  }

  /** @implements {settings.PluginVmBrowserProxy} */
  /* #export */ class PluginVmBrowserProxyImpl {
    /** @override */
    isRelaunchNeededForNewPermissions() {
      return cr.sendWithPromise('isRelaunchNeededForNewPermissions');
    }

    /** @override */
    relaunchPluginVm() {
      chrome.send('relaunchPluginVm');
    }
  }

  // The singleton instance_ can be replaced with a test version of this wrapper
  // during testing.
  cr.addSingletonGetter(PluginVmBrowserProxyImpl);

  // #cr_define_end
  return {
    PluginVmBrowserProxy: PluginVmBrowserProxy,
    PluginVmBrowserProxyImpl: PluginVmBrowserProxyImpl,
  };
});

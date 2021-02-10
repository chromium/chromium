// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// #import 'chrome://resources/mojo/mojo/public/js/mojo_bindings_lite.js';
// #import 'chrome://resources/mojo/mojo/public/mojom/base/string16.mojom-lite.js';
// #import '../constants/routes.mojom-lite.js';
// #import '../constants/setting.mojom-lite.js';
// #import '../search/search_result_icon.mojom-lite.js';
// #import '../search/user_action_recorder.mojom-lite.js';
// #import '../search/search.mojom-lite.js';

/**
 * @fileoverview
 * Provides functions used for OS settings search.
 * Also provides a way to inject a test implementation for verifying
 * OS settings search.
 */
cr.define('settings', function() {
  /** @type {?chromeos.settings.mojom.SearchHandlerInterface} */
  let settingsSearchHandler = null;

  /**
   * @param {!chromeos.settings.mojom.SearchHandlerInterface}
   *     testSearchHandler A test search handler.
   */
  /* #export */ function setSearchHandlerForTesting(testSearchHandler) {
    settingsSearchHandler = testSearchHandler;
  }

  /**
   * @return {!chromeos.settings.mojom.SearchHandlerInterface} Search handler.
   */
  /* #export */ function getSearchHandler() {
    if (settingsSearchHandler) {
      return settingsSearchHandler;
    }

    settingsSearchHandler = chromeos.settings.mojom.SearchHandler.getRemote();
    return settingsSearchHandler;
  }

  // #cr_define_end
  return {
    setSearchHandlerForTesting,
    getSearchHandler,
  };
});

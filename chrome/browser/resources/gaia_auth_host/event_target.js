// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Work-around for
 * https://github.com/google/closure-compiler/issues/3143, such that WebUI code
 * can use the native EventTarget class.
 * This file is copied here since it is no longer used anywhere else in
 * Chromium. Code in this directory should be updated to use the up to date JS
 * module version of event_target.js in ui/webui/resources/js/cr/ instead, or
 * migrated to TypeScript such that the closure compiler workaround is no longer
 * necessary.
 */

cr.define('cr', function() {
  /**
   * @constructor
   * @implements {EventTarget}
   */
  /* #export */ const NativeEventTarget = self['EventTarget'];

  /** @override */ NativeEventTarget.prototype.addEventListener;
  /** @override */ NativeEventTarget.prototype.dispatchEvent;
  /** @override */ NativeEventTarget.prototype.removeEventListener;

  // #cr_define_end
  console.warn('crbug/1173575, non-JS module files deprecated.');
  return {EventTarget: NativeEventTarget};
});

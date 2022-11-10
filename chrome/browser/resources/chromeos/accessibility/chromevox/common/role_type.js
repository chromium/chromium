// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Augments chrome.automation.RoleType with abstract types for
 * ChromeVox.
 */

/** @enum {string} */
export const AbstractRole = {
  CONTAINER: 'abstractContainer',
  FORM_FIELD_CONTAINER: 'abstractFormFieldContainer',
  ITEM: 'abstractItem',
  LIST: 'abstractList',
  NAME_FROM_CONTENTS: 'abstractNameFromContents',
  RANGE: 'abstractRange',
  SPAN: 'abstractSpan',
};

/** @enum {string} */
export const CustomRole = {
  DEFAULT: 'default',
  NO_ROLE: 'noRole',
};

/** @typedef {!chrome.automation.RoleType|!AbstractRole|!CustomRole} */
export let ChromeVoxRole;

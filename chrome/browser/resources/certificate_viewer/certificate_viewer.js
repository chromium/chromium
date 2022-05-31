// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './strings.m.js';
import 'chrome://resources/cr_elements/cr_tab_box/cr_tab_box.js';
import 'chrome://resources/cr_elements/cr_tree/cr_tree.js';
import 'chrome://resources/cr_elements/cr_tree/cr_tree_item.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {sendWithPromise} from 'chrome://resources/js/cr.m.js';

// TODO (rbpotter): Remove these temporary definitions after migrating to TS.
/**
 * @typedef {{
 *   detail: { children: Object, payload: (Object|undefined) },
 *   expanded: boolean,
 *   add: function(CrTreeItemElement): void,
 * }}
 */
let CrTreeItemElement;

/**
 * @typedef {{
 *   detail: { children: Object, payload: Object },
 *   add: function(CrTreeItemElement): void,
 *   removeTreeItem: function(CrTreeItemElement): void,
 *   expanded: boolean,
 *   items: Array<CrTreeItemElement>,
 *   selectedItem: CrTreeItemElement,
 * }}
 */
let CrTreeElement;

/**
 * @typedef {{
 *   general: !Object,
 *   hierarchy: !Object,
 * }}
 */
let CertificateInfo;

/**
 * Initialize the certificate viewer dialog by wiring up the close button,
 * substituting in translated strings and requesting certificate details.
 */
function initialize() {
  const tabBox = document.querySelector('cr-tab-box');
  tabBox.hidden = false;

  const args = JSON.parse(chrome.getVariableValue('dialogArguments'));
  getCertificateInfo(/** @type {!CertificateInfo} */ (args));

  /**
   * Initialize the second tab's contents.
   * This is a 'oneShot' function, meaning it will only be invoked once,
   * no matter how many times it is called.  This is done for unit-testing
   * purposes in case a test needs to initialize the tab before the timer
   * fires.
   */
  const initializeDetailTab = oneShot(function() {
    const hierarchy =
        /** @type {CrTreeElement} */ (document.querySelector('#hierarchy'));
    assert(hierarchy);
    initializeTree(hierarchy, showCertificateFields);
    const certFields =
        /** @type {CrTreeElement} */ (document.querySelector('#cert-fields'));
    assert(certFields);
    initializeTree(certFields, showCertificateFieldValue);
    createCertificateHierarchy(args.hierarchy);
  });

  // The second tab's contents aren't visible on startup, so we can
  // shorten startup by not populating those controls until after we
  // have had a chance to draw the visible controls the first time.
  // The value of 200ms is quick enough that the user couldn't open the
  // tab in that time but long enough to allow the first tab to draw on
  // even the slowest machine.
  setTimeout(initializeDetailTab, 200);

  tabBox.addEventListener('selected-index-change', function f() {
    tabBox.removeEventListener('selected-index-change', f);
    initializeDetailTab();
  }, true);

  stripGtkAccessorKeys();

  document.querySelector('#export').onclick = exportCertificate;
}

/**
 * Decorate a function so that it can only be invoked once.
 */
function oneShot(fn) {
  let fired = false;
  return function() {
    if (fired) {
      return;
    }
    fired = true;
    fn();
  };
}

/**
 * Initialize a Tree object from a given element using the specified
 * change handler.
 * @param {!CrTreeElement} tree The HTMLElement to style as a tree.
 * @param {function()} handler Function to call when a node is selected.
 */
function initializeTree(tree, handler) {
  tree.detail = {payload: {}, children: {}};
  /** @type {HTMLElement} */ (tree).addEventListener('cr-tree-change', handler);
}

/**
 * The tab name strings in the languages file have accessor keys indicated
 * by a preceding & sign. Strip these out for now.
 * TODO(flackr) These accessor keys could be implemented with Javascript or
 *     translated strings could be added / modified to remove the & sign.
 */
function stripGtkAccessorKeys() {
  // Copy all the tab labels into an array.
  const tabs = document.querySelectorAll('div[slot=\'tab\']');
  const nodes = Array.prototype.slice.call(tabs, 0);
  const exportButton = document.querySelector('#export');
  nodes.push(exportButton);
  for (let i = 0; i < nodes.length; i++) {
    nodes[i].textContent = nodes[i].textContent.replace('&', '');
  }
}

/**
 * Expand all nodes of the given tree object.
 * @param {!CrTreeElement|CrTreeItemElement} tree The tree object to expand all
 *     nodes on.
 */
function revealTree(tree) {
  tree.expanded = true;
  for (const key in tree.detail.children) {
    revealTree(tree.detail.children[key]);
  }
}

/**
 * This function is called from certificate_viewer_ui.cc with the certificate
 * information. Display all returned information to the user.
 * @param {!CertificateInfo} certInfo Certificate information in named fields.
 */
function getCertificateInfo(certInfo) {
  for (const key in certInfo.general) {
    document.querySelector(`#${key}`).textContent = certInfo.general[key];
  }
}

/**
 * This function populates the certificate hierarchy.
 * @param {Object} hierarchy A dictionary containing the hierarchy.
 */
function createCertificateHierarchy(hierarchy) {
  const tree =
      /** @type {CrTreeElement} */ (document.querySelector('#hierarchy'));
  const root = constructTree(hierarchy[0]);
  tree.detail['children']['root'] = root;
  tree.add(root);

  // Select the last item in the hierarchy (really we have a list here - each
  // node has at most one child).  This will reveal the parent nodes and
  // populate the fields view.
  let last = root;
  while (last.detail['children'] && last.detail['children'][0]) {
    last = last.detail['children'][0];
  }
  tree.selectedItem = last;
}

/**
 * Constructs a TreeItem corresponding to the passed in tree
 * @param {Object} tree Dictionary describing the tree structure.
 * @return {!CrTreeItemElement} Tree node corresponding to the input dictionary.
 */
function constructTree(tree) {
  const treeItem =
      /** @type {CrTreeItemElement} */ (document.createElement('cr-tree-item'));
  treeItem.label = tree.label;
  treeItem.detail = {payload: tree.payload ? tree.payload : {}, children: {}};
  if (tree.children) {
    for (let i = 0; i < tree.children.length; i++) {
      const child = constructTree(tree.children[i]);
      treeItem.add(child);
      treeItem.detail.children[i] = child;
    }
  }
  return treeItem;
}

/**
 * Clear any previous certificate fields in the tree.
 */
function clearCertificateFields() {
  const treeItem =
      /** @type {!CrTreeElement} */ (document.querySelector('#cert-fields'));
  for (const key in treeItem.detail.children) {
    treeItem.removeTreeItem(treeItem.detail.children[key]);
    delete treeItem.detail.children[key];
  }
}

/**
 * Request certificate fields for the selected certificate in the hierarchy.
 */
function showCertificateFields() {
  clearCertificateFields();
  const hierarchy =
      /** @type {!CrTreeElement} */ (document.querySelector('#hierarchy'));
  const item = hierarchy.selectedItem;
  if (item && item.detail.payload.index !== undefined) {
    sendWithPromise('requestCertificateFields', item.detail.payload.index)
        .then(onCertificateFields);
  }
}

/**
 * Show the returned certificate fields for the selected certificate.
 * @param {Object} certFields A dictionary containing the fields tree
 *     structure.
 */
function onCertificateFields(certFields) {
  clearCertificateFields();
  const tree =
      /** @type {!CrTreeElement} */ (document.querySelector('#cert-fields'));
  const root = constructTree(certFields[0]);
  tree.detail.children['root'] = root;
  tree.add(root);
  revealTree(tree);
  // Ensure the list is scrolled to the top by selecting the first item.
  tree.selectedItem = tree.items[0];
  document.body.dispatchEvent(
      new CustomEvent('certificate-fields-updated-for-testing'));
}

/**
 * Show certificate field value for a selected certificate field.
 */
function showCertificateFieldValue() {
  const certFields =
      /** @type {CrTreeElement} */ (document.querySelector('#cert-fields'));
  const item = certFields.selectedItem;
  const fieldValue = document.querySelector('#cert-field-value');
  if (item && item.detail.payload.val) {
    fieldValue.textContent = item.detail.payload.val;
  } else {
    fieldValue.textContent = '';
  }
}

/**
 * Export the selected certificate.
 */
function exportCertificate() {
  const hierarchy =
      /** @type {CrTreeElement} */ (document.querySelector('#hierarchy'));
  const item = hierarchy.selectedItem;
  if (item && item.detail.payload.index !== undefined) {
    chrome.send('exportCertificate', [item.detail.payload.index]);
  }
}

document.addEventListener('DOMContentLoaded', initialize);

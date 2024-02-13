// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './strings.m.js';
import 'chrome://resources/cr_elements/cr_tab_box/cr_tab_box.js';
import 'chrome://resources/cr_elements/cr_tree/cr_tree.js';
import 'chrome://resources/cr_elements/cr_tree/cr_tree_item.js';

import type {CrTreeElement} from 'chrome://resources/cr_elements/cr_tree/cr_tree.js';
import type {CrTreeItemElement} from 'chrome://resources/cr_elements/cr_tree/cr_tree_item.js';
import {assert} from 'chrome://resources/js/assert.js';
import {sendWithPromise} from 'chrome://resources/js/cr.js';

interface TreeInfo {
  payload?: object;
  children?: TreeInfo[];
  label: string;
}

interface CertificateInfo {
  general: {[key: string]: string};
  hierarchy: TreeInfo[];
  isError: boolean;
}

export interface TreeItemDetail {
  payload: {
    val?: string,
    index?: number,
  };
  children: {[key: string|number]: CrTreeItemElement};
}

/**
 * Initialize the certificate viewer dialog by wiring up the close button,
 * substituting in translated strings and requesting certificate details.
 */
function initialize() {
  const tabBox = document.querySelector('cr-tab-box');
  assert(tabBox);
  tabBox.hidden = false;

  const args =
      JSON.parse(chrome.getVariableValue('dialogArguments')) as CertificateInfo;
  getCertificateInfo(args);

  /**
   * Initialize the second tab's contents.
   * This is a 'oneShot' function, meaning it will only be invoked once,
   * no matter how many times it is called.  This is done for unit-testing
   * purposes in case a test needs to initialize the tab before the timer
   * fires.
   */
  const initializeDetailTab = oneShot(function() {
    const hierarchy = document.querySelector<CrTreeElement>('#hierarchy');
    assert(hierarchy);
    initializeTree(hierarchy, showCertificateFields);
    const certFields = document.querySelector<CrTreeElement>('#cert-fields');
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

  const exportButton = document.querySelector<HTMLElement>('#export');
  assert(exportButton);
  exportButton.onclick = exportCertificate;
}

/**
 * Decorate a function so that it can only be invoked once.
 */
function oneShot(fn: () => void) {
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
 * tree The HTMLElement to style as a tree.
 * handler Function to call when a node is selected.
 */
function initializeTree(tree: CrTreeElement, handler: () => void) {
  tree.detail = {payload: {}, children: {}};
  tree.addEventListener('cr-tree-change', handler);
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
 * @param tree The tree object to expand all nodes on.
 */
function revealTree(tree: CrTreeElement|CrTreeItemElement) {
  tree.expanded = true;
  const detail = tree.detail as TreeItemDetail;
  for (const key in detail.children) {
    revealTree(detail.children[key]!);
  }
}

/**
 * This function is called from certificate_viewer_ui.cc with the certificate
 * information. Display all returned information to the user.
 * @param certInfo Certificate information in named fields.
 */
function getCertificateInfo(certInfo: CertificateInfo) {
  const generalError = document.querySelector<HTMLElement>('#general-error');
  assert(generalError);
  generalError.hidden = !certInfo.isError;
  const generalFields = document.querySelector<HTMLElement>('#general-fields');
  assert(generalFields);
  generalFields.hidden = certInfo.isError;
  for (const key in certInfo.general) {
    const el = document.querySelector<HTMLElement>(`#${key}`);
    assert(el);
    el.textContent = certInfo.general[key]!;
  }
}

/**
 * This function populates the certificate hierarchy.
 * @param hierarchy A dictionary containing the hierarchy.
 */
function createCertificateHierarchy(hierarchy: TreeInfo[]) {
  const tree = document.querySelector<CrTreeElement>('#hierarchy');
  assert(tree);
  const root = constructTree(hierarchy[0]!);
  (tree.detail as TreeItemDetail).children['root'] = root;
  tree.add(root);

  // Select the last item in the hierarchy (really we have a list here - each
  // node has at most one child).  This will reveal the parent nodes and
  // populate the fields view.
  let last: CrTreeItemElement = root;
  while ((last.detail as TreeItemDetail).children &&
         (last.detail as TreeItemDetail).children[0]) {
    last = (last.detail as TreeItemDetail).children[0]!;
  }
  tree.selectedItem = last;
}

/**
 * Constructs a TreeItem corresponding to the passed in tree
 * @param tree Dictionary describing the tree structure.
 * @return Tree node corresponding to the input dictionary.
 */
function constructTree(tree: TreeInfo): CrTreeItemElement {
  const treeItem = document.createElement('cr-tree-item');
  treeItem.label = tree.label;
  treeItem.detail = {payload: tree.payload ? tree.payload : {}, children: {}};
  treeItem.setExtraAriaLabel(
      (treeItem.detail as TreeItemDetail).payload.val || '');
  if (tree.children) {
    for (let i = 0; i < tree.children.length; i++) {
      const child = constructTree(tree.children[i]!);
      treeItem.add(child);
      (treeItem.detail as TreeItemDetail).children[i] = child;
    }
  }
  return treeItem;
}

/**
 * Clear any previous certificate fields in the tree.
 */
function clearCertificateFields() {
  const treeItem = document.querySelector<CrTreeElement>('#cert-fields');
  assert(treeItem);
  const detail = treeItem.detail as TreeItemDetail;
  for (const key in detail.children) {
    treeItem.removeTreeItem(detail.children[key]!);
    delete detail.children[key];
  }
}

/**
 * Request certificate fields for the selected certificate in the hierarchy.
 */
function showCertificateFields() {
  clearCertificateFields();
  const hierarchy = document.querySelector<CrTreeElement>('#hierarchy');
  assert(hierarchy);
  const item = hierarchy.selectedItem;
  if (item && (item.detail as TreeItemDetail).payload.index !== undefined) {
    sendWithPromise(
        'requestCertificateFields',
        (item.detail as TreeItemDetail).payload.index)
        .then(onCertificateFields);
  }
}

/**
 * Show the returned certificate fields for the selected certificate.
 * @param certFields A dictionary containing the fields tree structure.
 */
function onCertificateFields(certFields: TreeInfo[]) {
  clearCertificateFields();
  const tree = document.querySelector<CrTreeElement>('#cert-fields');
  assert(tree);
  const root = constructTree(certFields[0]!);
  (tree.detail as TreeItemDetail).children['root'] = root;
  tree.add(root);
  revealTree(tree);
  // Ensure the list is scrolled to the top by selecting the first item.
  tree.selectedItem = tree.items[0]!;
  document.body.dispatchEvent(
      new CustomEvent('certificate-fields-updated-for-testing'));
}

/**
 * Show certificate field value for a selected certificate field.
 */
function showCertificateFieldValue() {
  const certFields = document.querySelector<CrTreeElement>('#cert-fields');
  assert(certFields);
  const item = certFields.selectedItem;
  const fieldValue = document.querySelector<HTMLElement>('#cert-field-value');
  assert(fieldValue);
  if (item && (item.detail as TreeItemDetail).payload.val) {
    fieldValue.textContent = (item.detail as TreeItemDetail).payload.val || '';
  } else {
    fieldValue.textContent = '';
  }
}

/**
 * Export the selected certificate.
 */
function exportCertificate() {
  const hierarchy = document.querySelector<CrTreeElement>('#hierarchy');
  assert(hierarchy);
  const item = hierarchy.selectedItem;
  if (item && (item.detail as TreeItemDetail).payload.index !== undefined) {
    chrome.send(
        'exportCertificate', [(item.detail as TreeItemDetail).payload.index]);
  }
}

document.addEventListener('DOMContentLoaded', initialize);

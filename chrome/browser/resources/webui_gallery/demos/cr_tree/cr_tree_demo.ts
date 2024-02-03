// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_tree/cr_tree.js';
import '//resources/cr_elements/cr_tree/cr_tree_item.js';

import type {CrTreeElement} from '//resources/cr_elements/cr_tree/cr_tree.js';
import type {CrTreeItemElement} from '//resources/cr_elements/cr_tree/cr_tree_item.js';
import {assert} from '//resources/js/assert.js';
import {CustomElement} from '//resources/js/custom_element.js';

import {getTemplate} from './cr_tree_demo.html.js';

class CrTreeDemoElement extends CustomElement {
  static get is() {
    return 'cr-tree-demo';
  }

  static override get template() {
    return getTemplate();
  }

  private log_: HTMLElement|null = null;
  private tree_: CrTreeElement|null = null;

  connectedCallback() {
    this.log_ = this.shadowRoot!.querySelector('#log');
    this.tree_ = this.shadowRoot!.querySelector('cr-tree');
    assert(this.tree_);

    this.tree_.addEventListener(
        'cr-tree-change', () => this.addLogItem_('Selected item changed'));
    this.tree_.addEventListener(
        'cr-tree-item-collapse', () => this.addLogItem_('Collapsed'));
    this.tree_.addEventListener(
        'cr-tree-item-expand', () => this.addLogItem_('Expanded'));

    const iconCheckbox = this.shadowRoot!.querySelector('#iconVisibility');
    assert(iconCheckbox);
    iconCheckbox.addEventListener(
        'change', () => this.onIconVisibilityChanged_());

    const expandAllButton = this.shadowRoot!.querySelector('#expandAll');
    assert(expandAllButton);
    expandAllButton.addEventListener('click', () => this.expandAll_());

    const collapseAllButton = this.shadowRoot!.querySelector('#collapseAll');
    assert(collapseAllButton);
    collapseAllButton.addEventListener('click', () => this.collapseAll_());

    const removeItemButton = this.shadowRoot!.querySelector('#removeItem');
    assert(removeItemButton);
    removeItemButton.addEventListener('click', () => this.removeItem_());

    const addItemButton = this.shadowRoot!.querySelector('#addItem');
    assert(addItemButton);
    addItemButton.addEventListener('click', () => this.addItem_());

    window.setTimeout(() => {
      // Need to wait so that the cr-tree instance has been upgraded.
      this.populateTree_();
    });
  }

  private addItem_() {
    assert(this.tree_);
    const newItem = document.createElement('cr-tree-item');
    newItem.label = 'New item';

    if (this.tree_.selectedItem) {
      this.tree_.selectedItem.add(newItem);
    } else {
      this.tree_.add(newItem);
    }

    newItem.reveal();
  }

  private collapseAll_() {
    assert(this.tree_);
    function collapseItem(item: CrTreeElement|CrTreeItemElement) {
      item.expanded = false;
      item.items.forEach(child => collapseItem(child as CrTreeItemElement));
    }
    collapseItem(this.tree_);
  }

  private expandAll_() {
    assert(this.tree_);
    function expandItem(item: CrTreeElement|CrTreeItemElement) {
      item.expanded = true;
      item.items.forEach(child => expandItem(child as CrTreeItemElement));
    }
    expandItem(this.tree_);
  }

  private addLogItem_(log: string) {
    assert(this.log_);
    const div = document.createElement('div');
    div.innerText = log;
    this.log_.appendChild(div);
  }

  private onIconVisibilityChanged_() {
    const checkbox =
        this.shadowRoot!.querySelector<HTMLInputElement>('#iconVisibility');
    assert(checkbox);
    assert(this.tree_);
    this.tree_.setIconVisibility(checkbox.checked ? '' : 'hidden');
  }

  private populateTree_() {
    assert(this.tree_);

    const food = document.createElement('cr-tree-item');
    food.label = 'Food';
    this.tree_.add(food);
    this.tree_.selectedItem = food;

    const fruits = document.createElement('cr-tree-item');
    fruits.label = 'Fruits';
    food.add(fruits);

    ['Apple', 'Orange', 'Honeydew'].forEach(fruit => {
      const item = document.createElement('cr-tree-item');
      item.label = fruit;
      fruits.add(item);
    });

    const vegetables = document.createElement('cr-tree-item');
    vegetables.label = 'Vegetables';
    food.add(vegetables);

    ['Corn', 'Carrot', 'Broccoli', 'Cauliflower'].forEach(vegetable => {
      const item = document.createElement('cr-tree-item');
      item.label = vegetable;
      vegetables.add(item);
    });

    const drinks = document.createElement('cr-tree-item');
    drinks.label = 'Drinks';
    this.tree_.add(drinks);
  }

  private removeItem_() {
    assert(this.tree_);
    const newItem = document.createElement('cr-tree-item');
    newItem.label = 'New item';
    if (this.tree_.selectedItem) {
      this.tree_.selectedItem.parentItem!.removeTreeItem(
          this.tree_.selectedItem);
      this.tree_.selectedItem = this.tree_.items[0] || null;
    }
  }
}

export const tagName = CrTreeDemoElement.is;

customElements.define(CrTreeDemoElement.is, CrTreeDemoElement);
